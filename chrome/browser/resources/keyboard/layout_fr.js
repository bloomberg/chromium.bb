// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is generaged by gen_keyboard_layouts.py

var KEYS_FR_POPUP_E = [
  [
    new PopupKey(C('\u0117', 'U+0117'), C('\u0116', 'U+0116'),
                 C('\u0117', 'U+0117'), C('\u0117', 'U+0117')),
    new PopupKey(C('\u0113', 'U+0113'), C('\u0112', 'U+0112'),
                 C('\u0113', 'U+0113'), C('\u0113', 'U+0113')),
  ],
  [
    new PopupKey(C('\u0119', 'U+0119'), C('\u0118', 'U+0118'),
                 C('\u0119', 'U+0119'), C('\u0119', 'U+0119')),
    new PopupKey(C('\u00ea', 'U+00EA'), C('\u00ca', 'U+00CA'),
                 C('\u00ea', 'U+00EA'), C('\u00ea', 'U+00EA')),
    new PopupKey(C('\u00e9', 'U+00E9'), C('\u00c9', 'U+00C9'),
                 C('\u00e9', 'U+00E9'), C('\u00e9', 'U+00E9')),
    new PopupKey(C('\u00e8', 'U+00E8'), C('\u00c8', 'U+00C8'),
                 C('\u00e8', 'U+00E8'), C('\u00e8', 'U+00E8')),
    new PopupKey(C('\u00eb', 'U+00EB'), C('\u00cb', 'U+00CB'),
                 C('\u00eb', 'U+00EB'), C('\u00eb', 'U+00EB')),
  ],
];

KEYBOARDS['fr_popup_e'] = {
  'definition': KEYS_FR_POPUP_E,
  'aspect': 1.5,
};

var KEYS_FR = [
  [
    new SvgKey('tab', 'Tab'),
    new Key(CP('a', 'a', 'fr_popup_a'), CP('A', 'A', 'fr_popup_a'), CP('1'),
            CP('~')),
    new Key(CP('z'), CP('Z'), CP('2'), CP('`')),
    new Key(CP('e', 'e', 'fr_popup_e'), CP('E', 'E', 'fr_popup_e'), CP('3'),
            CP('|')),
    new Key(CP('r'), CP('R'), CP('4'), CP('\u2022', 'U+2022')),
    new Key(CP('t'), CP('T'), CP('5'), CP('\u221a', 'U+221A')),
    new Key(CP('y', 'y', 'fr_popup_y'), CP('Y', 'Y', 'fr_popup_y'), CP('6'),
            CP('\u03c0', 'U+03C0')),
    new Key(CP('u', 'u', 'fr_popup_u'), CP('U', 'U', 'fr_popup_u'), CP('7'),
            CP('\u00f7', 'U+00F7')),
    new Key(CP('i', 'i', 'fr_popup_i'), CP('I', 'I', 'fr_popup_i'), CP('8'),
            CP('\u00d7', 'U+00D7')),
    new Key(CP('o', 'o', 'fr_popup_o'), CP('O', 'O', 'fr_popup_o'), CP('9'),
            CP('\u00a7', 'U+00A7')),
    new Key(CP('p'), CP('P'), CP('0'), CP('\u0394', 'U+0394')),
    new SvgKey('backspace', 'Backspace', true),
  ],
  [
    new SymbolKey(),
    new Key(CP('q'), CP('Q'), CP('#'), CP('\u00a3', 'U+00A3')),
    new Key(CP('s'), CP('S'), CP('$'), CP('\u00a2', 'U+00A2')),
    new Key(CP('d'), CP('D'), CP('%'), CP('\u20ac', 'U+20AC')),
    new Key(CP('f'), CP('F'), CP('&'), CP('\u00a5', 'U+00A5')),
    new Key(CP('g'), CP('G'), CP('*'), CP('^')),
    new Key(CP('h'), CP('H'), CP('-'), CP('\u00b0', 'U+00B0')),
    new Key(CP('j'), CP('J'), CP('+', 'Plus'), CP('\u00b1', 'U+00B1')),
    new Key(CP('k'), CP('K'), CP('('), CP('{', 'U+007B')),
    new Key(CP('l'), CP('L'), CP(')'), CP('}', 'U+007D')),
    new Key(CP('m'), CP('M'), null, null),
    new SvgKey('return', 'Enter'),
  ],
  [
    new ShiftKey('left_shift'),
    new Key(CP('w'), CP('W'), CP('<'), CP('\\')),
    new Key(CP('x'), CP('X'), CP('>'), CP('\u00a9', 'U+00A9')),
    new Key(CP('c', 'c', 'fr_popup_c'), CP('C', 'C', 'fr_popup_c'),
            CP('=', 'Equals'), CP('\u00ae', 'U+00AE')),
    new Key(CP('v'), CP('V'), CP(':'), CP('\u2122', 'U+2122')),
    new Key(CP('b'), CP('B'), CP(';'), CP('\u2105', 'U+2105')),
    new Key(CP('n'), CP('N'), CP(','), CP('[')),
    new Key(CP('\'', 'Apostrophe'), CP(':'), CP('.'), CP(']')),
    new Key(CP(','), CP('!'), CP('!'), CP('\u00a1', 'U+00A1')),
    new Key(CP('.'), CP('?'), CP('?'), CP('\u00bf', 'U+00BF')),
    new ShiftKey('right_shift'),
  ],
  [
    new SvgKey('mic', ' '),
    new DotComKey(),
    new SpecialKey('at', '@', '@'),
    new SpecialKey('space', ' ', 'Spacebar'),
    new SpecialKey('comma', ',', ','),
    new SpecialKey('period', '.', '.'),
    new HideKeyboardKey(),
  ],
];

