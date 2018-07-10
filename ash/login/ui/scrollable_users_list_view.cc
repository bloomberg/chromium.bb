// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/scrollable_users_list_view.h"

#include "ash/login/ui/hover_notifier.h"
#include "ash/login/ui/login_display_style.h"
#include "ash/login/ui/login_user_view.h"
#include "ash/login/ui/non_accessible_view.h"
#include "ash/login/ui/views_utils.h"
#include "ash/public/cpp/login_constants.h"
#include "ash/public/interfaces/login_user_info.mojom.h"
#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_controller.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/controls/scrollbar/base_scroll_bar.h"
#include "ui/views/controls/scrollbar/base_scroll_bar_thumb.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Vertical padding between user rows in the small display style.
constexpr int kSmallVerticalDistanceBetweenUsersDp = 53;

// Horizontal padding left and right of the user list in the extra small display
// style.
constexpr int kExtraSmallHorizontalPaddingLeftOfUserListLandscapeDp = 72;
constexpr int kExtraSmallHorizontalPaddingRightOfUserListLandscapeDp = 72;
constexpr int kExtraSmallHorizontalPaddingLeftOfUserListPortraitDp = 46;
constexpr int kExtraSmallHorizontalPaddingRightOfUserListPortraitDp = 12;

// Vertical padding of the user list in the extra small display style.
constexpr int kExtraSmallVerticalPaddingOfUserListLandscapeDp = 72;
constexpr int kExtraSmallVerticalPaddingOfUserListPortraitDp = 66;

// Vertical padding between user rows in extra small display style.
constexpr int kExtraSmallVerticalDistanceBetweenUsersDp = 32;

// Height of gradient shown at the top/bottom of the user list in the extra
// small display style.
constexpr int kExtraSmallGradientHeightDp = 112;

// Thickness of scroll bar thumb.
constexpr int kScrollThumbThicknessDp = 6;
// Padding on the right of scroll bar thumb.
constexpr int kScrollThumbPaddingDp = 8;
// Radius of the scroll bar thumb.
constexpr int kScrollThumbRadiusDp = 8;
// Alpha of scroll bar thumb (43/255 = 17%).
constexpr int kScrollThumbAlpha = 43;

constexpr char kScrollableUsersListContentViewName[] =
    "ScrollableUsersListContent";

class ContentsView : public NonAccessibleView {
 public:
  ContentsView() : NonAccessibleView(kScrollableUsersListContentViewName) {}
  ~ContentsView() override = default;

  // NonAccessibleView:
  void Layout() override {
    // Make sure our height is at least as tall as the parent, so the layout
    // manager will center us properly.
    int min_height = parent()->height();
    if (size().height() < min_height) {
      gfx::Size new_size = size();
      new_size.set_height(min_height);
      SetSize(new_size);
    }
    NonAccessibleView::Layout();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentsView);
};

class ScrollBarThumb : public views::BaseScrollBarThumb {
 public:
  explicit ScrollBarThumb(views::BaseScrollBar* scroll_bar)
      : BaseScrollBarThumb(scroll_bar) {}
  ~ScrollBarThumb() override = default;

  // views::BaseScrollBarThumb:
  gfx::Size CalculatePreferredSize() const override {
    return gfx::Size(kScrollThumbThicknessDp, kScrollThumbThicknessDp);
  }
  void OnPaint(gfx::Canvas* canvas) override {
    cc::PaintFlags fill_flags;
    fill_flags.setStyle(cc::PaintFlags::kFill_Style);
    fill_flags.setColor(SkColorSetA(SK_ColorWHITE, kScrollThumbAlpha));
    canvas->DrawRoundRect(GetLocalBounds(), kScrollThumbRadiusDp, fill_flags);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScrollBarThumb);
};

struct LayoutParams {
  // Spacing between user entries on users list.
  int between_child_spacing;
  // Insets around users list used in landscape orientation.
  gfx::Insets insets_landscape;
  // Insets around users list used in portrait orientation.
  gfx::Insets insets_portrait;
};

