// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is generaged by gen_keyboard_layouts.py

var KEYS_FR = [
  [
    new SvgKey('tab', 'Tab'),
    new Key(C('a'), C('A'), C('1'), C('~')),
    new Key(C('z'), C('Z'), C('2'), C('`')),
    new Key(C('e'), C('E'), C('3'), C('|')),
    new Key(C('r'), C('R'), C('4'), new Character('\u2022', 'U+2022')),
    new Key(C('t'), C('T'), C('5'), new Character('\u221a', 'U+221A')),
    new Key(C('y'), C('Y'), C('6'), new Character('\u03c0', 'U+03C0')),
    new Key(C('u'), C('U'), C('7'), new Character('\u00f7', 'U+00F7')),
    new Key(C('i'), C('I'), C('8'), new Character('\u00d7', 'U+00D7')),
    new Key(C('o'), C('O'), C('9'), new Character('\u00a7', 'U+00A7')),
    new Key(C('p'), C('P'), C('0'), new Character('\u0394', 'U+0394')),
    new SvgKey('backspace', 'Backspace', true),
  ],
  [
    new SymbolKey(),
    new Key(C('q'), C('Q'), C('#'), new Character('\u00a3', 'U+00A3')),
    new Key(C('s'), C('S'), C('$'), new Character('\u00a2', 'U+00A2')),
    new Key(C('d'), C('D'), C('%'), new Character('\u20ac', 'U+20AC')),
    new Key(C('f'), C('F'), C('&'), new Character('\u00a5', 'U+00A5')),
    new Key(C('g'), C('G'), C('*'), C('^')),
    new Key(C('h'), C('H'), C('-'), new Character('\u00b0', 'U+00B0')),
    new Key(C('j'), C('J'), new Character('+', 'Plus'),
            new Character('\u00b1', 'U+00B1')),
    new Key(C('k'), C('K'), C('('), new Character('{', 'U+007B')),
    new Key(C('l'), C('L'), C(')'), new Character('}', 'U+007D')),
    new Key(C('m'), C('M'), null, null),
    new SvgKey('return', 'Enter'),
  ],
  [
    new ShiftKey('left_shift'),
    new Key(C('w'), C('W'), C('<'), C('\\')),
    new Key(C('x'), C('X'), C('>'), new Character('\u00a9', 'U+00A9')),
    new Key(C('c'), C('C'), new Character('=', 'Equals'),
            new Character('\u00ae', 'U+00AE')),
    new Key(C('v'), C('V'), C(':'), new Character('\u2122', 'U+2122')),
    new Key(C('b'), C('B'), C(';'), new Character('\u2105', 'U+2105')),
    new Key(C('n'), C('N'), C(','), C('[')),
    new Key(new Character('\'', 'Apostrophe'), C(':'), C('.'), C(']')),
    new Key(C(','), C('!'), C('!'), new Character('\u00a1', 'U+00A1')),
    new Key(C('.'), C('?'), C('?'), new Character('\u00bf', 'U+00BF')),
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

// Add layout to KEYBOARDS, which is defined in common.js
KEYBOARDS['fr'] = {
  "definition": KEYS_FR,
  "aspect": 3.15,
};
