// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EXAMPLES_KEYBOARD_KEYBOARD_DELEGATE_H_
#define MOJO_EXAMPLES_KEYBOARD_KEYBOARD_DELEGATE_H_

#include "base/basictypes.h"

namespace mojo {
namespace examples {

class KeyboardDelegate {
 public:
  KeyboardDelegate() {}

  virtual void OnKeyPressed(int key_code, int event_flags) = 0;

 protected:
  virtual ~KeyboardDelegate() {}
};

}  // namespace examples
}  // namespace mojo

#endif  // MOJO_EXAMPLES_KEYBOARD_KEYBOARD_DELEGATE_H_
