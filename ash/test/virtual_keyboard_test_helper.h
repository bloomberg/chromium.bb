// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_VIRTUAL_KEYBOARD_TEST_HELPER_H_
#define ASH_TEST_VIRTUAL_KEYBOARD_TEST_HELPER_H_

namespace ash {
namespace test {

class VirtualKeyboardTestHelper {
 public:
  // Mocks the input device configuration so that the on-screen keyboard is
  // suppressed.
  static void SuppressKeyboard();
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_VIRTUAL_KEYBOARD_TEST_HELPER_H_
