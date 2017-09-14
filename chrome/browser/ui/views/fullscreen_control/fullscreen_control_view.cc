// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/fullscreen_control/fullscreen_control_view.h"

#include <memory>

#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/layout/box_layout.h"

namespace {

constexpr int kCircleButtonDiameter = 48;

// Spacing applied to all sides of the border of the control.
constexpr int kSpacingInsetsAllSides = 45;

class CloseFullscreenButton : public views::Button {
 public:
  explicit CloseFullscreenButton(views::ButtonListener* listener)
      : views::Button(listener) {}

 private:
  void PaintButtonContents(gfx::Canvas* canvas) override {
    // TODO(robliao): If we decide to move forward with this, use themes.
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(SK_ColorDKGRAY);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    float radius = kCircleButtonDiameter / 2.0f;
    canvas->DrawCircle(gfx::PointF(radius, radius), radius, flags);
  }

  DISALLOW_COPY_AND_ASSIGN(CloseFullscreenButton);
};

}  // namespace

FullscreenControlView::FullscreenControlView(BrowserView* browser_view)
    : browser_view_(browser_view),
      exit_fullscreen_button_(new CloseFullscreenButton(this)) {
  AddChildView(exit_fullscreen_button_);
  SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kHorizontal, gfx::Insets(kSpacingInsetsAllSides), 0));
  exit_fullscreen_button_->SetPreferredSize(
      gfx::Size(kCircleButtonDiameter, kCircleButtonDiameter));
}

FullscreenControlView::~FullscreenControlView() = default;

void FullscreenControlView::SetFocusAndSelection(bool select_all) {}

void FullscreenControlView::ButtonPressed(views::Button* sender,
                                          const ui::Event& event) {
  if (sender == exit_fullscreen_button_)
    browser_view_->ExitFullscreen();
}
