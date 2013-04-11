// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_IMAGE_CURSORS_H_
#define ASH_WM_IMAGE_CURSORS_H_

#include "ash/ash_export.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {
class Display;
}

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

  // Returns the display the cursors are loaded for. The display must
  // be set by SetDisplay before using this.
  gfx::Display GetDisplay() const;

  // Sets the display the cursors are loaded for. The device scale factor
  // determines the size of the image to load, and the rotation of the display
  // determines if the image and its host point has to be retated.
  // Returns true if the cursor image is reloaded.
  bool SetDisplay(const gfx::Display& display);

  // Sets the platform cursor based on the native type of |cursor|.
  void SetPlatformCursor(gfx::NativeCursor* cursor);

  // Sets the cursor resource module name for non system cursors.
  void SetCursorResourceModule(const base::string16& module_name);

 private:
  scoped_ptr<ui::CursorLoader> cursor_loader_;

  DISALLOW_COPY_AND_ASSIGN(ImageCursors);
};

}  // namespace ash

#endif  // ASH_WM_IMAGE_CURSORS_H_
