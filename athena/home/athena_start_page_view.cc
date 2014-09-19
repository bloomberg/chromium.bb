// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/home/athena_start_page_view.h"

#include "athena/home/home_card_constants.h"
#include "athena/system/public/system_ui.h"
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

// Taken from the mock. The width is not specified by pixel but the search box
// covers 6 icons with margin.
const int kSearchBoxWidth = kIconSize * 6 + kIconMargin * 7;
const int kSearchBoxHeight = 40;

gfx::Size GetIconContainerSize() {
  return gfx::Size(kIconSize *  kMaxIconNum + kIconMargin * (kMaxIconNum - 1),
                   kIconSize);
}

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
    SetLayoutManager(new views::FillLayout());
    AddChildView(search_box_);
  }
  virtual ~SearchBoxContainer() {}

 private:
  // views::View:
  virtual gfx::Size GetPreferredSize() const OVERRIDE {
    return gfx::Size(kSearchBoxWidth, kSearchBoxHeight);
  }

  // Owned by the views hierarchy.
  app_list::SearchBoxView* search_box_;

  DISALLOW_COPY_AND_ASSIGN(SearchBoxContainer);
};

}  // namespace

namespace athena {

// static
const char AthenaStartPageView::kViewClassName[] = "AthenaStartPageView";

AthenaStartPageView::LayoutData::LayoutData()
    : system_info_opacity(1.0f),
      logo_opacity(1.0f),
      background_opacity(1.0f) {
}

AthenaStartPageView::AthenaStartPageView(
    app_list::AppListViewDelegate* view_delegate)
    : delegate_(view_delegate),
      layout_state_(0.0f),
      weak_factory_(this) {
  background_ = new views::View();
  background_->set_background(
      views::Background::CreateSolidBackground(SK_ColorWHITE));
  background_->SetPaintToLayer(true);
  background_->SetFillsBoundsOpaquely(false);
  AddChildView(background_);

  system_info_view_ =
      SystemUI::Get()->CreateSystemInfoView(SystemUI::COLOR_SCHEME_DARK);
  system_info_view_->SetPaintToLayer(true);
  system_info_view_->SetFillsBoundsOpaquely(false);
  AddChildView(system_info_view_);

  logo_ = view_delegate->CreateStartPageWebView(
      gfx::Size(kWebViewWidth, kWebViewHeight));
  logo_->SetPaintToLayer(true);
  logo_->SetFillsBoundsOpaquely(false);
  logo_->SetSize(gfx::Size(kWebViewWidth, kWebViewHeight));
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
  app_icon_container_->SetSize(GetIconContainerSize());

  control_icon_container_ = new views::View();
  control_icon_container_->SetPaintToLayer(true);
  control_icon_container_->SetFillsBoundsOpaquely(false);
  AddChildView(control_icon_container_);
  control_icon_container_->SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kHorizontal, 0, 0, kIconMargin));
  for (size_t i = 0; i < kMaxIconNum; ++i)
    control_icon_container_->AddChildView(new PlaceHolderButton());
  control_icon_container_->SetSize(GetIconContainerSize());

  search_box_view_ = new app_list::SearchBoxView(this, view_delegate);
  search_box_view_->set_contents_view(this);
  search_box_view_->search_box()->set_id(kHomeCardSearchBoxId);
  search_box_container_ = new SearchBoxContainer(search_box_view_);
  search_box_container_->SetPaintToLayer(true);
  search_box_container_->SetFillsBoundsOpaquely(false);
  search_box_container_->SetSize(search_box_container_->GetPreferredSize());
  AddChildView(search_box_container_);
}

AthenaStartPageView::~AthenaStartPageView() {}

void AthenaStartPageView::RequestFocusOnSearchBox() {
  search_box_view_->search_box()->RequestFocus();
}

void AthenaStartPageView::SetLayoutState(float layout_state) {
  layout_state_ = layout_state;
  Layout();
}

void AthenaStartPageView::SetLayoutStateWithAnimation(
    float layout_state,
    gfx::Tween::Type tween_type) {
  ui::ScopedLayerAnimationSettings system_info(
      system_info_view_->layer()->GetAnimator());
  ui::ScopedLayerAnimationSettings logo(logo_->layer()->GetAnimator());
  ui::ScopedLayerAnimationSettings search_box(
      search_box_container_->layer()->GetAnimator());
  ui::ScopedLayerAnimationSettings icons(
      app_icon_container_->layer()->GetAnimator());
  ui::ScopedLayerAnimationSettings controls(
      control_icon_container_->layer()->GetAnimator());

  system_info.SetTweenType(tween_type);
  logo.SetTweenType(tween_type);
  search_box.SetTweenType(tween_type);
  icons.SetTweenType(tween_type);
  controls.SetTweenType(tween_type);

  SetLayoutState(layout_state);
}

