// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/scrollable_shelf_view.h"

#include "ash/public/cpp/shelf_config.h"
#include "ash/screen_util.h"
#include "ash/shelf/shelf_focus_cycler.h"
#include "ash/shelf/shelf_tooltip_manager.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/system/status_area_widget.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/numerics/ranges.h"
#include "chromeos/constants/chromeos_switches.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/vector2d_conversions.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/focus/focus_search.h"
#include "ui/views/view_targeter_delegate.h"

namespace ash {

namespace {

// Padding between the arrow button and the end of ScrollableShelf. It is
// applied when the arrow button shows.
constexpr int kArrowButtonEndPadding = 6;

// Padding between the the shelf container view and the arrow button (if any).
constexpr int kDistanceToArrowButton = 2;

// Size of the arrow button.
constexpr int kArrowButtonSize = 20;

// The distance between ShelfContainerView and the end of ScrollableShelf when
// the arrow button shows.
constexpr int kArrowButtonGroupWidth =
    kArrowButtonSize + kArrowButtonEndPadding + kDistanceToArrowButton;

// The gesture fling event with the velocity smaller than the threshold will be
// ignored.
constexpr int kGestureFlingVelocityThreshold = 1000;

// The mouse wheel event (including touchpad scrolling) with the main axis
// offset smaller than the threshold will be ignored.
constexpr int KScrollOffsetThreshold = 20;

// Horizontal size of the tap areafor the overflow arrow button.
constexpr int kArrowButtonTapAreaHorizontal = 32;

// Length of the fade in/out zone.
constexpr int kGradientZoneLength = 26;

// Sum of the shelf button size and the gap between shelf buttons.
int GetUnit() {
  return ShelfConfig::Get()->button_size() +
         ShelfConfig::Get()->button_spacing();
}

// Decides whether the current first visible shelf icon of the scrollable shelf
// should be hidden or fully shown when gesture scroll ends.
int GetGestureDragThreshold() {
  return ShelfConfig::Get()->button_size() / 2;
}

bool IsInTabletMode() {
  TabletModeController* tablet_mode_controller =
      Shell::Get()->tablet_mode_controller();

  // TabletModeController is destructed before ScrollableShelfView.
  if (!tablet_mode_controller || !tablet_mode_controller->InTabletMode())
    return false;

  return true;
}

// Returns the padding between the app icon and the end of the ScrollableShelf.
int GetAppIconEndPadding() {
  return (chromeos::switches::ShouldShowShelfHotseat() && IsInTabletMode()) ? 4
                                                                            : 0;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// GradientLayerDelegate

class ScrollableShelfView::GradientLayerDelegate : public ui::LayerDelegate {
 public:
  GradientLayerDelegate() : layer_(ui::LAYER_TEXTURED) {
    layer_.set_delegate(this);
    layer_.SetFillsBoundsOpaquely(false);
  }

  ~GradientLayerDelegate() override { layer_.set_delegate(nullptr); }

  bool IsStartFadeZoneVisible() const {
    return !start_fade_zone_.zone_rect.IsEmpty();
  }
  bool IsEndFadeZoneVisible() const {
    return !end_fade_zone_.zone_rect.IsEmpty();
  }

  void set_start_fade_zone(const FadeZone& fade_zone) {
    start_fade_zone_ = fade_zone;
  }
  void set_end_fade_zone(const FadeZone& fade_zone) {
    end_fade_zone_ = fade_zone;
  }
  ui::Layer* layer() { return &layer_; }

 private:
  // ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override {
    const gfx::Size size = layer()->size();

    views::PaintInfo paint_info =
        views::PaintInfo::CreateRootPaintInfo(context, size);
    const auto& paint_recording_size = paint_info.paint_recording_size();

    // Pass the scale factor when constructing PaintRecorder so the MaskLayer
    // size is not incorrectly rounded (see https://crbug.com/921274).
    ui::PaintRecorder recorder(
        context, paint_info.paint_recording_size(),
        static_cast<float>(paint_recording_size.width()) / size.width(),
        static_cast<float>(paint_recording_size.height()) / size.height(),
        nullptr);

    recorder.canvas()->DrawColor(SK_ColorBLACK, SkBlendMode::kSrc);

    if (!start_fade_zone_.zone_rect.IsEmpty())
      DrawFadeZone(start_fade_zone_, recorder.canvas());
    if (!end_fade_zone_.zone_rect.IsEmpty())
      DrawFadeZone(end_fade_zone_, recorder.canvas());
  }
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {}

  void DrawFadeZone(const FadeZone& fade_zone, gfx::Canvas* canvas) {
    gfx::Point start_point;
    gfx::Point end_point;
    if (fade_zone.is_horizontal) {
      start_point = gfx::Point(fade_zone.zone_rect.x(), 0);
      end_point = gfx::Point(fade_zone.zone_rect.right(), 0);
    } else {
      start_point = gfx::Point(0, fade_zone.zone_rect.y());
      end_point = gfx::Point(0, fade_zone.zone_rect.bottom());
    }

    cc::PaintFlags flags;
    flags.setBlendMode(SkBlendMode::kSrc);
    flags.setAntiAlias(false);

    flags.setShader(gfx::CreateGradientShader(
        start_point, end_point,
        fade_zone.fade_in ? SK_ColorTRANSPARENT : SK_ColorBLACK,
        fade_zone.fade_in ? SK_ColorBLACK : SK_ColorTRANSPARENT));

    canvas->DrawRect(fade_zone.zone_rect, flags);
  }

  ui::Layer layer_;

  FadeZone start_fade_zone_;
  FadeZone end_fade_zone_;

  DISALLOW_COPY_AND_ASSIGN(GradientLayerDelegate);
};

////////////////////////////////////////////////////////////////////////////////
// ScrollableShelfArrowView

class ScrollableShelfView::ScrollableShelfArrowView
    : public ash::ScrollArrowView,
      public views::ViewTargeterDelegate {
 public:
  explicit ScrollableShelfArrowView(ArrowType arrow_type,
                                    bool is_horizontal_alignment,
                                    Shelf* shelf,
                                    ShelfButtonDelegate* shelf_button_delegate)
      : ScrollArrowView(arrow_type,
                        is_horizontal_alignment,
                        shelf,
                        shelf_button_delegate) {
    SetEventTargeter(std::make_unique<views::ViewTargeter>(this));
  }
  ~ScrollableShelfArrowView() override = default;

  // views::ViewTargeterDelegate:
  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override {
    DCHECK_EQ(target, this);
    gfx::Rect bounds = gfx::Rect(size());

    // Calculate padding for the tap area. (Should be 32 x shelfButtonSize)
    constexpr int horizontalPadding =
        (kArrowButtonTapAreaHorizontal - kArrowButtonSize) / 2;
    const int verticalPadding =
        (ShelfConfig::Get()->button_size() - kArrowButtonSize) / 2;
    bounds.Inset(gfx::Insets(-verticalPadding, -horizontalPadding));
    return bounds.Intersects(rect);
  }
};

////////////////////////////////////////////////////////////////////////////////
// ScrollableShelfContainerView

class ScrollableShelfContainerView : public ShelfContainerView,
                                     public views::ViewTargeterDelegate {
 public:
  explicit ScrollableShelfContainerView(
      ScrollableShelfView* scrollable_shelf_view)
      : ShelfContainerView(scrollable_shelf_view->shelf_view()),
        scrollable_shelf_view_(scrollable_shelf_view) {
    SetEventTargeter(std::make_unique<views::ViewTargeter>(this));
  }
  ~ScrollableShelfContainerView() override = default;

 private:
  // views::View:
  void Layout() override;

  // views::ViewTargeterDelegate:
  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override;

  ScrollableShelfView* scrollable_shelf_view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ScrollableShelfContainerView);
};

void ScrollableShelfContainerView::Layout() {
  // Should not use ShelfView::GetPreferredSize in replace of
  // CalculateIdealSize. Because ShelfView::CalculatePreferredSize relies on the
  // bounds of app icon. Meanwhile, the icon's bounds may be updated by
  // animation.
  const gfx::Rect ideal_bounds = gfx::Rect(CalculateIdealSize());

  const gfx::Rect local_bounds = GetLocalBounds();
  gfx::Rect shelf_view_bounds =
      local_bounds.Contains(ideal_bounds) ? local_bounds : ideal_bounds;
  shelf_view_->SetBoundsRect(shelf_view_bounds);
}

bool ScrollableShelfContainerView::DoesIntersectRect(
    const views::View* target,
    const gfx::Rect& rect) const {
  // This view's layer is clipped. So the view should only handle the events
  // within the area after cilp.

  gfx::RectF bounds = gfx::RectF(scrollable_shelf_view_->visible_space());
  views::View::ConvertRectToTarget(scrollable_shelf_view_, this, &bounds);
  return ToEnclosedRect(bounds).Contains(rect);
}

////////////////////////////////////////////////////////////////////////////////
// ScrollableShelfFocusSearch

class ScrollableShelfFocusSearch : public views::FocusSearch {
 public:
  explicit ScrollableShelfFocusSearch(
      ScrollableShelfView* scrollable_shelf_view)
      : FocusSearch(/*root=*/nullptr,
                    /*cycle=*/true,
                    /*accessibility_mode=*/true),
        scrollable_shelf_view_(scrollable_shelf_view) {}