// static
LayoutParams BuildLayoutForStyle(LoginDisplayStyle style) {
  switch (style) {
    case LoginDisplayStyle::kExtraSmall: {
      LayoutParams params;
      params.between_child_spacing = kExtraSmallVerticalDistanceBetweenUsersDp;
      params.insets_landscape =
          gfx::Insets(kExtraSmallVerticalPaddingOfUserListLandscapeDp,
                      kExtraSmallHorizontalPaddingLeftOfUserListLandscapeDp,
                      kExtraSmallVerticalPaddingOfUserListLandscapeDp,
                      kExtraSmallHorizontalPaddingRightOfUserListLandscapeDp);
      params.insets_portrait =
          gfx::Insets(kExtraSmallVerticalPaddingOfUserListPortraitDp,
                      kExtraSmallHorizontalPaddingLeftOfUserListPortraitDp,
                      kExtraSmallVerticalPaddingOfUserListPortraitDp,
                      kExtraSmallHorizontalPaddingRightOfUserListPortraitDp);
      return params;
    }
    case LoginDisplayStyle::kSmall: {
      LayoutParams params;
      params.between_child_spacing = kSmallVerticalDistanceBetweenUsersDp;
      return params;
    }
    default: {
      NOTREACHED();
      return LayoutParams();
    }
  }
}

}  // namespace

class ScrollableUsersListView::ScrollBar : public views::BaseScrollBar {
 public:
  explicit ScrollBar(bool horizontal) : BaseScrollBar(horizontal) {
    SetThumb(new ScrollBarThumb(this));
  }
  ~ScrollBar() override = default;

  void SetThumbVisible(bool visible) { GetThumb()->SetVisible(visible); }

  // views::BaseScrollBar:
  gfx::Rect GetTrackBounds() const override { return GetLocalBounds(); }
  bool OverlapsContent() const override { return true; }
  int GetThickness() const override {
    return kScrollThumbThicknessDp + kScrollThumbPaddingDp;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScrollBar);
};

// static
ScrollableUsersListView::GradientParams
ScrollableUsersListView::GradientParams::BuildForStyle(
    LoginDisplayStyle style) {
  switch (style) {
    case LoginDisplayStyle::kExtraSmall: {
      SkColor dark_muted_color =
          Shell::Get()->wallpaper_controller()->GetProminentColor(
              color_utils::ColorProfile(color_utils::LumaRange::DARK,
                                        color_utils::SaturationRange::MUTED));
      SkColor tint_color = color_utils::GetResultingPaintColor(
          SkColorSetA(login_constants::kDefaultBaseColor,
                      login_constants::kTranslucentColorDarkenAlpha),
          SkColorSetA(dark_muted_color, SK_AlphaOPAQUE));
      tint_color =
          SkColorSetA(tint_color, login_constants::kScrollTranslucentAlpha);

      GradientParams params;
      params.color_from = dark_muted_color;
      params.color_to = tint_color;
      params.height = kExtraSmallGradientHeightDp;
      return params;
    }
    case LoginDisplayStyle::kSmall: {
      GradientParams params;
      params.height = 0.f;
      return params;
    }
    default: {
      NOTREACHED();
      return GradientParams();
    }
  }
}

ScrollableUsersListView::TestApi::TestApi(ScrollableUsersListView* view)
    : view_(view) {}

ScrollableUsersListView::TestApi::~TestApi() = default;

const std::vector<LoginUserView*>&
ScrollableUsersListView::TestApi::user_views() const {
  return view_->user_views_;
}

