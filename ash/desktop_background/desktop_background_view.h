// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DESKTOP_BACKGROUND_DESKTOP_BACKGROUND_VIEW_H_
#define ASH_DESKTOP_BACKGROUND_DESKTOP_BACKGROUND_VIEW_H_
#pragma once

#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {
namespace internal {

class DesktopBackgroundView : public views::WidgetDelegateView {
 public:
  DesktopBackgroundView(const SkBitmap& wallpaper);
  virtual ~DesktopBackgroundView();

  // TODO(bshe): Remove this function once issue 117244 is fixed. It is
  // currently used in DesktopBackgroundController::
  // OnDesktopBackgroundChanged.
  void SetWallpaper(const SkBitmap& wallpaper);

 private:
  // Overridden from views::View:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual bool OnMousePressed(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const views::MouseEvent& event) OVERRIDE;

  SkBitmap wallpaper_;

  DISALLOW_COPY_AND_ASSIGN(DesktopBackgroundView);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_DESKTOP_BACKGROUND_DESKTOP_BACKGROUND_VIEW_H_
