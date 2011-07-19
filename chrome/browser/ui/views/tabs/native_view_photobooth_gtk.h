// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_NATIVE_VIEW_PHOTOBOOTH_GTK_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_NATIVE_VIEW_PHOTOBOOTH_GTK_H_
#pragma once

#include "base/compiler_specific.h"
#include "chrome/browser/ui/views/tabs/native_view_photobooth.h"

class NativeViewPhotoboothGtk : public NativeViewPhotobooth {
 public:
  explicit NativeViewPhotoboothGtk(gfx::NativeView new_view);

  // Destroys the photo booth window.
  virtual ~NativeViewPhotoboothGtk();

  // Replaces the view in the photo booth with the specified one.
  virtual void Replace(gfx::NativeView new_view) OVERRIDE;

  // Paints the current display image of the window into |canvas|, clipped to
  // |target_bounds|.
  virtual void PaintScreenshotIntoCanvas(
      gfx::Canvas* canvas,
      const gfx::Rect& target_bounds) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(NativeViewPhotoboothGtk);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_NATIVE_VIEW_PHOTOBOOTH_GTK_H_
