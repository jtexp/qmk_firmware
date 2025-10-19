#include QMK_KEYBOARD_H
#include "analog.h"
#include "qmk_midi.h"

// -------------------- Custom keycodes --------------------
enum custom_keycodes {
    LYR_NEXT = SAFE_RANGE, // tap to go to next layer (wraps to 0)
};

// ------------------------- Keymaps -------------------------
// clang-format off
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {

//      NUM->Cycle     /              *            -
//      7              8              9            +
//      4              5              6            ENCODER(CALC)
//      1              2              3            RET(ENTER)
//      0                                           .

  // Layer 0: Normal numpad
  [0] = LAYOUT(
    LYR_NEXT, KC_PSLS,   KC_PAST,   KC_PMNS,
    KC_P7,    KC_P8,     KC_P9,     KC_PPLS,
    KC_P4,    KC_P5,     KC_P6,     KC_CALC,
    KC_P1,    KC_P2,     KC_P3,     KC_PENT,
    KC_P0,                          KC_PDOT
  ),

  // Layer 1: RESTORED original (with RM_* keys) — NUM = LYR_NEXT
  [1] = LAYOUT(
    LYR_NEXT,   KC_PSLS,   KC_PAST,   KC_PMNS,
    KC_P7,      RM_VALU,   KC_P9,     KC_PPLS,
    RM_PREV,    KC_P5,     RM_NEXT,   KC_CALC,
    KC_P1,      RM_VALD,   KC_P3,     KC_PENT,
    RM_TOGG,                          QK_BOOT
  ),

  // Layer 2: Numpad keys as function keys (1–9 => F1–F9, 0 => F10)
  [2] = LAYOUT(
    LYR_NEXT, _______,   _______,   _______,
    KC_F7,    KC_F8,     KC_F9,     _______,
    KC_F4,    KC_F5,     KC_F6,     _______,
    KC_F1,    KC_F2,     KC_F3,     _______,
    KC_F10,                         _______
  ),

// +-----------+-----------+-----------+-----------+
// | NUM       | /         | *         | -         |
// | Cycle Lyr | Step Out  | Step Into | Step Over |
// |           | Shift+F8  | F7        | F8        |
// +-----------+-----------+-----------+-----------+
// | 7         | 8         | 9         | +         |
// | Toggle BP | Evaluate  | RunToCur  | Resume    |
// | Ctrl+F8   | Alt+F8    | Alt+F9    | F9        |
// +-----------+-----------+-----------+-----------+
// | 4         | 5         | 6         | Calc      |
// | Smart In  | Step Into | Step Out  | Breakpts  |
// | Shift+F7  | F7        | Shift+F8  | Ctrl+Sh+F8|
// +-----------+-----------+-----------+-----------+
// | 1         | 2         | 3         | Enter     |
// | Run       | Debug     | Rerun     | Stop      |
// | Shift+F10 | Shift+F9  | Ctrl+F5   | Ctrl+F2   |
// +-----------+-----------+-----------+-----------+
// | 0         |           |           | .         |
// | FocusDbg  |           |           | Evaluate  |
// | Alt+5     |           |           | Alt+F8    |
// +-----------+-----------+-----------+-----------+

  // Layer 3: IntelliJ Debug (Windows/Linux keymap)
  // / Step Out = Shift+F8 | * Step Into = F7 | - Step Over = F8 | + Resume = F9
  // 7 Toggle BP = Ctrl+F8 | 8 Evaluate = Alt+F8 | 9 RunToCursor = Alt+F9
  // 4 Smart Step Into = Shift+F7 | 5 Step Into = F7 | 6 Step Out = Shift+F8 | Calc View BPs = Ctrl+Shift+F8
  // 1 Run = Shift+F10 | 2 Debug = Shift+F9 | 3 Rerun = Ctrl+F5 | Enter Stop = Ctrl+F2
  // 0 Focus Debug Tool Window = Alt+5 | . Evaluate = Alt+F8
  [3] = LAYOUT(
    LYR_NEXT,       LSFT(KC_F8), KC_F7,        KC_F8,
    LCTL(KC_F8),    LALT(KC_F8), LALT(KC_F9),  KC_F9,
    LSFT(KC_F7),    KC_F7,       LSFT(KC_F8),  LCTL(LSFT(KC_F8)),
    LSFT(KC_F10),   LSFT(KC_F9), LCTL(KC_F5),  LCTL(KC_F2),
    LALT(KC_5),                               LALT(KC_F8)
  )
};
// clang-format on

// ---------------- Layer cycling behavior -----------------
bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    switch (keycode) {
        case LYR_NEXT:
            if (record->event.pressed) {
                const uint8_t num_layers = (uint8_t)(sizeof(keymaps) / sizeof(keymaps[0]));
                uint8_t cur = get_highest_layer(layer_state | default_layer_state);
                uint8_t nxt = (uint8_t)((cur + 1) % (num_layers ? num_layers : 1));
                layer_move(nxt);
            }
            return false; // don't send a keycode
    }
    return true;
}

