// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DESKTOP_BACKGROUND_DESKTOP_BACKGROUND_VIEW_H_
#define ASH_DESKTOP_BACKGROUND_DESKTOP_BACKGROUND_VIEW_H_
#pragma once

#include "ash/desktop_background/desktop_background_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {
namespace internal {

class DesktopBackgroundView : public views::WidgetDelegateView {
 public:
  DesktopBackgroundView(const SkBitmap& wallpaper,
                        WallpaperLayout wallpaper_layout);
  virtual ~DesktopBackgroundView();

 private:
  // Overridden from views::View:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual bool OnMousePressed(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const views::MouseEvent& event) OVERRIDE;

  SkBitmap wallpaper_;
  WallpaperLayout wallpaper_layout_;

  DISALLOW_COPY_AND_ASSIGN(DesktopBackgroundView);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_DESKTOP_BACKGROUND_DESKTOP_BACKGROUND_VIEW_H_
