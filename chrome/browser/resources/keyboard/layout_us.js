// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A simple English virtual keyboard implementation.
 */

/**
 * All keys for the rows of the keyboard.
 * NOTE: every row below should have an aspect of 12.6.
 * @type {Array.<Array.<BaseKey>>}
 */
var KEYS_US = [
  [
    new SvgKey('tab', 'Tab'),
    new Key(CP('q'), CP('Q'), CP('1'), CP('`')),
    new Key(CP('w'), CP('W'), CP('2'), CP('~')),
    new Key(CP('e'), CP('E'), CP('3'), CP('<', 'LessThan')),
    new Key(CP('r'), CP('R'), CP('4'), CP('>', 'GreaterThan')),
    new Key(CP('t'), CP('T'), CP('5'), CP('[')),
    new Key(CP('y'), CP('Y'), CP('6'), CP(']')),
    new Key(CP('u'), CP('U'), CP('7'), CP('{')),
    new Key(CP('i'), CP('I'), CP('8'), CP('}')),
    new Key(CP('o'), CP('O'), CP('9'), CP('\'')),
    new Key(CP('p'), CP('P'), CP('0'), CP('|')),
    new SvgKey('backspace', 'Backspace', true /* repeat */)
  ],
  [
    new SymbolKey(),
    new Key(CP('a'), CP('A'), CP('!'), CP('+')),
    new Key(CP('s'), CP('S'), CP('@'), CP('=')),
    new Key(CP('d'), CP('D'), CP('#'), CP(' ')),
    new Key(CP('f'), CP('F'), CP('$'), CP(' ')),
    new Key(CP('g'), CP('G'), CP('%'), CP(' ')),
    new Key(CP('h'), CP('H'), CP('^'), CP(' ')),
    new Key(CP('j'), CP('J'), CP('&', 'Ampersand'), CP(' ')),
    new Key(CP('k'), CP('K'), CP('*'), CP('#')),
    new Key(CP('l'), CP('L'), CP('('), CP(' ')),
    new Key(CP('\''), CP('\''), CP(')'), CP(' ')),
    new SvgKey('return', 'Enter')
  ],
  [
    new ShiftKey('left_shift'),
    new Key(CP('z'), CP('Z'), CP('/'), CP(' ')),
    new Key(CP('x'), CP('X'), CP('-'), CP(' ')),
    new Key(CP('c'), CP('C'), CP('\''), CP(' ')),
    new Key(CP('v'), CP('V'), CP('"'), CP(' ')),
    new Key(CP('b'), CP('B'), CP(':'), CP('.')),
    new Key(CP('n'), CP('N'), CP(';'), CP(' ')),
    new Key(CP('m'), CP('M'), CP('_'), CP(' ')),
    new Key(CP('!'), CP('!'), CP('{'), CP(' ')),
    new Key(CP('?'), CP('?'), CP('}'), CP(' ')),
    new Key(CP('/'), CP('/'), CP('\\'), CP(' ')),
    new ShiftKey()
  ],
  [
    new SvgKey('mic', ''),
    new DotComKey(),
    new SpecialKey('at', '@', '@'),
    // TODO(bryeung): the spacebar needs to be a little bit more stretchy,
    // since this row has only 7 keys (as opposed to 12), the truncation
    // can cause it to not be wide enough.
    new SpecialKey('space', ' ', 'Spacebar'),
    new SpecialKey('comma', ',', ','),
    new SpecialKey('period', '.', '.'),
    new HideKeyboardKey()
  ]
];

// Add layout to KEYBOARDS, which is defined in common.js
KEYBOARDS['us'] = {
  "definition": KEYS_US,
  "aspect": 3.15,
}