// --------------------- RGB Matrix (per-key + sides) ----------------
#ifdef RGB_MATRIX_ENABLE

// ---- Color helpers ---------------------------------------------------------
typedef uint32_t Color; // 0xRRGGBB
#define RGB_HEX(c) (uint8_t)(((c) >> 16) & 0xFF), (uint8_t)(((c) >> 8) & 0xFF), (uint8_t)((c) & 0xFF)
#define COLOR_DIM(c, s) ( \
    ( (((((c) >> 16) & 0xFF) * (s)) / 255u) << 16 ) | \
    ( (((((c) >>  8) & 0xFF) * (s)) / 255u) <<  8 ) | \
      ((( (c)        & 0xFF) * (s)) / 255u)         \
)
static inline Color color_lerp(Color a, Color b, uint8_t t) {
    uint8_t ar = (a >> 16) & 0xFF, ag = (a >> 8) & 0xFF, ab = a & 0xFF;
    uint8_t br = (b >> 16) & 0xFF, bg = (b >> 8) & 0xFF, bb = b & 0xFF;
    uint8_t r = ar + ((int16_t)br - ar) * t / 255;
    uint8_t g = ag + ((int16_t)bg - ag) * t / 255;
    uint8_t b2 = ab + ((int16_t)bb - ab) * t / 255;
    return ((Color)r << 16) | ((Color)g << 8) | b2;
}

// ---- Named palette ---------------------------------------------------------
#define C_BLACK     0x000000
#define C_WHITE     0xFFFFFF
#define C_SILVER    0xC0C0C0
#define C_GRAY      0x7A7A7A
#define C_RED       0xFF3B30
#define C_ORANGE    0xFF9500
#define C_AMBER     0xFFC107
#define C_GOLD      0xFFD54F
#define C_GREEN     0x34C759
#define C_EMERALD   0x2DD4BF
#define C_BLUE      0x0A84FF
#define C_SKY       0x38BDF8
#define C_CYAN      0x00C8FF
#define C_MAGENTA   0xFF2D95
#define C_LAVENDER  0xC7A6FF
#define C_INDIGO    0x6366F1

// ---- Helpers ---------------------------------------------------------------
static inline int8_t led_index_for(uint8_t row, uint8_t col) {
    if (row >= MATRIX_ROWS || col >= MATRIX_COLS) return -1;
    return g_led_config.matrix_co[row][col]; // -1 (NO_LED) if no LED for that key
}

// ---- Per-layer key colors (hex) -------------------------------------------
// Layer 0 (base) — same as before
static const Color layer0_hex[MATRIX_ROWS][MATRIX_COLS] = {
    { C_ORANGE, C_CYAN,  C_CYAN,  C_RED },
    { C_GREEN,  C_GREEN, C_GREEN, C_ORANGE },
    { COLOR_DIM(C_GREEN, 180), COLOR_DIM(C_GREEN, 180), COLOR_DIM(C_GREEN, 180), C_SILVER },
    { C_BLUE,   C_BLUE,  C_BLUE,  C_WHITE },
    { C_SKY,    C_GRAY,  C_BLACK, C_BLACK }
};

// Layer 1 (restored fn/utility vibe; generic palette)
static const Color layer1_hex[MATRIX_ROWS][MATRIX_COLS] = {
    { C_MAGENTA, C_ORANGE,  C_ORANGE,  C_ORANGE },
    { C_ORANGE,  C_AMBER,   C_ORANGE,  C_AMBER  },
    { C_EMERALD, C_SILVER,  C_EMERALD, C_GOLD   },
    { C_SKY,     C_SKY,     C_SKY,     C_WHITE  },
    { C_CYAN,    C_SILVER,  C_BLACK,   C_BLACK  }
};

// Layer 2 (F-key layer) — gold/indigo motif
static const Color layer2_hex[MATRIX_ROWS][MATRIX_COLS] = {
    { C_GOLD,   C_INDIGO, C_INDIGO, C_INDIGO },
    { C_GOLD,   C_GOLD,   C_GOLD,   C_INDIGO },
    { C_GOLD,   C_GOLD,   C_GOLD,   C_INDIGO },
    { C_GOLD,   C_GOLD,   C_GOLD,   C_INDIGO },
    { C_GOLD,   C_INDIGO, C_BLACK,  C_BLACK  }
};