  ~ScrollableShelfFocusSearch() override = default;

  // views::FocusSearch
  views::View* FindNextFocusableView(
      views::View* starting_view,
      FocusSearch::SearchDirection search_direction,
      FocusSearch::TraversalDirection traversal_direction,
      FocusSearch::StartingViewPolicy check_starting_view,
      FocusSearch::AnchoredDialogPolicy can_go_into_anchored_dialog,
      views::FocusTraversable** focus_traversable,
      views::View** focus_traversable_view) override {
    std::vector<views::View*> focusable_views;
    ShelfView* shelf_view = scrollable_shelf_view_->shelf_view();

    for (int i = shelf_view->first_visible_index();
         i <= shelf_view->last_visible_index(); ++i) {
      focusable_views.push_back(shelf_view->view_model()->view_at(i));
    }

    int start_index = 0;
    for (size_t i = 0; i < focusable_views.size(); ++i) {
      if (focusable_views[i] == starting_view) {
        start_index = i;
        break;
      }
    }

    int new_index =
        start_index +
        (search_direction == FocusSearch::SearchDirection::kBackwards ? -1 : 1);

    // Scrolls to the new page if the focused shelf item is not tappable
    // on the current page.
    if (new_index < 0)
      new_index = focusable_views.size() - 1;
    else if (new_index >= static_cast<int>(focusable_views.size()))
      new_index = 0;
    else if (new_index < scrollable_shelf_view_->first_tappable_app_index())
      scrollable_shelf_view_->ScrollToNewPage(/*forward=*/false);
    else if (new_index > scrollable_shelf_view_->last_tappable_app_index())
      scrollable_shelf_view_->ScrollToNewPage(/*forward=*/true);

    return focusable_views[new_index];
  }

 private:
  ScrollableShelfView* scrollable_shelf_view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ScrollableShelfFocusSearch);
};

////////////////////////////////////////////////////////////////////////////////
// ScrollableShelfView

ScrollableShelfView::ScrollableShelfView(ShelfModel* model, Shelf* shelf)
    : shelf_view_(new ShelfView(model, shelf)) {
  Shell::Get()->AddShellObserver(this);
  set_allow_deactivate_on_esc(true);
}

ScrollableShelfView::~ScrollableShelfView() {
  Shell::Get()->RemoveShellObserver(this);
  GetShelf()->tooltip()->set_shelf_tooltip_delegate(nullptr);
}

void ScrollableShelfView::Init() {
  shelf_view_->Init();

  // Although there is no animation for ScrollableShelfView, a layer is still
  // needed. Otherwise, the child view without its own layer will be painted on
  // RootView and RootView is beneath |opaque_background_| in ShelfWidget. As a
  // result, the child view will not show.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  // Initialize the shelf container view.
  // Note that |shelf_container_view_| should be under the arrow buttons. It
  // ensures that the arrow button receives the tapping events which happen
  // within the overlapping zone between the arrow button's tapping area and
  // the bounds of |shelf_container_view_|.
  shelf_container_view_ =
      AddChildView(std::make_unique<ScrollableShelfContainerView>(this));
  shelf_container_view_->Initialize();

  // Initialize the left arrow button.
  left_arrow_ = AddChildView(std::make_unique<ScrollableShelfArrowView>(
      ScrollArrowView::kLeft, GetShelf()->IsHorizontalAlignment(), GetShelf(),
      this));

  // Initialize the right arrow button.
  right_arrow_ = AddChildView(std::make_unique<ScrollableShelfArrowView>(
      ScrollArrowView::kRight, GetShelf()->IsHorizontalAlignment(), GetShelf(),
      this));

  gradient_layer_delegate_ = std::make_unique<GradientLayerDelegate>();
  layer()->SetMaskLayer(gradient_layer_delegate_->layer());

  focus_search_ = std::make_unique<ScrollableShelfFocusSearch>(this);

  GetShelf()->tooltip()->set_shelf_tooltip_delegate(this);

  set_context_menu_controller(this);
}

