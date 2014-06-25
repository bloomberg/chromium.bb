// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/examples/keyboard/keys.h"

#include "base/macros.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace mojo {
namespace examples {
namespace {

const Key kQWERTYKeysRow1[] =
{
  { ui::VKEY_Q, 1, 0, 'q' },
  { ui::VKEY_W, 1, 0, 'w' },
  { ui::VKEY_E, 1, 0, 'e' },
  { ui::VKEY_R, 1, 0, 'r' },
  { ui::VKEY_T, 1, 0, 't' },
  { ui::VKEY_Y, 1, 0, 'y' },
  { ui::VKEY_U, 1, 0, 'u' },
  { ui::VKEY_I, 1, 0, 'i' },
  { ui::VKEY_O, 1, 0, 'o' },
  { ui::VKEY_P, 1, 0, 'p' },
};

const Key kQWERTYKeysRow2[] =
{
  { ui::VKEY_A, 1, 0, 'a' },
  { ui::VKEY_S, 1, 0, 's' },
  { ui::VKEY_D, 1, 0, 'd' },
  { ui::VKEY_F, 1, 0, 'f' },
  { ui::VKEY_G, 1, 0, 'g' },
  { ui::VKEY_H, 1, 0, 'h' },
  { ui::VKEY_J, 1, 0, 'j' },
  { ui::VKEY_K, 1, 0, 'k' },
  { ui::VKEY_L, 1, 0, 'l' },
};

const Key kQWERTYKeysRow3[] =
{
  { SPECIAL_KEY_SHIFT, 1.5, 0, 0 },
  { ui::VKEY_Z, 1, 0, 'z' },
  { ui::VKEY_X, 1, 0, 'x' },
  { ui::VKEY_C, 1, 0, 'c' },
  { ui::VKEY_V, 1, 0, 'v' },
  { ui::VKEY_B, 1, 0, 'b' },
  { ui::VKEY_N, 1, 0, 'n' },
  { ui::VKEY_M, 1, 0, 'm' },
  { ui::VKEY_BACK, 1.5, 0, 0 },
};

const Key kQWERTYKeysRow4[] =
{
  { SPECIAL_KEY_NUMERIC, 1.5, 0, 0 },
  { ui::VKEY_DIVIDE, 1, 0, '/' },
  { ui::VKEY_SPACE, 5, 0, ' ' },
  { ui::VKEY_DECIMAL, 1, 0, '.' },
  { ui::VKEY_RETURN, 1.5, 0, 0 },
};

const Row kQWERTYRow1 = {
  0,
  kQWERTYKeysRow1,
  arraysize(kQWERTYKeysRow1),
};

const Row kQWERTYRow2 = {
  .5,
  kQWERTYKeysRow2,
  arraysize(kQWERTYKeysRow2),
};

const Row kQWERTYRow3 = {
  0,
  kQWERTYKeysRow3,
  arraysize(kQWERTYKeysRow3),
};

const Row kQWERTYRow4 = {
  0,
  kQWERTYKeysRow4,
  arraysize(kQWERTYKeysRow4),
};

const Key kNumericKeysRow1[] =
{
  { ui::VKEY_1, 1, 0, 0 },
  { ui::VKEY_2, 1, 0, 0 },
  { ui::VKEY_3, 1, 0, 0 },
  { ui::VKEY_4, 1, 0, 0 },
  { ui::VKEY_5, 1, 0, 0 },
  { ui::VKEY_6, 1, 0, 0 },
  { ui::VKEY_7, 1, 0, 0 },
  { ui::VKEY_8, 1, 0, 0 },
  { ui::VKEY_9, 1, 0, 0 },
  { ui::VKEY_0, 1, 0, 0 },
};

const Key kNumericKeysRow2[] =
{
  // @#$%&-+()
  { ui::VKEY_2, 1, ui::EF_SHIFT_DOWN, '@' },
  { ui::VKEY_3, 1, ui::EF_SHIFT_DOWN, '#' },
  { ui::VKEY_4, 1, ui::EF_SHIFT_DOWN, '$' },
  { ui::VKEY_5, 1, ui::EF_SHIFT_DOWN, '%' },
  { ui::VKEY_7, 1, ui::EF_SHIFT_DOWN, '&' },
  { ui::VKEY_SUBTRACT, 1, 0, '-' },
  { ui::VKEY_ADD, 1, 0, '+' },
  { ui::VKEY_9, 1, ui::EF_SHIFT_DOWN, '(' },
  { ui::VKEY_0, 1, ui::EF_SHIFT_DOWN, ')' },
};

const Key kNumericKeysRow3[] =
{
  // *"':;!? backspace
  { ui::VKEY_MULTIPLY, 1, 0, '*' },
  { ui::VKEY_OEM_7, 1, ui::EF_SHIFT_DOWN, '"' },
  { ui::VKEY_OEM_7, 1, 0, '\'' },
  { ui::VKEY_OEM_1, 1, ui::EF_SHIFT_DOWN, ':' },
  { ui::VKEY_OEM_1, 1, 0, ';' },
  { ui::VKEY_1, 1, ui::EF_SHIFT_DOWN, '!' },
  { ui::VKEY_OEM_2, 1, ui::EF_SHIFT_DOWN, '?' },
  { ui::VKEY_BACK, 1.5, 0, 0 },
};

const Key kNumericKeysRow4[] =
{
  // ABC _ / space (3) ,.enter
  { SPECIAL_KEY_ALPHA, 1.5, 0, 0 },
  { ui::VKEY_OEM_MINUS, 1, ui::EF_SHIFT_DOWN, '_' },
  { ui::VKEY_OEM_2, 1, 0, '/' },
  { ui::VKEY_SPACE, 3, 0, ' ' },
  { ui::VKEY_OEM_COMMA, 1, 0, ',' },
  { ui::VKEY_OEM_PERIOD, 1, 0, '.' },
  { ui::VKEY_RETURN, 1.5, 0, 0 },
};

const Row kNumericRow1 = {
  0,
  kNumericKeysRow1,
  arraysize(kNumericKeysRow1),
};

const Row kNumericRow2 = {
  .5,
  kNumericKeysRow2,
  arraysize(kNumericKeysRow2),
};

const Row kNumericRow3 = {
  1.5,
  kNumericKeysRow3,
  arraysize(kNumericKeysRow3),
};

const Row kNumericRow4 = {
  0,
  kNumericKeysRow4,
  arraysize(kNumericKeysRow4),
};

}  // namespace

std::vector<const Row*> GetQWERTYRows() {
  std::vector<const Row*> rows;
  rows.push_back(&kQWERTYRow1);
  rows.push_back(&kQWERTYRow2);
  rows.push_back(&kQWERTYRow3);
  rows.push_back(&kQWERTYRow4);
  return rows;
}

std::vector<const Row*> GetNumericRows() {
  std::vector<const Row*> rows;
  rows.push_back(&kNumericRow1);
  rows.push_back(&kNumericRow2);
  rows.push_back(&kNumericRow3);
  rows.push_back(&kNumericRow4);
  return rows;
}

}  // namespace examples
}  // namespace mojo
