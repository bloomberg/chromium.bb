// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/home/athena_start_page_view.h"

#include "base/bind.h"
#include "base/strings/string_util.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/app_list/app_list_item.h"
#include "ui/app_list/app_list_item_list.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/app_list_view_delegate.h"
#include "ui/app_list/search_box_model.h"
#include "ui/app_list/views/search_box_view.h"
#include "ui/app_list/views/search_result_list_view.h"
#include "ui/compositor/closure_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/round_rect_painter.h"

namespace {

const size_t kMaxIconNum = 3;
const int kIconSize = 50;
const int kIconMargin = 25;

const int kTopMargin = 100;

// Copied from ui/app_list/views/start_page_view.cc
const int kInstantContainerSpacing = 20;
const int kWebViewWidth = 500;
const int kWebViewHeight = 105;
const int kSearchBoxBorderWidth = 1;
const int kSearchBoxCornerRadius = 2;
const int kSearchBoxWidth = 490;
const int kSearchBoxHeight = 40;

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

class SearchBoxContainer : public views::View {
 public:
  explicit SearchBoxContainer(app_list::SearchBoxView* search_box)
      : search_box_(search_box) {
    search_box->set_background(
        new RoundRectBackground(SK_ColorWHITE, kSearchBoxCornerRadius));
    search_box->SetBorder(views::Border::CreateBorderPainter(
        new views::RoundRectPainter(SK_ColorGRAY, kSearchBoxCornerRadius),
        gfx::Insets(kSearchBoxBorderWidth, kSearchBoxBorderWidth,
                    kSearchBoxBorderWidth, kSearchBoxBorderWidth)));
    AddChildView(search_box_);
  }
  virtual ~SearchBoxContainer() {}

 private:
  // views::View:
  virtual void Layout() OVERRIDE {
    gfx::Rect search_box_bounds = GetContentsBounds();
    search_box_bounds.ClampToCenteredSize(GetPreferredSize());
    search_box_->SetBoundsRect(search_box_bounds);
  }
  virtual gfx::Size GetPreferredSize() const OVERRIDE {
    return gfx::Size(kSearchBoxWidth, kSearchBoxHeight);
  }

  // Owned by the views hierarchy.
  app_list::SearchBoxView* search_box_;

  DISALLOW_COPY_AND_ASSIGN(SearchBoxContainer);
};

}  // namespace

namespace athena {

AthenaStartPageView::AthenaStartPageView(
    app_list::AppListViewDelegate* view_delegate)
    : delegate_(view_delegate),
      weak_factory_(this) {
  logo_ = view_delegate->CreateStartPageWebView(
      gfx::Size(kWebViewWidth, kWebViewHeight));
  logo_->SetPaintToLayer(true);
  AddChildView(logo_);

  search_results_view_ = new app_list::SearchResultListView(
      NULL, view_delegate);
  // search_results_view_'s size will shrink after settings results.
  search_results_height_ = static_cast<views::View*>(
      search_results_view_)->GetHeightForWidth(kSearchBoxWidth);
  search_results_view_->SetResults(view_delegate->GetModel()->results());

  search_results_view_->SetVisible(false);
  search_results_view_->SetPaintToLayer(true);
  search_results_view_->SetFillsBoundsOpaquely(false);
  AddChildView(search_results_view_);

  app_icon_container_ = new views::View();
  AddChildView(app_icon_container_);
  app_icon_container_->SetPaintToLayer(true);
  app_icon_container_->layer()->SetFillsBoundsOpaquely(false);
  app_icon_container_->SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kHorizontal, 0, 0, kIconMargin));
  app_list::AppListItemList* top_level =
      view_delegate->GetModel()->top_level_item_list();
  for (size_t i = 0; i < std::min(top_level->item_count(), kMaxIconNum); ++i)
    app_icon_container_->AddChildView(new AppIconButton(top_level->item_at(i)));

  search_box_view_ = new app_list::SearchBoxView(this, view_delegate);
  search_box_view_->set_contents_view(this);
  search_box_container_ = new SearchBoxContainer(search_box_view_);
  search_box_container_->SetPaintToLayer(true);
  search_box_container_->SetFillsBoundsOpaquely(false);
  AddChildView(search_box_container_);

  control_icon_container_ = new views::View();
  control_icon_container_->SetPaintToLayer(true);
  control_icon_container_->SetFillsBoundsOpaquely(false);
  AddChildView(control_icon_container_);
  control_icon_container_->SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kHorizontal, 0, 0, kIconMargin));
  for (size_t i = 0; i < kMaxIconNum; ++i)
    control_icon_container_->AddChildView(new PlaceHolderButton());
}

AthenaStartPageView::~AthenaStartPageView() {}

void AthenaStartPageView::RequestFocusOnSearchBox() {
  search_box_view_->search_box()->RequestFocus();
}