// Layer 3 (IntelliJ Debug) — uses the mnemonic colors we designed
static const Color layer3_hex[MATRIX_ROWS][MATRIX_COLS] = {
    // Row 0: NUM(cycle) gets overridden to case color; then /, *, -
    { C_ORANGE,  C_INDIGO, C_SKY,    C_BLUE  },
    // Row 1: 7, 8, 9, +
    { C_ORANGE,  C_MAGENTA, C_CYAN,  C_GREEN },
    // Row 2: 4, 5, 6, Calc
    { C_LAVENDER, C_SKY,    C_INDIGO, C_GOLD },
    // Row 3: 1, 2, 3, Enter
    { C_AMBER,    C_ORANGE, C_EMERALD, C_RED },
    // Row 4: 0, ., (pad), (pad)
    { C_SILVER,   C_MAGENTA, C_BLACK,  C_BLACK }
};

// ----- Side/underglow color mapping ----------------------------------------
// Define the case/side gradient per layer (left → right).
static const Color side_start[] = {
    C_SKY,      // layer 0 left
    C_ORANGE,   // layer 1 left (generic fn/utility)
    C_GOLD,     // layer 2 left (F-key layer)
    C_MAGENTA   // layer 3 left (IntelliJ debug)
};
static const Color side_end[] = {
    C_BLUE,     // layer 0 right
    C_SILVER,   // layer 1 right
    C_INDIGO,   // layer 2 right
    C_ORANGE    // layer 3 right
};

#define SIDE_LAYERS (uint8_t)(sizeof(side_start)/sizeof(side_start[0]))
static inline uint8_t side_idx_for_layer(uint8_t layer) {
    return (layer < SIDE_LAYERS) ? layer : (SIDE_LAYERS - 1);
}
static inline Color case_color_for_layer(uint8_t layer) {
    uint8_t i = side_idx_for_layer(layer);
    return color_lerp(side_start[i], side_end[i], 128); // midpoint of gradient
}

// ----- Side/underglow painting ---------------------------------------------
static void paint_side_underglow(uint8_t layer) {
    uint8_t i = side_idx_for_layer(layer);
    Color start = side_start[i];
    Color end   = side_end[i];

    int16_t min_x =  32767, max_x = -32768;
    uint8_t idxs[RGB_MATRIX_LED_COUNT];
    uint8_t n = 0;

    for (uint8_t led = 0; led < RGB_MATRIX_LED_COUNT; led++) {
        if (g_led_config.flags[led] & LED_FLAG_UNDERGLOW) {
            idxs[n++] = led;
            int16_t x = g_led_config.point[led].x;
            if (x < min_x) min_x = x;
            if (x > max_x) max_x = x;
        }
    }
    if (n == 0) return;

    int16_t span = (max_x > min_x) ? (max_x - min_x) : 1;
    for (uint8_t k = 0; k < n; k++) {
        uint8_t led = idxs[k];
        uint8_t t = (uint8_t)((uint16_t)(g_led_config.point[led].x - min_x) * 255u / span);
        Color col = color_lerp(start, end, t);
        rgb_matrix_set_color(led, RGB_HEX(col));
    }
}

// Paint per-key colors from a hex table (clears others first)
static void paint_keys_from_hex(const Color table[MATRIX_ROWS][MATRIX_COLS]) {
    rgb_matrix_set_color_all(0, 0, 0);
    for (uint8_t r = 0; r < MATRIX_ROWS; r++) {
        for (uint8_t c = 0; c < MATRIX_COLS; c++) {
            int8_t idx = led_index_for(r, c);
            if (idx >= 0) {
                Color col = table[r][c];
                rgb_matrix_set_color(idx, RGB_HEX(col));
            }
        }
    }
}

// Ensure NUM key (row 0, col 0) matches the current layer's case color
static void override_num_to_case_color(uint8_t layer) {
    int8_t idx = led_index_for(0, 0); // top-left key (LYR_NEXT)
    if (idx >= 0) {
        Color c = case_color_for_layer(layer);
        rgb_matrix_set_color(idx, RGB_HEX(c));
    }
}

// Called every frame; enforce per-layer colors (keys + side strips + NUM match)
bool rgb_matrix_indicators_user(void) {
    uint8_t layer = get_highest_layer(layer_state | default_layer_state);

    switch (layer) {
        case 3:
            paint_keys_from_hex(layer3_hex);
            paint_side_underglow(3);
            break;
        case 2:
            paint_keys_from_hex(layer2_hex);
            paint_side_underglow(2);
            break;
        case 1:
            paint_keys_from_hex(layer1_hex);
            paint_side_underglow(1);
            break;
        case 0:
        default:
            paint_keys_from_hex(layer0_hex);
            paint_side_underglow(0);
            break;
    }

    // Make NUM key match the case color for the active layer
    override_num_to_case_color(layer);

    return false; // let other indicators run if needed
}
#endif // RGB_MATRIX_ENABLE

// --------------- Potentiometer Slider, MIDI Control ---------------
uint8_t divisor = 0;

void slider(void) {
    if (divisor++) { // only run the slider function 1/256 times it's called
        return;
    }
    midi_send_cc(&midi_device, 2, 0x3E, 0x7F + (analogReadPin(SLIDER_PIN) >> 3));
}

void housekeeping_task_user(void) {
    slider();
}