void ScrollableShelfView::OnFocusRingActivationChanged(bool activated) {
  if (activated) {
    focus_ring_activated_ = true;
    SetPaneFocusAndFocusDefault();
  } else {
    // Shows the gradient shader when the focus ring is disabled.
    focus_ring_activated_ = false;
  }

  // Not needs to update the gradient areas. Returns early.
  if (layout_strategy_ == kNotShowArrowButtons)
    return;

  UpdateGradientZoneState();
  UpdateGradientZone();
}

void ScrollableShelfView::ScrollToNewPage(bool forward) {
  float offset = CalculatePageScrollingOffset(forward);
  if (GetShelf()->IsHorizontalAlignment())
    ScrollByXOffset(offset, /*animating=*/true);
  else
    ScrollByYOffset(offset, /*animating=*/true);
}

views::FocusSearch* ScrollableShelfView::GetFocusSearch() {
  return focus_search_.get();
}

views::FocusTraversable* ScrollableShelfView::GetFocusTraversableParent() {
  return parent()->GetFocusTraversable();
}

views::View* ScrollableShelfView::GetFocusTraversableParentView() {
  return this;
}

views::View* ScrollableShelfView::GetDefaultFocusableChild() {
  // Adapts |scroll_offset_| to show the view properly right after the focus
  // ring is enabled.

  if (default_last_focusable_child_) {
    scroll_offset_ = GetShelf()->IsHorizontalAlignment()
                         ? gfx::Vector2dF(CalculateScrollUpperBound(), 0)
                         : gfx::Vector2dF(0, CalculateScrollUpperBound());
    Layout();
    return FindLastFocusableChild();
  } else {
    scroll_offset_ = gfx::Vector2dF();
    Layout();
    return FindFirstFocusableChild();
  }
}

gfx::Rect ScrollableShelfView::GetHotseatBackgroundBounds() {
  return available_space_;
}

views::View* ScrollableShelfView::GetShelfContainerViewForTest() {
  return shelf_container_view_;
}

bool ScrollableShelfView::ShouldAdjustForTest() const {
  return CalculateAdjustedOffset();
}

int ScrollableShelfView::CalculateScrollUpperBound() const {
  if (layout_strategy_ == kNotShowArrowButtons)
    return 0;

  // Calculate the length of the available space.
  int available_length = GetSpaceForIcons() - 2 * GetAppIconEndPadding();

  // Calculate the length of the preferred size.
  const gfx::Size shelf_preferred_size(
      shelf_container_view_->GetPreferredSize());
  const int preferred_length =
      (GetShelf()->IsHorizontalAlignment() ? shelf_preferred_size.width()
                                           : shelf_preferred_size.height());

  return std::max(0, preferred_length - available_length);
}

float ScrollableShelfView::CalculateClampedScrollOffset(float scroll) const {
  const float scroll_upper_bound = CalculateScrollUpperBound();
  scroll = base::ClampToRange(scroll, 0.0f, scroll_upper_bound);
  return scroll;
}

void ScrollableShelfView::StartShelfScrollAnimation(float scroll_distance) {
  during_scrolling_animation_ = true;

  // Shows the gradient zones if needed.
  UpdateGradientZoneState();
  if (gradient_layer_delegate_->IsStartFadeZoneVisible() !=
          should_show_start_gradient_zone_ ||
      gradient_layer_delegate_->IsEndFadeZoneVisible() !=
          should_show_end_gradient_zone_) {
    UpdateGradientZone();
  }

  const gfx::Transform current_transform = shelf_view_->GetTransform();
  gfx::Transform reverse_transform = current_transform;
  if (ShouldAdaptToRTL())
    scroll_distance = -scroll_distance;
  if (GetShelf()->IsHorizontalAlignment())
    reverse_transform.Translate(gfx::Vector2dF(scroll_distance, 0));
  else
    reverse_transform.Translate(gfx::Vector2dF(0, scroll_distance));
  shelf_view_->layer()->SetTransform(reverse_transform);
  ui::ScopedLayerAnimationSettings animation_settings(
      shelf_view_->layer()->GetAnimator());
  animation_settings.SetTweenType(gfx::Tween::EASE_OUT);
  animation_settings.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_SET_NEW_TARGET);
  animation_settings.AddObserver(this);
  shelf_view_->layer()->SetTransform(current_transform);
}

void ScrollableShelfView::UpdateLayoutStrategy() {
  if (CanFitAllAppsWithoutScrolling()) {
    layout_strategy_ = kNotShowArrowButtons;
    return;
  }

  int scroll_length = GetShelf()->IsHorizontalAlignment() ? scroll_offset_.x()
                                                          : scroll_offset_.y();
  if (scroll_length == 0) {
    // No invisible shelf buttons at the left side. So hide the left button.
    layout_strategy_ = kShowRightArrowButton;
  } else if (scroll_length == CalculateScrollUpperBound()) {
    // If there is no invisible shelf button at the right side, hide the right
    // button.
    layout_strategy_ = kShowLeftArrowButton;
  } else {
    // There are invisible shelf buttons at both sides. So show two buttons.
    layout_strategy_ = kShowButtons;
  }
}

bool ScrollableShelfView::ShouldAdaptToRTL() const {
  return base::i18n::IsRTL() && GetShelf()->IsHorizontalAlignment();
}

bool ScrollableShelfView::ShouldApplyDisplayCentering() const {
  if (!CanFitAllAppsWithoutScrolling())
    return false;

  const gfx::Rect display_bounds =
      screen_util::GetDisplayBoundsWithShelf(GetWidget()->GetNativeWindow());
  const int display_size_primary = GetShelf()->PrimaryAxisValue(
      display_bounds.width(), display_bounds.height());
  StatusAreaWidget* status_widget =
      GetShelf()->shelf_widget()->status_area_widget();
  const int status_widget_size = GetShelf()->PrimaryAxisValue(
      status_widget->GetWindowBoundsInScreen().width(),
      status_widget->GetWindowBoundsInScreen().height());

  // An easy way to check whether the apps fit at the exact center of the
  // screen is to imagine that we have another status widget on the other
  // side (the status widget is always bigger than the home button plus
  // the back button if applicable) and see if the apps can fit in the middle
  int available_space_for_screen_centering =
      display_size_primary -
      2 * (status_widget_size + ShelfConfig::Get()->app_icon_group_margin());
  return available_space_for_screen_centering >
         shelf_view_->GetSizeOfAppIcons(shelf_view_->number_of_visible_apps(),
                                        false);
}

