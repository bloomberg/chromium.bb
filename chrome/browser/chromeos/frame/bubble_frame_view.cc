// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/frame/bubble_frame_view.h"

#include "gfx/canvas_skia.h"
#include "gfx/font.h"
#include "gfx/insets.h"
#include "gfx/path.h"
#include "gfx/rect.h"
#include "chrome/browser/chromeos/frame/bubble_window.h"
#include "chrome/browser/views/bubble_border.h"
#include "views/controls/label.h"
#include "views/window/hit_test.h"
#include "views/window/window.h"
#include "views/window/window_delegate.h"
#include "third_party/skia/include/core/SkPaint.h"

namespace {

static const int kTitleTopPadding = 10;
static const int kTitleContentPadding = 10;
static const int kHorizontalPadding = 10;

}  // namespace

namespace chromeos {

BubbleFrameView::BubbleFrameView(views::Window* frame)
    : frame_(frame),
      title_(NULL) {
  set_border(new BubbleBorder(BubbleBorder::NONE));

  title_ = new views::Label(frame_->GetDelegate()->GetWindowTitle());
  title_->SetFont(title_->font().DeriveFont(1, gfx::Font::BOLD));
  AddChildView(title_);
}

BubbleFrameView::~BubbleFrameView() {
}

gfx::Rect BubbleFrameView::GetBoundsForClientView() const {
  return client_view_bounds_;
}

gfx::Rect BubbleFrameView::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  gfx::Insets insets = GetInsets();
  gfx::Size title_size = title_->GetPreferredSize();
  int top_height = insets.top() + title_size.height() + kTitleContentPadding;
  return gfx::Rect(std::max(0, client_bounds.x() - insets.left()),
                   std::max(0, client_bounds.y() - top_height),
                   client_bounds.width() + insets.width(),
                   client_bounds.height() + top_height + insets.bottom());
}

int BubbleFrameView::NonClientHitTest(const gfx::Point& point) {
  return HTNOWHERE;
}

void BubbleFrameView::GetWindowMask(const gfx::Size& size,
                                    gfx::Path* window_mask) {
}

void BubbleFrameView::EnableClose(bool enable) {
}

void BubbleFrameView::ResetWindowControls() {
}

gfx::Insets BubbleFrameView::GetInsets() const {
  gfx::Insets border_insets;
  border()->GetInsets(&border_insets);

  gfx::Insets insets(kTitleTopPadding,
                     kHorizontalPadding,
                     0,
                     kHorizontalPadding);
  insets += border_insets;
  return insets;
}

gfx::Size BubbleFrameView::GetPreferredSize() {
  gfx::Size pref = frame_->GetClientView()->GetPreferredSize();
  gfx::Rect bounds(0, 0, pref.width(), pref.height());
  return frame_->GetNonClientView()->GetWindowBoundsForClientBounds(
      bounds).size();
}

void BubbleFrameView::Layout() {
  gfx::Insets insets = GetInsets();

  gfx::Size title_size = title_->GetPreferredSize();
  title_->SetBounds(insets.left(), insets.top(),
      std::max(0, width() - insets.width()),
      title_size.height());

  int top_height = insets.top() + title_size.height() + kTitleContentPadding;
  client_view_bounds_.SetRect(insets.left(), top_height,
      std::max(0, width() - insets.width()),
      std::max(0, height() - top_height - insets.bottom()));
}

void BubbleFrameView::Paint(gfx::Canvas* canvas) {
  // The border of this view creates an anti-aliased round-rect region for the
  // contents, which we need to fill with the background color.
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(BubbleWindow::kBackgroundColor);
  gfx::Path path;
  gfx::Rect bounds(GetLocalBounds(false));
  SkRect rect;
  rect.set(SkIntToScalar(bounds.x()), SkIntToScalar(bounds.y()),
           SkIntToScalar(bounds.right()), SkIntToScalar(bounds.bottom()));
  SkScalar radius = SkIntToScalar(BubbleBorder::GetCornerRadius());
  path.addRoundRect(rect, radius, radius);
  canvas->AsCanvasSkia()->drawPath(path, paint);

  PaintBorder(canvas);
}

}  // namespace chromeos
