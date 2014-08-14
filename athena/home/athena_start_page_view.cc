// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/home/athena_start_page_view.h"

#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/app_list/app_list_item.h"
#include "ui/app_list/app_list_item_list.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/app_list_view_delegate.h"
#include "ui/app_list/views/search_box_view.h"
#include "ui/gfx/canvas.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/round_rect_painter.h"

namespace {

const size_t kMaxIconNum = 3;
const int kIconSize = 50;
const int kIconMargin = 25;

const int kBorderWidth = 1;
const int kCornerRadius = 1;

// The preferred height for VISIBLE_BOTTOM state.
const int kPreferredHeightBottom = 100;

class PlaceHolderButton : public views::ImageButton,
                          public views::ButtonListener {
 public:
  PlaceHolderButton()
      : ImageButton(this) {
    gfx::Canvas canvas(gfx::Size(kIconSize, kIconSize), 1.0f, true);
    SkPaint paint;
    paint.setStyle(SkPaint::kFill_Style);
    paint.setColor(SkColorSetRGB(86, 119, 252));
    paint.setFlags(SkPaint::kAntiAlias_Flag);
    canvas.DrawCircle(
        gfx::Point(kIconSize / 2, kIconSize / 2), kIconSize / 2, paint);

    scoped_ptr<gfx::ImageSkia> image(
        new gfx::ImageSkia(canvas.ExtractImageRep()));
    SetImage(STATE_NORMAL, image.get());
  }

 private:
  // views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE {
    // Do nothing: remove these place holders.
  }

  DISALLOW_COPY_AND_ASSIGN(PlaceHolderButton);
};

class AppIconButton : public views::ImageButton,
                      public views::ButtonListener {
 public:
  explicit AppIconButton(app_list::AppListItem* item)
      : ImageButton(this),
        item_(item) {
    // TODO(mukai): icon should be resized.
    SetImage(STATE_NORMAL, &item->icon());
  }

 private:
  // views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE {
    DCHECK_EQ(sender, this);
    item_->Activate(event.flags());
  }

  app_list::AppListItem* item_;

  DISALLOW_COPY_AND_ASSIGN(AppIconButton);
};

// The background to paint the round rectangle of the view area.
class RoundRectBackground : public views::Background {
 public:
  RoundRectBackground(SkColor color, int corner_radius)
      : color_(color),
        corner_radius_(corner_radius) {}
  virtual ~RoundRectBackground() {}

 private:
  // views::Background:
  virtual void Paint(gfx::Canvas* canvas, views::View* view) const OVERRIDE {
    SkPaint paint;
    paint.setStyle(SkPaint::kFill_Style);
    paint.setColor(color_);
    canvas->DrawRoundRect(view->GetContentsBounds(), corner_radius_, paint);
  }

  SkColor color_;
  int corner_radius_;

  DISALLOW_COPY_AND_ASSIGN(RoundRectBackground);
};

}  // namespace

namespace athena {

AthenaStartPageView::AthenaStartPageView(
    app_list::AppListViewDelegate* view_delegate) {
  app_list::AppListItemList* top_level =
      view_delegate->GetModel()->top_level_item_list();

  container_ = new views::View();
  AddChildView(container_);

  views::BoxLayout* box_layout = new views::BoxLayout(
      views::BoxLayout::kHorizontal, kIconMargin, kIconMargin, kIconMargin);
  box_layout->set_main_axis_alignment(
      views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_CENTER);
  container_->SetLayoutManager(box_layout);
  for (size_t i = 0; i < std::min(top_level->item_count(), kMaxIconNum); ++i)
    container_->AddChildView(new AppIconButton(top_level->item_at(i)));

  views::View* search_box_container = new views::View();
  search_box_container->set_background(
      new RoundRectBackground(SK_ColorWHITE, kCornerRadius));
  search_box_container->SetBorder(views::Border::CreateBorderPainter(
      new views::RoundRectPainter(SK_ColorGRAY, kCornerRadius),
      gfx::Insets(kBorderWidth, kBorderWidth, kBorderWidth, kBorderWidth)));
  search_box_container->SetLayoutManager(new views::FillLayout());
  search_box_container->AddChildView(
      new app_list::SearchBoxView(this, view_delegate));
  container_->AddChildView(search_box_container);
  box_layout->SetFlexForView(search_box_container, 1);

  for (size_t i = 0; i < kMaxIconNum; ++i)
    container_->AddChildView(new PlaceHolderButton());

  set_background(views::Background::CreateSolidBackground(
      255, 255, 255, 255 * 0.9));
}

AthenaStartPageView::~AthenaStartPageView() {}

void AthenaStartPageView::Layout() {
  gfx::Rect container_bounds = bounds();
  container_bounds.set_height(kPreferredHeightBottom);
  container_->SetBoundsRect(container_bounds);
}

void AthenaStartPageView::QueryChanged(app_list::SearchBoxView* sender) {
  // Nothing needs to be done.
}

}  // namespace athena
