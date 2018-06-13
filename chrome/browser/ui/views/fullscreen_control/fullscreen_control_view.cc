// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/fullscreen_control/fullscreen_control_view.h"

#include <memory>

#include "base/callback.h"
#include "cc/paint/paint_flags.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/vector_icons.h"

namespace {

// Partially-transparent background color.
const SkColor kButtonBackgroundColor = SkColorSetARGB(0xcc, 0x28, 0x2c, 0x32);

constexpr int kCloseIconSize = 24;

class CloseFullscreenButton : public views::Button {
 public:
  explicit CloseFullscreenButton(views::ButtonListener* listener)
      : views::Button(listener) {
    std::unique_ptr<views::ImageView> close_image_view =
        std::make_unique<views::ImageView>();
    close_image_view->SetImage(gfx::CreateVectorIcon(
        views::kIcCloseIcon, kCloseIconSize, SK_ColorWHITE));
    SetAccessibleName(l10n_util::GetStringUTF16(IDS_EXIT_FULLSCREEN_MODE));
    AddChildView(close_image_view.release());
    SetLayoutManager(std::make_unique<views::FillLayout>());
  }

 private:
  void PaintButtonContents(gfx::Canvas* canvas) override {
    // TODO(robliao): If we decide to move forward with this, use themes.
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(kButtonBackgroundColor);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    float radius = FullscreenControlView::kCircleButtonDiameter / 2.0f;
    canvas->DrawCircle(gfx::PointF(radius, radius), radius, flags);
  }

  DISALLOW_COPY_AND_ASSIGN(CloseFullscreenButton);
};

}  // namespace

FullscreenControlView::FullscreenControlView(
    const base::RepeatingClosure& on_button_pressed)
    : on_button_pressed_(on_button_pressed),
      exit_fullscreen_button_(new CloseFullscreenButton(this)) {
  AddChildView(exit_fullscreen_button_);
  SetLayoutManager(std::make_unique<views::FillLayout>());
  exit_fullscreen_button_->SetPreferredSize(
      gfx::Size(kCircleButtonDiameter, kCircleButtonDiameter));
}

FullscreenControlView::~FullscreenControlView() = default;

void FullscreenControlView::ButtonPressed(views::Button* sender,
                                          const ui::Event& event) {
  if (sender == exit_fullscreen_button_ && on_button_pressed_)
    on_button_pressed_.Run();
}