void AthenaStartPageView::LayoutSearchResults(bool should_show_search_results) {
  if (should_show_search_results ==
      search_results_view_->layer()->GetTargetVisibility()) {
    return;
  }
  if (GetContentsBounds().height() <= kPreferredHeightBottom) {
    search_results_view_->SetVisible(false);
    Layout();
    return;
  }

  gfx::Rect search_box_bounds = search_box_container_->bounds();
  if (!search_results_view_->visible()) {
    search_results_view_->SetVisible(true);
    search_results_view_->SetBounds(
        search_box_bounds.x(), search_box_bounds.bottom(),
        search_box_bounds.width(), 0);
  }
  logo_->SetVisible(true);

  {
    ui::ScopedLayerAnimationSettings logo_settings(
        logo_->layer()->GetAnimator());
    logo_settings.SetTweenType(gfx::Tween::EASE_IN_OUT);
    logo_settings.AddObserver(new ui::ClosureAnimationObserver(
        base::Bind(&AthenaStartPageView::OnSearchResultLayoutAnimationCompleted,
                   weak_factory_.GetWeakPtr(),
                   should_show_search_results)));

    ui::ScopedLayerAnimationSettings search_box_settings(
        search_box_container_->layer()->GetAnimator());
    search_box_settings.SetTweenType(gfx::Tween::EASE_IN_OUT);

    ui::ScopedLayerAnimationSettings search_results_settings(
        search_results_view_->layer()->GetAnimator());
    search_results_settings.SetTweenType(gfx::Tween::EASE_IN_OUT);

    if (should_show_search_results) {
      logo_->layer()->SetOpacity(0.0f);
      search_box_bounds.set_y(
          search_box_bounds.y() - search_results_height_ -
          kInstantContainerSpacing);
      search_box_container_->SetBoundsRect(search_box_bounds);
      search_results_view_->SetBounds(
          search_box_bounds.x(),
          search_box_bounds.bottom() + kInstantContainerSpacing,
          search_box_bounds.width(),
          search_results_height_);
    } else {
      logo_->layer()->SetOpacity(1.0f);
      search_box_bounds.set_y(
          logo_->bounds().bottom() + kInstantContainerSpacing);
      search_box_container_->SetBoundsRect(search_box_bounds);

      gfx::Rect search_results_bounds = search_results_view_->bounds();
      search_results_bounds.set_y(search_results_bounds.bottom());
      search_results_bounds.set_height(0);
      search_results_view_->SetBoundsRect(search_results_bounds);
    }
  }
}

void AthenaStartPageView::OnSearchResultLayoutAnimationCompleted(
    bool should_show_search_results) {
  logo_->SetVisible(!should_show_search_results);
  search_results_view_->SetVisible(should_show_search_results);
}

void AthenaStartPageView::Layout() {
  gfx::Rect bounds = GetContentsBounds();
  search_results_view_->SetVisible(false);

  if (bounds.height() <= kPreferredHeightBottom) {
    logo_->SetVisible(false);
    gfx::Rect icon_bounds(app_icon_container_->GetPreferredSize());
    icon_bounds.set_x(bounds.x() + kIconMargin);
    icon_bounds.set_y(bounds.x() + kIconMargin);
    app_icon_container_->SetBoundsRect(icon_bounds);

    gfx::Rect control_bounds(control_icon_container_->GetPreferredSize());
    control_bounds.set_x(
        bounds.right() - kIconMargin - control_bounds.width());
    control_bounds.set_y(bounds.y() + kIconMargin);
    control_icon_container_->SetBoundsRect(control_bounds);

    search_box_container_->SetBounds(
        icon_bounds.right(), bounds.y(),
        control_bounds.x() - icon_bounds.right(), kPreferredHeightBottom);

    set_background(views::Background::CreateSolidBackground(
        255, 255, 255, 255 * 0.9));
  } else {
    // TODO(mukai): set the intermediate state.
    logo_->SetVisible(true);
    logo_->layer()->SetOpacity(1.0f);
    set_background(views::Background::CreateSolidBackground(SK_ColorWHITE));
    gfx::Rect logo_bounds(bounds.x() + bounds.width() / 2 - kWebViewWidth / 2,
                          bounds.y() + kTopMargin,
                          kWebViewWidth,
                          kWebViewHeight);
    logo_->SetBoundsRect(logo_bounds);

    gfx::Rect search_box_bounds(search_box_container_->GetPreferredSize());
    search_box_bounds.set_x(
        bounds.x() + bounds.width() / 2 - search_box_bounds.width() / 2);
    search_box_bounds.set_y(logo_bounds.bottom() + kInstantContainerSpacing);
    search_box_container_->SetBoundsRect(search_box_bounds);

    gfx::Rect icon_bounds(app_icon_container_->GetPreferredSize());
    icon_bounds.set_x(bounds.x() + bounds.width() / 2 -
                      icon_bounds.width() - kIconMargin / 2);
    icon_bounds.set_y(search_box_bounds.bottom() + kInstantContainerSpacing);
    app_icon_container_->SetBoundsRect(icon_bounds);

    gfx::Rect control_bounds(control_icon_container_->GetPreferredSize());
    control_bounds.set_x(bounds.x() + bounds.width() / 2 +
                         kIconMargin / 2 + kIconMargin % 2);
    control_bounds.set_y(icon_bounds.y());
    control_icon_container_->SetBoundsRect(control_bounds);
  }
}

bool AthenaStartPageView::OnKeyPressed(const ui::KeyEvent& key_event) {
  return search_results_view_->visible() &&
      search_results_view_->OnKeyPressed(key_event);
}

void AthenaStartPageView::QueryChanged(app_list::SearchBoxView* sender) {
  delegate_->StartSearch();

  base::string16 query;
  base::TrimWhitespace(
      delegate_->GetModel()->search_box()->text(), base::TRIM_ALL, &query);

  if (!query.empty())
    search_results_view_->SetSelectedIndex(0);

  LayoutSearchResults(!query.empty());
}

}  // namespace athena