KEYBOARDS['fr'] = {
  'definition': KEYS_FR,
  'aspect': 3.15,
};

var KEYS_FR_POPUP_A = [
  [
    new PopupKey(C('\u0101', 'U+0101'), C('\u0100', 'U+0100'),
                 C('\u0101', 'U+0101'), C('\u0101', 'U+0101')),
    new PopupKey(C('\u00e3', 'U+00E3'), C('\u00c3', 'U+00C3'),
                 C('\u00e3', 'U+00E3'), C('\u00e3', 'U+00E3')),
    new PopupKey(C('\u00e5', 'U+00E5'), C('\u00c5', 'U+00C5'),
                 C('\u00e5', 'U+00E5'), C('\u00e5', 'U+00E5')),
    new PopupKey(C('\u00aa', 'U+00AA'), C('\u00aa', 'U+00AA'),
                 C('\u00aa', 'U+00AA'), C('\u00aa', 'U+00AA')),
  ],
  [
    new PopupKey(C('\u00e4', 'U+00E4'), C('\u00c4', 'U+00C4'),
                 C('\u00e4', 'U+00E4'), C('\u00e4', 'U+00E4')),
    new PopupKey(C('\u00e6', 'U+00E6'), C('\u00c6', 'U+00C6'),
                 C('\u00e6', 'U+00E6'), C('\u00e6', 'U+00E6')),
    new PopupKey(C('\u00e0', 'U+00E0'), C('\u00c0', 'U+00C0'),
                 C('\u00e0', 'U+00E0'), C('\u00e0', 'U+00E0')),
    new PopupKey(C('\u00e2', 'U+00E2'), C('\u00c2', 'U+00C2'),
                 C('\u00e2', 'U+00E2'), C('\u00e2', 'U+00E2')),
    new PopupKey(C('\u00e1', 'U+00E1'), C('\u00c1', 'U+00C1'),
                 C('\u00e1', 'U+00E1'), C('\u00e1', 'U+00E1')),
  ],
];

KEYBOARDS['fr_popup_a'] = {
  'definition': KEYS_FR_POPUP_A,
  'aspect': 1.5,
};

var KEYS_FR_POPUP_C = [
  [
    new PopupKey(C('\u010d', 'U+010D'), C('\u010c', 'U+010C'),
                 C('\u010d', 'U+010D'), C('\u010d', 'U+010D')),
    new PopupKey(C('\u00e7', 'U+00E7'), C('\u00c7', 'U+00C7'),
                 C('\u00e7', 'U+00E7'), C('\u00e7', 'U+00E7')),
    new PopupKey(C('\u0107', 'U+0107'), C('\u0106', 'U+0106'),
                 C('\u0107', 'U+0107'), C('\u0107', 'U+0107')),
  ],
];

KEYBOARDS['fr_popup_c'] = {
  'definition': KEYS_FR_POPUP_C,
  'aspect': 1.8,
};

