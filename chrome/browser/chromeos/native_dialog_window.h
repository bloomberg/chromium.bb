// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NATIVE_DIALOG_WINDOW_H_
#define CHROME_BROWSER_CHROMEOS_NATIVE_DIALOG_WINDOW_H_
#pragma once

#include "ui/gfx/native_widget_types.h"

namespace gfx {
class Rect;
class Size;
}  // namespace gfx

namespace chromeos {

// Flags for ShowNativeDialog.
enum NativeDialogFlags {
  DIALOG_FLAG_DEFAULT    = 0x00,  // Default non-resizeable, non-modal dialog.
  DIALOG_FLAG_RESIZEABLE = 0x01,  // For resizeable dialog.
  DIALOG_FLAG_MODAL      = 0x02,  // For modal dialog.
};

// Shows a |native_dialog| hosted in a views::Window. |flags| are combinations
// of the NativeDialogFlags. |size| is a default size. Zero width/height of
// |size| means let gtk choose a proper size for that dimension. |min_size| is
// the minimum size of the final host Window.
void ShowNativeDialog(gfx::NativeWindow parent,
                      gfx::NativeView native_dialog,
                      int flags,
                      const gfx::Size& size,
                      const gfx::Size& min_size);

// Gets the container window of the given |native_dialog|.
gfx::NativeWindow GetNativeDialogWindow(gfx::NativeView native_dialog);

// Gets the bounds of the contained dialog content.
gfx::Rect GetNativeDialogContentsBounds(gfx::NativeView native_dialog);

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NATIVE_DIALOG_WINDOW_H_