Shelf* ScrollableShelfView::GetShelf() {
  return const_cast<Shelf*>(
      const_cast<const ScrollableShelfView*>(this)->GetShelf());
}

const Shelf* ScrollableShelfView::GetShelf() const {
  return shelf_view_->shelf();
}

gfx::Size ScrollableShelfView::CalculatePreferredSize() const {
  return shelf_container_view_->GetPreferredSize();
}

void ScrollableShelfView::Layout() {
  const bool is_horizontal = GetShelf()->IsHorizontalAlignment();

  // |padding_insets| includes the app icon group margin.
  const gfx::Insets padding_insets = CalculateEdgePadding();
  available_space_ = GetLocalBounds();
  available_space_.Inset(padding_insets);
  // The hotseat uses |available_space_| to determine where to show its
  // background, so notify it when it is recalculated.
  GetShelf()->shelf_widget()->hotseat_widget()->UpdateOpaqueBackground();

  // The upper bound of scrolling offset may vary due to display rotation.
  // Ensures that |scroll_offset_| is within the legal range.
  if (is_horizontal)
    scroll_offset_.set_x(CalculateClampedScrollOffset(scroll_offset_.x()));
  else
    scroll_offset_.set_y(CalculateClampedScrollOffset(scroll_offset_.y()));

  UpdateLayoutStrategy();

  gfx::Size arrow_button_size(kArrowButtonSize, kArrowButtonSize);
  gfx::Rect shelf_container_bounds = gfx::Rect(size());

  // Transpose and layout as if it is horizontal.
  if (!is_horizontal)
    shelf_container_bounds.Transpose();

  gfx::Size arrow_button_group_size(kArrowButtonGroupWidth,
                                    shelf_container_bounds.height());

  // The bounds of |left_arrow_| and |right_arrow_| in the parent coordinates.
  gfx::Rect left_arrow_bounds;
  gfx::Rect right_arrow_bounds;

  const int before_padding =
      is_horizontal ? padding_insets.left() : padding_insets.top();
  const int after_padding =
      is_horizontal ? padding_insets.right() : padding_insets.bottom();

  // Calculates the bounds of the left arrow button. If the left arrow button
  // should not show, |left_arrow_bounds| should be empty.
  if (layout_strategy_ == kShowLeftArrowButton ||
      layout_strategy_ == kShowButtons) {
    left_arrow_bounds = gfx::Rect(arrow_button_group_size);
    left_arrow_bounds.Offset(before_padding, 0);
    left_arrow_bounds.Inset(kArrowButtonEndPadding, 0, kDistanceToArrowButton,
                            0);
    left_arrow_bounds.ClampToCenteredSize(arrow_button_size);
  }

  if (layout_strategy_ == kShowRightArrowButton ||
      layout_strategy_ == kShowButtons) {
    gfx::Point right_arrow_start_point(
        shelf_container_bounds.right() - after_padding - kArrowButtonGroupWidth,
        0);
    right_arrow_bounds =
        gfx::Rect(right_arrow_start_point, arrow_button_group_size);
    right_arrow_bounds.Inset(kDistanceToArrowButton, 0, kArrowButtonEndPadding,
                             0);
    right_arrow_bounds.ClampToCenteredSize(arrow_button_size);
  }

  // Paddings are within the shelf view. It makes sure that |shelf_view_|'s
  // bounds are never changed.
  shelf_view_->set_app_icons_layout_offset(before_padding +
                                           GetAppIconEndPadding());

  // Adjust the bounds when not showing in the horizontal
  // alignment.tShelf()->IsHorizontalAlignment()) {
  if (!is_horizontal) {
    left_arrow_bounds.Transpose();
    right_arrow_bounds.Transpose();
    shelf_container_bounds.Transpose();
  }

  const bool is_left_arrow_changed =
      (left_arrow_->bounds() != left_arrow_bounds) ||
      (!left_arrow_bounds.IsEmpty() && !left_arrow_->GetVisible());
  const bool is_right_arrow_changed =
      (right_arrow_->bounds() != right_arrow_bounds) ||
      (!right_arrow_bounds.IsEmpty() && !right_arrow_->GetVisible());

  // Layout |left_arrow| if it should show.
  left_arrow_->SetVisible(!left_arrow_bounds.IsEmpty());
  if (left_arrow_->GetVisible())
    left_arrow_->SetBoundsRect(left_arrow_bounds);

  // Layout |right_arrow| if it should show.
  right_arrow_->SetVisible(!right_arrow_bounds.IsEmpty());
  if (right_arrow_->GetVisible())
    right_arrow_->SetBoundsRect(right_arrow_bounds);

  if (gradient_layer_delegate_->layer()->bounds() != layer()->bounds())
    gradient_layer_delegate_->layer()->SetBounds(layer()->bounds());

  // Fade zones should be updated if:
  // (1) Fade zone's visibility changes.
  // (2) Fade zone should show and the arrow button's location changes.
  UpdateGradientZoneState();
  const bool should_update_end_fade_zone =
      (should_show_end_gradient_zone_ !=
       gradient_layer_delegate_->IsEndFadeZoneVisible()) ||
      (should_show_end_gradient_zone_ && is_right_arrow_changed);
  const bool should_update_start_fade_zone =
      (should_show_start_gradient_zone_ !=
       gradient_layer_delegate_->IsStartFadeZoneVisible()) ||
      (should_show_start_gradient_zone_ && is_left_arrow_changed);

  // Bounds of the gradient zone are relative to the location of arrow buttons.
  // So updates the gradient zone after the bounds of arrow buttons are set.
  if (should_update_start_fade_zone || should_update_end_fade_zone)
    UpdateGradientZone();

  // Layout |shelf_container_view_|.
  shelf_container_view_->SetBoundsRect(shelf_container_bounds);

  // Updates the clip rectangle of |shelf_container_view_|. Note that
  // |shelf_container_view_|'s bounds are the same with ScrollableShelfView's.
  // It is why we can use |visible_space_| directly without coordinate
  // transformation.
  UpdateVisibleSpace();
  shelf_container_view_->layer()->SetClipRect(visible_space_);
  if (IsInTabletMode() && chromeos::switches::ShouldShowShelfHotseat()) {
    shelf_container_view_->layer()->SetRoundedCornerRadius(
        CalculateShelfContainerRoundedCorners());
  }

  gfx::Vector2dF total_offset = scroll_offset_;
  if (ShouldAdaptToRTL())
    total_offset = -total_offset;

  shelf_container_view_->TranslateShelfView(total_offset);

  UpdateTappableIconIndices();

  // Ensures that the app icons are shown correctly when the scrollable shelf
  // is idle (not under scrolling or during animation process).
  if (!during_scrolling_animation_ && scroll_status_ == kNotInScroll)
    AdjustOffset();
}

