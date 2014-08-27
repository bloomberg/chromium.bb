// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_VIEWS_INPUT_METHOD_MOJO_LINUX_H_
#define MOJO_VIEWS_INPUT_METHOD_MOJO_LINUX_H_

#include "ui/base/ime/input_method_auralinux.h"
#include "ui/base/ime/input_method_delegate.h"

namespace mojo {

// An input method for linux that does absolutely no translation.
//
// The current InputMethodMinimal makes assumptions that a system will only
// input/output keydown/keyup events; it assumes that things don't work like
// Windows does. When it gets a keydown event, it then tries to insert a
// character at the same time.
//
// However, we're standardizing on Windows' WM_CHAR style events. This tries to
// follow InputMethodWin::DispatchKeyEvent() instead, because PlatformViewX11
// now synthesizes a character events so that we have one behaviour across our
// platforms.
class InputMethodMojoLinux : public ui::InputMethodAuraLinux {
 public:
  explicit InputMethodMojoLinux(ui::internal::InputMethodDelegate* delegate);
  virtual ~InputMethodMojoLinux();

  // Overriden from ui::InputMethodAuraLinux:
  virtual bool DispatchKeyEvent(const ui::KeyEvent& event) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(InputMethodMojoLinux);
};

}  // namespace mojo

#endif  // MOJO_VIEWS_INPUT_METHOD_MOJO_LINUX_H_
