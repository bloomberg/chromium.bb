// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_background_view.h"

#include "ash/launcher/background_animator.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/system/tray/tray_constants.h"
#include "ui/aura/window.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"

namespace {

const SkColor kTrayBackgroundAlpha = 100;
const SkColor kTrayBackgroundHoverAlpha = 150;

}  // namespace

namespace ash {
namespace internal {

class TrayBackground : public views::Background {
 public:
  TrayBackground() : alpha_(kTrayBackgroundAlpha) {}
  virtual ~TrayBackground() {}

  void set_alpha(int alpha) { alpha_ = alpha; }

 private:
  // Overridden from views::Background.
  virtual void Paint(gfx::Canvas* canvas, views::View* view) const OVERRIDE {
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setStyle(SkPaint::kFill_Style);
    paint.setColor(SkColorSetARGB(alpha_, 0, 0, 0));
    SkPath path;
    gfx::Rect bounds(view->bounds());
    SkScalar radius = SkIntToScalar(kTrayRoundedBorderRadius);
    path.addRoundRect(gfx::RectToSkRect(bounds), radius, radius);
    canvas->DrawPath(path, paint);
  }

  int alpha_;

  DISALLOW_COPY_AND_ASSIGN(TrayBackground);
};

////////////////////////////////////////////////////////////////////////////////
// TrayBackgroundView

TrayBackgroundView::TrayBackgroundView()
    : shelf_alignment_(SHELF_ALIGNMENT_BOTTOM),
      background_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(hide_background_animator_(
          this, 0, kTrayBackgroundAlpha)),
      ALLOW_THIS_IN_INITIALIZER_LIST(hover_background_animator_(
          this, 0, kTrayBackgroundHoverAlpha - kTrayBackgroundAlpha)) {
  set_border(views::Border::CreateEmptyBorder(0, 0,
        kPaddingFromBottomOfScreenBottomAlignment,
        kPaddingFromRightEdgeOfScreenBottomAlignment));
  set_notify_enter_exit_on_child(true);

  // Initially we want to paint the background, but without the hover effect.
  SetPaintsBackground(true, internal::BackgroundAnimator::CHANGE_IMMEDIATE);
  hover_background_animator_.SetPaintsBackground(false,
      internal::BackgroundAnimator::CHANGE_IMMEDIATE);
}

TrayBackgroundView::~TrayBackgroundView() {
}

void TrayBackgroundView::OnMouseEntered(const views::MouseEvent& event) {
  hover_background_animator_.SetPaintsBackground(true,
      internal::BackgroundAnimator::CHANGE_ANIMATE);
}

void TrayBackgroundView::OnMouseExited(const views::MouseEvent& event) {
  hover_background_animator_.SetPaintsBackground(false,
      internal::BackgroundAnimator::CHANGE_ANIMATE);
}

void TrayBackgroundView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

bool TrayBackgroundView::PerformAction(const views::Event& event) {
  return false;
}

void TrayBackgroundView::UpdateBackground(int alpha) {
  if (background_) {
    background_->set_alpha(hide_background_animator_.alpha() +
                           hover_background_animator_.alpha());
  }
  SchedulePaint();
}

void TrayBackgroundView::SetContents(views::View* contents) {
  background_ = new internal::TrayBackground;
  contents->set_background(background_);

  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0));
  AddChildView(contents);
}

void TrayBackgroundView::SetPaintsBackground(
      bool value,
      internal::BackgroundAnimator::ChangeType change_type) {
  hide_background_animator_.SetPaintsBackground(value, change_type);
}

void TrayBackgroundView::SetShelfAlignment(ShelfAlignment alignment) {
  shelf_alignment_ = alignment;
}

}  // namespace internal
}  // namespace ash