void ScrollableShelfView::ChildPreferredSizeChanged(views::View* child) {
  // Add/remove a shelf icon may change the layout strategy.
  Layout();
}

void ScrollableShelfView::OnMouseEvent(ui::MouseEvent* event) {
  if (event->IsMouseWheelEvent()) {
    HandleMouseWheelEvent(event->AsMouseWheelEvent());
    return;
  }

  // The mouse event's location may be outside of ShelfView but within the
  // bounds of the ScrollableShelfView. Meanwhile, ScrollableShelfView should
  // handle the mouse event consistently with ShelfView. To achieve this,
  // we simply redirect |event| to ShelfView.
  gfx::Point location_in_shelf_view = event->location();
  View::ConvertPointToTarget(this, shelf_view_, &location_in_shelf_view);
  event->set_location(location_in_shelf_view);
  shelf_view_->OnMouseEvent(event);
}

void ScrollableShelfView::OnGestureEvent(ui::GestureEvent* event) {
  if (ShouldHandleGestures(*event))
    HandleGestureEvent(event);
  else if (shelf_view_->HandleGestureEvent(event))
    event->StopPropagation();
}

const char* ScrollableShelfView::GetClassName() const {
  return "ScrollableShelfView";
}

void ScrollableShelfView::OnShelfButtonAboutToRequestFocusFromTabTraversal(
    ShelfButton* button,
    bool reverse) {}

void ScrollableShelfView::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  // |point| is in screen coordinates. So it does not need to transform.
  shelf_view_->ShowContextMenuForViewImpl(shelf_view_, point, source_type);
}

void ScrollableShelfView::ButtonPressed(views::Button* sender,
                                        const ui::Event& event,
                                        views::InkDrop* ink_drop) {
  // Verfies that |sender| is either |left_arrow_| or |right_arrow_|.
  views::View* sender_view = sender;
  DCHECK((sender_view == left_arrow_) || (sender_view == right_arrow_));

  ScrollToNewPage(sender_view == right_arrow_);
}

void ScrollableShelfView::OnShelfAlignmentChanged(aura::Window* root_window) {
  const bool is_horizontal_alignment = GetShelf()->IsHorizontalAlignment();
  left_arrow_->set_is_horizontal_alignment(is_horizontal_alignment);
  right_arrow_->set_is_horizontal_alignment(is_horizontal_alignment);
  scroll_offset_ = gfx::Vector2dF();
  Layout();
}

bool ScrollableShelfView::ShouldShowTooltipForView(
    const views::View* view) const {
  if (!view || !view->parent())
    return false;

  if (view == left_arrow_ || view == right_arrow_)
    return true;

  if (view->parent() != shelf_view_)
    return false;

  // The shelf item corresponding to |view| may have been removed from the
  // model.
  if (!shelf_view_->ShouldShowTooltipForChildView(view))
    return false;

  const gfx::Rect screen_bounds = view->GetBoundsInScreen();
  gfx::Rect visible_bounds_in_screen = visible_space_;
  views::View::ConvertRectToScreen(this, &visible_bounds_in_screen);

  return visible_bounds_in_screen.Contains(screen_bounds);
}

bool ScrollableShelfView::ShouldHideTooltip(
    const gfx::Point& cursor_location) const {
  return !visible_space_.Contains(cursor_location);
}

const std::vector<aura::Window*> ScrollableShelfView::GetOpenWindowsForView(
    views::View* view) {
  if (!view || view->parent() != shelf_view_)
    return std::vector<aura::Window*>();

  return shelf_view_->GetOpenWindowsForView(view);
}

base::string16 ScrollableShelfView::GetTitleForView(
    const views::View* view) const {
  if (!view || !view->parent())
    return base::string16();

  if (view->parent() == shelf_view_)
    return shelf_view_->GetTitleForView(view);

  if (view == left_arrow_)
    return l10n_util::GetStringUTF16(IDS_SHELF_PREVIOUS);

  if (view == right_arrow_)
    return l10n_util::GetStringUTF16(IDS_SHELF_NEXT);

  return base::string16();
}

views::View* ScrollableShelfView::GetViewForEvent(const ui::Event& event) {
  if (event.target() == GetWidget()->GetNativeWindow())
    return this;

  return nullptr;
}

void ScrollableShelfView::OnImplicitAnimationsCompleted() {
  during_scrolling_animation_ = false;
  Layout();
}

gfx::Insets ScrollableShelfView::CalculateEdgePadding() const {
  // Display centering alignment
  if (ShouldApplyDisplayCentering())
    return CalculatePaddingForDisplayCentering();

  const int icons_size = shelf_view_->GetSizeOfAppIcons(
                             shelf_view_->number_of_visible_apps(), false) +
                         2 * GetAppIconEndPadding();
  const int base_padding = ShelfConfig::Get()->app_icon_group_margin();

  const int available_size_for_app_icons =
      (GetShelf()->IsHorizontalAlignment() ? width() : height()) -
      2 * ShelfConfig::Get()->app_icon_group_margin();

  int gap = CanFitAllAppsWithoutScrolling()
                ? available_size_for_app_icons - icons_size  // shelf centering
                : 0;                                         // overflow

  // Calculates the paddings before/after the visible area of scrollable shelf.
  const int before_padding = base_padding + gap / 2;
  const int after_padding = base_padding + (gap % 2 ? gap / 2 + 1 : gap / 2);

  gfx::Insets padding_insets;
  if (GetShelf()->IsHorizontalAlignment()) {
    padding_insets =
        gfx::Insets(/*top=*/0, before_padding, /*bottom=*/0, after_padding);
  } else {
    padding_insets =
        gfx::Insets(before_padding, /*left=*/0, after_padding, /*right=*/0);
  }

  return padding_insets;
}

