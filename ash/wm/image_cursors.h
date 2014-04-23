// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_IMAGE_CURSORS_H_
#define ASH_WM_IMAGE_CURSORS_H_

#include "ash/ash_export.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "ui/base/cursor/cursor.h"
#include "ui/gfx/display.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {
class CursorLoader;
}

namespace ash {

// A utility class that provides cursors for NativeCursors for which we have
// image resources.
class ASH_EXPORT ImageCursors {
 public:
  ImageCursors();
  ~ImageCursors();

  // Returns the scale and rotation of the currently loaded cursor.
  float GetScale() const;
  gfx::Display::Rotation GetRotation() const;

  // Sets the display the cursors are loaded for. The device scale factor
  // determines the size of the image to load, and the rotation of the display
  // determines if the image and its host point has to be retated.
  // Returns true if the cursor image is reloaded.
  bool SetDisplay(const gfx::Display& display);

  // Sets the type of the mouse cursor icon.
  void SetCursorSet(ui::CursorSetType cursor_set);

  // Sets the platform cursor based on the native type of |cursor|.
  void SetPlatformCursor(gfx::NativeCursor* cursor);

 private:
  // Reloads the all loaded cursors in the cursor loader.
  void ReloadCursors();

  scoped_ptr<ui::CursorLoader> cursor_loader_;
  ui::CursorSetType cursor_set_;

  DISALLOW_COPY_AND_ASSIGN(ImageCursors);
};

}  // namespace ash

#endif  // ASH_WM_IMAGE_CURSORS_H_
