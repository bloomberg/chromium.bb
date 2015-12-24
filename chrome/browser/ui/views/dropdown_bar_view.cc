// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/dropdown_bar_view.h"

#include "base/macros.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/painter.h"
#include "ui/views/widget/widget.h"

namespace {

// Background to paint toolbar background with rounded corners.
class DropdownBackground : public views::Background {
 public:
  explicit DropdownBackground(BrowserView* browser,
                              const gfx::ImageSkia* left_alpha_mask,
                              const gfx::ImageSkia* right_alpha_mask);
  ~DropdownBackground() override {}

  // Overridden from views::Background.
  void Paint(gfx::Canvas* canvas, views::View* view) const override;

 private:
  BrowserView* browser_view_;
  const gfx::ImageSkia* left_alpha_mask_;
  const gfx::ImageSkia* right_alpha_mask_;

  DISALLOW_COPY_AND_ASSIGN(DropdownBackground);
};

DropdownBackground::DropdownBackground(BrowserView* browser_view,
                                     const gfx::ImageSkia* left_alpha_mask,
                                     const gfx::ImageSkia* right_alpha_mask)
    : browser_view_(browser_view),
      left_alpha_mask_(left_alpha_mask),
      right_alpha_mask_(right_alpha_mask) {
}

void DropdownBackground::Paint(gfx::Canvas* canvas, views::View* view) const {
  // Find the offset from which to tile the toolbar background image.
  // First, get the origin with respect to the screen.
  gfx::Point origin = view->GetWidget()->GetWindowBoundsInScreen().origin();
  // Now convert from screen to parent coordinates.
  views::View::ConvertPointFromScreen(browser_view_, &origin);
  // Finally, calculate the background image tiling offset.
  origin = browser_view_->OffsetPointForToolbarBackgroundImage(origin);

  const ui::ThemeProvider* tp = view->GetThemeProvider();
  gfx::ImageSkia background = *tp->GetImageSkiaNamed(IDR_THEME_TOOLBAR);

  int left_edge_width = left_alpha_mask_->width();
  int right_edge_width = right_alpha_mask_->width();
  int mask_height = left_alpha_mask_->height();
  int height = view->bounds().height();

  // Stretch the middle background to cover the entire area.
  canvas->TileImageInt(background, origin.x(), origin.y(),
      0, 0, view->bounds().width(), height);

  SkPaint paint;
  paint.setXfermodeMode(SkXfermode::kDstIn_Mode);
  // Draw left edge.
  canvas->DrawImageInt(*left_alpha_mask_, 0, 0, left_edge_width, mask_height,
      0, 0, left_edge_width, height, false, paint);

  // Draw right edge.
  int x_right_edge = view->bounds().width() - right_edge_width;
  canvas->DrawImageInt(*right_alpha_mask_, 0, 0, right_edge_width,
      mask_height, x_right_edge, 0, right_edge_width, height, false, paint);
}

}  // namespace

DropdownBarView::DropdownBarView(DropdownBarHost* host) : host_(host) {}

DropdownBarView::~DropdownBarView() {
}

////////////////////////////////////////////////////////////////////////////////
// DropDownBarView, protected:

void DropdownBarView::SetBackground(const gfx::ImageSkia* left_alpha_mask,
                                    const gfx::ImageSkia* right_alpha_mask) {
  set_background(new DropdownBackground(host()->browser_view(), left_alpha_mask,
      right_alpha_mask));
}

void DropdownBarView::SetBorderFromIds(int left_border_image_id,
                                       int middle_border_image_id,
                                       int right_border_image_id) {
  int border_image_ids[3] = {left_border_image_id, middle_border_image_id,
      right_border_image_id};
  SetBorder(views::Border::CreateBorderPainter(
      new views::HorizontalPainter(border_image_ids), gfx::Insets()));
}