gfx::Insets ScrollableShelfView::CalculatePaddingForDisplayCentering() const {
  const int icons_size = shelf_view_->GetSizeOfAppIcons(
                             shelf_view_->number_of_visible_apps(), false) +
                         2 * GetAppIconEndPadding();
  const gfx::Rect display_bounds =
      screen_util::GetDisplayBoundsWithShelf(GetWidget()->GetNativeWindow());
  const int display_size_primary = GetShelf()->PrimaryAxisValue(
      display_bounds.width(), display_bounds.height());
  const int gap = (display_size_primary - icons_size) / 2;

  // Calculates paddings in view coordinates.
  const gfx::Rect screen_bounds = GetBoundsInScreen();
  const int before_padding = gap - GetShelf()->PrimaryAxisValue(
                                       screen_bounds.x() - display_bounds.x(),
                                       screen_bounds.y() - display_bounds.y());
  const int after_padding =
      gap - GetShelf()->PrimaryAxisValue(
                display_bounds.right() - screen_bounds.right(),
                display_bounds.bottom() - screen_bounds.bottom());

  gfx::Insets padding_insets;
  if (GetShelf()->IsHorizontalAlignment()) {
    padding_insets =
        gfx::Insets(/*top=*/0, before_padding, /*bottom=*/0, after_padding);
  } else {
    padding_insets =
        gfx::Insets(before_padding, /*left=*/0, after_padding, /*right=*/0);
  }

  return padding_insets;
}

bool ScrollableShelfView::ShouldHandleGestures(const ui::GestureEvent& event) {
  // ScrollableShelfView only handles the gesture scrolling along the main axis.
  // For other gesture events, including the scrolling across the main axis,
  // they are handled by ShelfView.

  if (scroll_status_ == kNotInScroll && !event.IsScrollGestureEvent())
    return false;

  if (event.type() == ui::ET_GESTURE_SCROLL_BEGIN) {
    CHECK_EQ(scroll_status_, kNotInScroll);

    float main_offset = event.details().scroll_x_hint();
    float cross_offset = event.details().scroll_y_hint();
    if (!GetShelf()->IsHorizontalAlignment())
      std::swap(main_offset, cross_offset);

    scroll_status_ = std::abs(main_offset) < std::abs(cross_offset)
                         ? kAcrossMainAxisScroll
                         : kAlongMainAxisScroll;
  }

  bool should_handle_gestures = scroll_status_ == kAlongMainAxisScroll;

  if (scroll_status_ == kAlongMainAxisScroll &&
      event.type() == ui::ET_GESTURE_SCROLL_BEGIN) {
    scroll_offset_before_main_axis_scrolling_ = scroll_offset_;
    layout_strategy_before_main_axis_scrolling_ = layout_strategy_;
  }

  if (event.type() == ui::ET_GESTURE_END) {
    scroll_status_ = kNotInScroll;

    if (should_handle_gestures) {
      scroll_offset_before_main_axis_scrolling_ = gfx::Vector2dF();
      layout_strategy_before_main_axis_scrolling_ = kNotShowArrowButtons;
    }
  }

  return should_handle_gestures;
}

void ScrollableShelfView::HandleGestureEvent(ui::GestureEvent* event) {
  if (ProcessGestureEvent(*event))
    event->SetHandled();
}

bool ScrollableShelfView::ProcessGestureEvent(const ui::GestureEvent& event) {
  if (layout_strategy_ == kNotShowArrowButtons)
    return true;

  // Handle scroll-related events, but don't do anything special for begin and
  // end.
  if (event.type() == ui::ET_GESTURE_SCROLL_BEGIN ||
      event.type() == ui::ET_GESTURE_SCROLL_END) {
    return true;
  }

  if (event.type() == ui::ET_GESTURE_END) {
    AdjustOffset();
    return true;
  }

  if (event.type() == ui::ET_SCROLL_FLING_START) {
    const bool is_horizontal_alignment = GetShelf()->IsHorizontalAlignment();
    if (!ShouldHandleScroll(gfx::Vector2dF(event.details().velocity_x(),
                                           event.details().velocity_y()),
                            /*is_gesture_fling=*/true)) {
      return false;
    }

    layout_strategy_ = layout_strategy_before_main_axis_scrolling_;
    const int scroll_velocity = is_horizontal_alignment
                                    ? event.details().velocity_x()
                                    : event.details().velocity_y();
    float page_scrolling_offset =
        CalculatePageScrollingOffset(scroll_velocity < 0);
    if (is_horizontal_alignment) {
      ScrollToXOffset(
          scroll_offset_before_main_axis_scrolling_.x() + page_scrolling_offset,
          true);
    } else {
      ScrollToYOffset(
          scroll_offset_before_main_axis_scrolling_.y() + page_scrolling_offset,
          true);
    }

    return true;
  }

  if (event.type() != ui::ET_GESTURE_SCROLL_UPDATE)
    return false;

  if (GetShelf()->IsHorizontalAlignment())
    ScrollByXOffset(-event.details().scroll_x(), /*animate=*/false);
  else
    ScrollByYOffset(-event.details().scroll_y(), /*animate=*/false);
  return true;
}

void ScrollableShelfView::HandleMouseWheelEvent(ui::MouseWheelEvent* event) {
  // Note that the scrolling from touchpad is propagated as mouse wheel event.

  // When shelf is horizontally aligned, the mouse wheel event may be handled
  // by AppList.
  if (!ShouldHandleScroll(gfx::Vector2dF(event->x_offset(), event->y_offset()),
                          /*is_gesture_fling=*/false) &&
      GetShelf()->IsHorizontalAlignment()) {
    GetShelf()->ProcessMouseWheelEvent(event);
    return;
  }

  event->SetHandled();

  // Scrolling the mouse wheel may create multiple mouse wheel events at the
  // same time. If the scrollable shelf view is during scrolling animation at
  // this moment, do not handle the mouse wheel event.
  if (shelf_view_->layer()->GetAnimator()->is_animating())
    return;

  if (GetShelf()->IsHorizontalAlignment()) {
    ScrollByXOffset(CalculatePageScrollingOffset(event->x_offset() < 0),
                    /*animating=*/true);
  } else {
    ScrollByYOffset(CalculatePageScrollingOffset(event->y_offset() < 0),
                    /*animating=*/true);
  }
}

void ScrollableShelfView::ScrollByXOffset(float x_offset, bool animating) {
  ScrollToXOffset(scroll_offset_.x() + x_offset, animating);
}

void ScrollableShelfView::ScrollByYOffset(float y_offset, bool animating) {
  ScrollToYOffset(scroll_offset_.y() + y_offset, animating);
}

void ScrollableShelfView::ScrollToXOffset(float x_target_offset,
                                          bool animating) {
  x_target_offset = CalculateClampedScrollOffset(x_target_offset);
  const float old_x = scroll_offset_.x();
  scroll_offset_.set_x(x_target_offset);
  Layout();
  const float diff = x_target_offset - old_x;

  if (animating)
    StartShelfScrollAnimation(diff);
}

