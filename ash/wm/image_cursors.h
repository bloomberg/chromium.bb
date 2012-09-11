// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_IMAGE_CURSORS_H_
#define ASH_WM_IMAGE_CURSORS_H_

#include "ash/ash_export.h"
#include "base/memory/scoped_ptr.h"
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

  // Returns the device scale factor of cursors.
  float GetDeviceScaleFactor() const;

  // Sets the device scale factor of the cursors with |device_scale_factor| and
  // reloads the cursor images if necessary.
  void SetDeviceScaleFactor(float device_scale_factor);

  // Sets the platform cursor based on the native type of |cursor|.
  void SetPlatformCursor(gfx::NativeCursor* cursor);

 private:
  scoped_ptr<ui::CursorLoader> cursor_loader_;

  DISALLOW_COPY_AND_ASSIGN(ImageCursors);
};

}  // namespace ash

#endif  // ASH_WM_IMAGE_CURSORS_H_