AthenaStartPageView::LayoutData AthenaStartPageView::CreateBottomBounds(
    int width) {
  LayoutData state;
  state.icons.set_size(app_icon_container_->size());
  state.icons.set_x(kIconMargin);
  state.icons.set_y(kIconMargin);

  state.controls.set_size(control_icon_container_->size());
  state.controls.set_x(width - kIconMargin - state.controls.width());
  state.controls.set_y(kIconMargin);

  int search_box_max_width =
      state.controls.x() - state.icons.right() - kIconMargin * 2;
  state.search_box.set_width(std::min(search_box_max_width, kSearchBoxWidth));
  state.search_box.set_height(search_box_container_->height());
  state.search_box.set_x((width - state.search_box.width()) / 2);
  state.search_box.set_y((kHomeCardHeight - state.search_box.height()) / 2);

  state.system_info_opacity = 0.0f;
  state.logo_opacity = 0.0f;
  state.background_opacity = 0.9f;
  return state;
}

AthenaStartPageView::LayoutData AthenaStartPageView::CreateCenteredBounds(
    int width) {
  LayoutData state;

  state.search_box.set_size(search_box_container_->GetPreferredSize());
  state.search_box.set_x((width - state.search_box.width()) / 2);
  state.search_box.set_y(logo_->bounds().bottom() + kInstantContainerSpacing);

  state.icons.set_size(app_icon_container_->size());
  state.icons.set_x(width / 2 - state.icons.width() - kIconMargin / 2);
  state.icons.set_y(state.search_box.bottom() + kInstantContainerSpacing);

  state.controls.set_size(control_icon_container_->size());
  state.controls.set_x(width / 2 + kIconMargin / 2 + kIconMargin % 2);
  state.controls.set_y(state.icons.y());

  state.system_info_opacity = 1.0f;
  state.logo_opacity = 1.0f;
  state.background_opacity = 1.0f;
  return state;
}

void AthenaStartPageView::LayoutSearchResults(bool should_show_search_results) {
  if (should_show_search_results ==
      search_results_view_->layer()->GetTargetVisibility()) {
    return;
  }
  if (GetContentsBounds().height() <= kHomeCardHeight) {
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
  search_results_view_->SetVisible(false);

  system_info_view_->SetBounds(
      0, 0, width(), system_info_view_->GetPreferredSize().height());

  gfx::Rect logo_bounds(x() + width() / 2 - kWebViewWidth / 2, y() + kTopMargin,
                        kWebViewWidth, kWebViewHeight);
  logo_->SetBoundsRect(logo_bounds);

  LayoutData bottom_bounds = CreateBottomBounds(width());
  LayoutData centered_bounds = CreateCenteredBounds(width());

  system_info_view_->layer()->SetOpacity(gfx::Tween::FloatValueBetween(
      gfx::Tween::CalculateValue(gfx::Tween::EASE_IN_2, layout_state_),
      bottom_bounds.system_info_opacity, centered_bounds.system_info_opacity));
  system_info_view_->SetVisible(
      system_info_view_->layer()->GetTargetOpacity() != 0.0f);

  logo_->layer()->SetOpacity(gfx::Tween::FloatValueBetween(
      gfx::Tween::CalculateValue(gfx::Tween::EASE_IN_2, layout_state_),
      bottom_bounds.logo_opacity, centered_bounds.logo_opacity));
  logo_->SetVisible(logo_->layer()->GetTargetOpacity() != 0.0f);

  app_icon_container_->SetBoundsRect(gfx::Tween::RectValueBetween(
      layout_state_, bottom_bounds.icons, centered_bounds.icons));
  control_icon_container_->SetBoundsRect(gfx::Tween::RectValueBetween(
      layout_state_, bottom_bounds.controls, centered_bounds.controls));
  search_box_container_->SetBoundsRect(gfx::Tween::RectValueBetween(
      layout_state_, bottom_bounds.search_box, centered_bounds.search_box));

  background_->SetBoundsRect(bounds());
  background_->layer()->SetOpacity(gfx::Tween::FloatValueBetween(
      layout_state_,
      bottom_bounds.background_opacity,
      centered_bounds.background_opacity));
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