void ScrollableShelfView::ScrollToYOffset(float y_target_offset,
                                          bool animating) {
  y_target_offset = CalculateClampedScrollOffset(y_target_offset);
  const int old_y = scroll_offset_.y();
  scroll_offset_.set_y(y_target_offset);
  Layout();
  const float diff = y_target_offset - old_y;
  if (animating)
    StartShelfScrollAnimation(diff);
}

float ScrollableShelfView::CalculatePageScrollingOffset(bool forward) const {
  DCHECK_NE(kNotShowArrowButtons, layout_strategy_);

  // Implement the arrow button handler in the same way with the gesture
  // scrolling. The key is to calculate the suitable scroll distance.

  float offset;

  // The available space for icons excluding the area taken by arrow button(s).
  int space_excluding_arrow;

  if (layout_strategy_ == kShowRightArrowButton) {
    space_excluding_arrow = GetSpaceForIcons() - kArrowButtonGroupWidth;

    // After scrolling, the left arrow button will show. Adapts the offset
    // to the extra arrow button.
    const int offset_for_extra_arrow =
        kArrowButtonGroupWidth - GetAppIconEndPadding();

    const int mod = space_excluding_arrow % GetUnit();
    offset = space_excluding_arrow - mod - offset_for_extra_arrow;
  } else if (layout_strategy_ == kShowButtons ||
             layout_strategy_ == kShowLeftArrowButton) {
    space_excluding_arrow = GetSpaceForIcons() - 2 * kArrowButtonGroupWidth;
    const int mod = space_excluding_arrow % GetUnit();
    offset = space_excluding_arrow - mod;

    // Layout of kShowLeftArrowButton can be regarded as the layout of
    // kShowButtons with extra offset.
    if (layout_strategy_ == kShowLeftArrowButton) {
      const int extra_offset =
          -ShelfConfig::Get()->button_spacing() -
          (GetSpaceForIcons() - kArrowButtonGroupWidth) % GetUnit() +
          GetAppIconEndPadding();
      offset += extra_offset;
    }
  }

  DCHECK_GT(offset, 0);

  if (!forward)
    offset = -offset;

  return offset;
}

void ScrollableShelfView::UpdateGradientZone() {
  gradient_layer_delegate_->set_start_fade_zone(CalculateStartGradientZone());
  gradient_layer_delegate_->set_end_fade_zone(CalculateEndGradientZone());

  SchedulePaint();
}

ScrollableShelfView::FadeZone ScrollableShelfView::CalculateStartGradientZone()
    const {
  gfx::Rect zone_rect;
  bool fade_in = false;
  const bool is_horizontal_alignment = GetShelf()->IsHorizontalAlignment();
  const gfx::Rect left_arrow_bounds = left_arrow_->bounds();

  if (!should_show_start_gradient_zone_)
    return FadeZone();

  if (is_horizontal_alignment) {
    int x;

    // Calculates the start location on x-axis of the gradient zone.
    if (ShouldAdaptToRTL()) {
      const gfx::Rect mirrored_left_arrow_bounds =
          GetMirroredRect(left_arrow_bounds);
      x = mirrored_left_arrow_bounds.x() - kGradientZoneLength;
    } else {
      x = left_arrow_bounds.right();
    }
    zone_rect = gfx::Rect(x, 0, kGradientZoneLength, height());
  } else {
    zone_rect =
        gfx::Rect(0, left_arrow_bounds.bottom(), width(), kGradientZoneLength);
  }

  fade_in = !ShouldAdaptToRTL();

  return {zone_rect, fade_in, is_horizontal_alignment};
}

ScrollableShelfView::FadeZone ScrollableShelfView::CalculateEndGradientZone()
    const {
  gfx::Rect zone_rect;
  bool fade_in = false;
  const bool is_horizontal_alignment = GetShelf()->IsHorizontalAlignment();
  const gfx::Rect right_arrow_bounds = right_arrow_->bounds();

  if (!should_show_end_gradient_zone_)
    return FadeZone();

  if (is_horizontal_alignment) {
    int x;

    // Calculates the start location on x-axis of the gradient zone.
    if (ShouldAdaptToRTL()) {
      const gfx::Rect mirrored_right_arrow_bounds =
          GetMirroredRect(right_arrow_bounds);
      x = mirrored_right_arrow_bounds.right();
    } else {
      x = right_arrow_bounds.x() - kGradientZoneLength;
    }
    zone_rect = gfx::Rect(x, 0, kGradientZoneLength, height());
  } else {
    zone_rect = gfx::Rect(0, right_arrow_bounds.y() - kGradientZoneLength,
                          width(), kGradientZoneLength);
  }

  fade_in = ShouldAdaptToRTL();

  return {zone_rect, fade_in, is_horizontal_alignment};
}

void ScrollableShelfView::UpdateGradientZoneState() {
  // The gradient zone is not painted when the focus ring shows in order to
  // display the focus ring correctly.
  if (focus_ring_activated_) {
    should_show_start_gradient_zone_ = false;
    should_show_end_gradient_zone_ = false;
    return;
  }

  if (during_scrolling_animation_) {
    should_show_start_gradient_zone_ = left_arrow_->GetVisible();
    should_show_end_gradient_zone_ = right_arrow_->GetVisible();
    return;
  }

  should_show_start_gradient_zone_ = layout_strategy_ == kShowLeftArrowButton ||
                                     (layout_strategy_ == kShowButtons &&
                                      scroll_status_ == kAlongMainAxisScroll);
  should_show_end_gradient_zone_ = right_arrow_->GetVisible();
}

int ScrollableShelfView::GetActualScrollOffset() const {
  int scroll_distance = GetShelf()->IsHorizontalAlignment()
                            ? scroll_offset_.x()
                            : scroll_offset_.y();
  if (left_arrow_->GetVisible())
    scroll_distance += (kArrowButtonGroupWidth - GetAppIconEndPadding());
  return scroll_distance;
}