ScrollableUsersListView::ScrollableUsersListView(
    const std::vector<mojom::LoginUserInfoPtr>& users,
    const ActionWithUser& on_tap_user,
    LoginDisplayStyle display_style)
    : display_style_(display_style), observer_(this) {
  auto layout_params = BuildLayoutForStyle(display_style);
  gradient_params_ = GradientParams::BuildForStyle(display_style);

  auto* contents = new ContentsView();
  contents_layout_ =
      contents->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::kVertical, gfx::Insets(),
          layout_params.between_child_spacing));
  contents_layout_->set_minimum_cross_axis_size(
      LoginUserView::WidthForLayoutStyle(display_style));
  contents_layout_->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::MAIN_AXIS_ALIGNMENT_CENTER);

  for (std::size_t i = 1u; i < users.size(); ++i) {
    auto* view = new LoginUserView(
        display_style, false /*show_dropdown*/, false /*show_domain*/,
        base::BindRepeating(on_tap_user, i - 1), base::RepeatingClosure(),
        base::RepeatingClosure());
    user_views_.push_back(view);
    view->UpdateForUser(users[i], false /*animate*/);
    contents->AddChildView(view);
  }

  SetContents(contents);
  SetBackgroundColor(SK_ColorTRANSPARENT);
  set_draw_overflow_indicator(false);

  scroll_bar_ = new ScrollBar(false);
  SetVerticalScrollBar(scroll_bar_);
  SetHorizontalScrollBar(new ScrollBar(true));

  hover_notifier_ = std::make_unique<HoverNotifier>(
      this, base::BindRepeating(&ScrollableUsersListView::OnHover,
                                base::Unretained(this)));

  observer_.Add(Shell::Get()->wallpaper_controller());
}

ScrollableUsersListView::~ScrollableUsersListView() = default;

LoginUserView* ScrollableUsersListView::GetUserView(
    const AccountId& account_id) {
  for (auto* view : user_views_) {
    if (view->current_user()->basic_user_info->account_id == account_id)
      return view;
  }
  return nullptr;
}

void ScrollableUsersListView::Layout() {
  DCHECK(contents_layout_);

  // Make sure to resize width, which may be invalid. For example, this happens
  // after a rotation.
  SizeToPreferredSize();

  // Update clipping height.
  if (parent()) {
    int parent_height = parent()->size().height();
    ClipHeightTo(parent_height, parent_height);
    if (height() != parent_height)
      PreferredSizeChanged();
  }

  // Update |contents()| layout spec.
  bool should_show_landscape =
      login_views_utils::ShouldShowLandscape(GetWidget());
  LayoutParams layout_params = BuildLayoutForStyle(display_style_);
  contents_layout_->set_inside_border_insets(
      should_show_landscape ? layout_params.insets_landscape
                            : layout_params.insets_portrait);

  // Layout everything.
  ScrollView::Layout();

  // Update scrollbar visibility.
  if (scroll_bar_)
    scroll_bar_->SetThumbVisible(IsMouseHovered());
}

void ScrollableUsersListView::OnPaintBackground(gfx::Canvas* canvas) {
  // Draws symmetrical linear gradient at the top and bottom of the view.
  SkScalar view_height = height();
  // Start and end point of the drawing in view space.
  SkPoint in_view_coordinates[2] = {SkPoint(), SkPoint::Make(0.f, view_height)};
  // Positions of colors to create gradient define in 0 to 1 range.
  SkScalar top_gradient_end = gradient_params_.height / view_height;
  SkScalar bottom_gradient_start = 1.f - top_gradient_end;
  SkScalar color_positions[4] = {0.f, top_gradient_end, bottom_gradient_start,
                                 1.f};
  SkColor colors[4] = {gradient_params_.color_from, gradient_params_.color_to,
                       gradient_params_.color_to, gradient_params_.color_from};

  cc::PaintFlags flags;
  flags.setShader(cc::PaintShader::MakeLinearGradient(
      in_view_coordinates, colors, color_positions, 4,
      SkShader::kClamp_TileMode));
  flags.setStyle(cc::PaintFlags::kFill_Style);
  canvas->DrawRect(GetLocalBounds(), flags);
}

// When the active user is updated, the wallpaper changes. The gradient color
// should be updated in response to the new primary wallpaper color.
void ScrollableUsersListView::OnWallpaperColorsChanged() {
  gradient_params_ = GradientParams::BuildForStyle(display_style_);
  SchedulePaint();
}

void ScrollableUsersListView::OnHover(bool has_hover) {
  scroll_bar_->SetThumbVisible(has_hover);
}

}  // namespace ash
