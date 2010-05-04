// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NATIVE_DIALOG_WINDOW_H_
#define CHROME_BROWSER_CHROMEOS_NATIVE_DIALOG_WINDOW_H_

#include "gfx/native_widget_types.h"

namespace gfx {
class Size;
}  // namespace gfx

namespace chromeos {

// Shows a native dialog in views::Window.
void ShowNativeDialog(gfx::NativeWindow parent,
                      gfx::NativeView native_dialog,
                      const gfx::Size& size,
                      bool resizeable);

// Gets the container window of the given |native_dialog|.
gfx::NativeWindow GetNativeDialogWindow(gfx::NativeView native_dialog);

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NATIVE_DIALOG_WINDOW_H_