void ScrollableShelfView::UpdateTappableIconIndices() {
  if (layout_strategy_ == kNotShowArrowButtons) {
    first_tappable_app_index_ = shelf_view_->first_visible_index();
    last_tappable_app_index_ = shelf_view_->last_visible_index();
    return;
  }

  const int scroll_distance_on_main_axis = GetShelf()->IsHorizontalAlignment()
                                               ? scroll_offset_.x()
                                               : scroll_offset_.y();
  const int visible_size = GetShelf()->IsHorizontalAlignment()
                               ? visible_space_.width()
                               : visible_space_.height();

  if (layout_strategy_ == kShowRightArrowButton ||
      layout_strategy_ == kShowButtons) {
    first_tappable_app_index_ = scroll_distance_on_main_axis / GetUnit() +
                                (layout_strategy_ == kShowButtons ? 1 : 0);
    last_tappable_app_index_ =
        first_tappable_app_index_ + visible_size / GetUnit();

    const int end_of_last_tappable_app = last_tappable_app_index_ * GetUnit() +
                                         ShelfConfig::Get()->button_size() -
                                         scroll_distance_on_main_axis;
    if (end_of_last_tappable_app > visible_size)
      last_tappable_app_index_--;
  } else {
    DCHECK_EQ(layout_strategy_, kShowLeftArrowButton);
    last_tappable_app_index_ = shelf_view_->last_visible_index();
    first_tappable_app_index_ =
        last_tappable_app_index_ - visible_size / GetUnit() + 1;
  }
}

views::View* ScrollableShelfView::FindFirstFocusableChild() {
  if (shelf_view_->view_model()->view_size() == 0)
    return nullptr;

  return shelf_view_->view_model()->view_at(shelf_view_->first_visible_index());
}

views::View* ScrollableShelfView::FindLastFocusableChild() {
  if (shelf_view_->view_model()->view_size() == 0)
    return nullptr;

  return shelf_view_->view_model()->view_at(shelf_view_->last_visible_index());
}

int ScrollableShelfView::GetSpaceForIcons() const {
  return GetShelf()->IsHorizontalAlignment() ? available_space_.width()
                                             : available_space_.height();
}

bool ScrollableShelfView::CanFitAllAppsWithoutScrolling() const {
  const int available_length =
      (GetShelf()->IsHorizontalAlignment() ? width() : height()) -
      2 * ShelfConfig::Get()->app_icon_group_margin();

  gfx::Size preferred_size = GetPreferredSize();
  int preferred_length = GetShelf()->IsHorizontalAlignment()
                             ? preferred_size.width()
                             : preferred_size.height();
  preferred_length += 2 * GetAppIconEndPadding();

  return available_length >= preferred_length;
}

bool ScrollableShelfView::ShouldHandleScroll(const gfx::Vector2dF& offset,
                                             bool is_gesture_scrolling) const {
  const float main_axis_offset =
      GetShelf()->IsHorizontalAlignment() ? offset.x() : offset.y();

  const int threshold = is_gesture_scrolling ? kGestureFlingVelocityThreshold
                                             : KScrollOffsetThreshold;
  return abs(main_axis_offset) > threshold;
}

void ScrollableShelfView::AdjustOffset() {
  // The type of scrolling offset is float to ensure that ScrollableShelfView
  // is responsive to slow gesture scrolling. However, after offset adjustment,
  // the scrolling offset should be floored.
  scroll_offset_ = gfx::ToFlooredVector2d(scroll_offset_);

  const int offset = CalculateAdjustedOffset();

  // Returns early when it does not need to adjust the shelf view's location.
  if (!offset)
    return;

  if (GetShelf()->IsHorizontalAlignment())
    ScrollByXOffset(offset, /*animate=*/true);
  else
    ScrollByYOffset(offset, /*animate=*/true);
}

int ScrollableShelfView::CalculateAdjustedOffset() const {
  // Returns early when it does not need to adjust the shelf view's location.
  const int scroll_distance = GetShelf()->IsHorizontalAlignment()
                                  ? scroll_offset_.x()
                                  : scroll_offset_.y();
  if (scroll_distance >= CalculateScrollUpperBound())
    return 0;

  const int remainder = GetActualScrollOffset() % GetUnit();
  int offset = remainder > GetGestureDragThreshold() ? GetUnit() - remainder
                                                     : -remainder;

  return offset;
}

void ScrollableShelfView::UpdateVisibleSpace() {
  const int before_padding =
      (left_arrow_->GetVisible() ? kArrowButtonGroupWidth : 0);
  const int after_padding =
      (right_arrow_->GetVisible() ? kArrowButtonGroupWidth : 0);

  gfx::Insets visible_space_insets;
  if (ShouldAdaptToRTL()) {
    visible_space_insets = gfx::Insets(0, after_padding, 0, before_padding);
  } else {
    visible_space_insets =
        GetShelf()->IsHorizontalAlignment()
            ? gfx::Insets(0, before_padding, 0, after_padding)
            : gfx::Insets(before_padding, 0, after_padding, 0);
  }
  visible_space_insets -= CalculateRipplePaddingInsets();

  visible_space_ =
      ShouldAdaptToRTL() ? available_space_ : GetMirroredRect(available_space_);
  visible_space_.Inset(visible_space_insets);
}

gfx::Insets ScrollableShelfView::CalculateRipplePaddingInsets() const {
  // Indicates whether it is in tablet mode with hotseat enabled.
  const bool in_hotseat_tablet =
      chromeos::switches::ShouldShowShelfHotseat() && IsInTabletMode();

  const int ripple_padding =
      ShelfConfig::Get()->scrollable_shelf_ripple_padding();
  const int before_padding =
      (in_hotseat_tablet && !left_arrow_->GetVisible()) ? 0 : ripple_padding;
  const int after_padding =
      (in_hotseat_tablet && !right_arrow_->GetVisible()) ? 0 : ripple_padding;

  if (ShouldAdaptToRTL())
    return gfx::Insets(0, after_padding, 0, before_padding);

  return GetShelf()->IsHorizontalAlignment()
             ? gfx::Insets(0, before_padding, 0, after_padding)
             : gfx::Insets(before_padding, 0, after_padding, 0);
}

gfx::RoundedCornersF
ScrollableShelfView::CalculateShelfContainerRoundedCorners() const {
  const bool is_horizontal_alignment = GetShelf()->IsHorizontalAlignment();
  const float radius = (is_horizontal_alignment ? height() : width()) / 2.f;

  const int upper_left = left_arrow_->GetVisible() ? 0 : radius;

  int upper_right;
  if (is_horizontal_alignment)
    upper_right = right_arrow_->GetVisible() ? 0 : radius;
  else
    upper_right = left_arrow_->GetVisible() ? 0 : radius;

  const int lower_right = right_arrow_->GetVisible() ? 0 : radius;

  int lower_left;
  if (is_horizontal_alignment)
    lower_left = left_arrow_->GetVisible() ? 0 : radius;
  else
    lower_left = right_arrow_->GetVisible() ? 0 : radius;

  return gfx::RoundedCornersF(upper_left, upper_right, lower_right, lower_left);
}

}  // namespace ash