var KEYS_FR_POPUP_O = [
  [
    new PopupKey(C('\u014d', 'U+014D'), C('\u014c', 'U+014C'),
                 C('\u014d', 'U+014D'), C('\u014d', 'U+014D')),
    new PopupKey(C('\u00f5', 'U+00F5'), C('\u00d5', 'U+00D5'),
                 C('\u00f5', 'U+00F5'), C('\u00f5', 'U+00F5')),
    new PopupKey(C('\u00f8', 'U+00F8'), C('\u00d8', 'U+00D8'),
                 C('\u00f8', 'U+00F8'), C('\u00f8', 'U+00F8')),
    new PopupKey(C('\u00ba', 'U+00BA'), C('\u00ba', 'U+00BA'),
                 C('\u00ba', 'U+00BA'), C('\u00ba', 'U+00BA')),
  ],
  [
    new PopupKey(C('\u00f3', 'U+00F3'), C('\u00f3', 'U+00F3'),
                 C('\u00f3', 'U+00F3'), C('\u00f3', 'U+00F3')),
    new PopupKey(C('\u00f6', 'U+00F6'), C('\u00d6', 'U+00D6'),
                 C('\u00f6', 'U+00F6'), C('\u00f6', 'U+00F6')),
    new PopupKey(C('\u00f4', 'U+00F4'), C('\u00d4', 'U+00D4'),
                 C('\u00f4', 'U+00F4'), C('\u00f4', 'U+00F4')),
    new PopupKey(C('\u0153', 'U+0153'), C('\u0152', 'U+0152'),
                 C('\u0153', 'U+0153'), C('\u0153', 'U+0153')),
    new PopupKey(C('\u00f2', 'U+00F2'), C('\u00d2', 'U+00D2'),
                 C('\u00f2', 'U+00F2'), C('\u00f2', 'U+00F2')),
  ],
];

KEYBOARDS['fr_popup_o'] = {
  'definition': KEYS_FR_POPUP_O,
  'aspect': 1.5,
};

var KEYS_FR_POPUP_I = [
  [
    new PopupKey(C('\u012b', 'U+012B'), C('\u012a', 'U+012A'),
                 C('\u012b', 'U+012B'), C('\u012b', 'U+012B')),
  ],
  [
    new PopupKey(C('\u012f', 'U+012F'), C('\u012e', 'U+012E'),
                 C('\u012f', 'U+012F'), C('\u012f', 'U+012F')),
    new PopupKey(C('\u00ec', 'U+00EC'), C('\u00cc', 'U+00CC'),
                 C('\u00ec', 'U+00EC'), C('\u00ec', 'U+00EC')),
    new PopupKey(C('\u00ee', 'U+00EE'), C('\u00ce', 'U+00CE'),
                 C('\u00ee', 'U+00EE'), C('\u00ee', 'U+00EE')),
    new PopupKey(C('\u00ef', 'U+00EF'), C('\u00cf', 'U+00CF'),
                 C('\u00ef', 'U+00EF'), C('\u00ef', 'U+00EF')),
    new PopupKey(C('\u00ed', 'U+00ED'), C('\u00cd', 'U+00CD'),
                 C('\u00ed', 'U+00ED'), C('\u00ed', 'U+00ED')),
  ],
];

KEYBOARDS['fr_popup_i'] = {
  'definition': KEYS_FR_POPUP_I,
  'aspect': 1.5,
};

var KEYS_FR_POPUP_U = [
  [
    new PopupKey(C('\u016b', 'U+016B'), C('\u016a', 'U+016A'),
                 C('\u016b', 'U+016B'), C('\u016b', 'U+016B')),
    new PopupKey(C('\u00fc', 'U+00FC'), C('\u00dc', 'U+00DC'),
                 C('\u00fc', 'U+00FC'), C('\u00fc', 'U+00FC')),
    new PopupKey(C('\u00fb', 'U+00FB'), C('\u00db', 'U+00DB'),
                 C('\u00fb', 'U+00FB'), C('\u00fb', 'U+00FB')),
    new PopupKey(C('\u00f9', 'U+00F9'), C('\u00d9', 'U+00D9'),
                 C('\u00f9', 'U+00F9'), C('\u00f9', 'U+00F9')),
    new PopupKey(C('\u00fa', 'U+00FA'), C('\u00da', 'U+00DA'),
                 C('\u00fa', 'U+00FA'), C('\u00fa', 'U+00FA')),
  ],
];

KEYBOARDS['fr_popup_u'] = {
  'definition': KEYS_FR_POPUP_U,
  'aspect': 3,
};

var KEYS_FR_POPUP_Y = [
  [
    new PopupKey(C('\u00ff', 'U+00FF'), C('\u0178', 'U+0178'),
                 C('\u00ff', 'U+00FF'), C('\u00ff', 'U+00FF')),
  ],
];

KEYBOARDS['fr_popup_y'] = {
  'definition': KEYS_FR_POPUP_Y,
  'aspect': 0.6,
};
