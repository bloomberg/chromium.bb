// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/window_selector_item.h"

#include <algorithm>
#include <vector>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/overview/cleanup_animation_observer.h"
#include "ash/wm/overview/overview_animation_type.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/overview/overview_window_drag_controller.h"
#include "ash/wm/overview/rounded_rect_view.h"
#include "ash/wm/overview/scoped_overview_animation_settings.h"
#include "ash/wm/overview/scoped_transform_overview_window.h"
#include "ash/wm/overview/window_grid.h"
#include "ash/wm/overview/window_selector.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "base/auto_reset.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/transform_util.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/window/non_client_view.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/shadow.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

// In the conceptual overview table, the window margin is the space reserved
// around the window within the cell. This margin does not overlap so the
// closest distance between adjacent windows will be twice this amount.
constexpr int kWindowMargin = 5;

// Cover the transformed window including the gaps between the windows with a
// transparent shield to block the input events from reaching the transformed
// window while in overview.
constexpr int kWindowSelectorMargin = kWindowMargin * 2;

// Foreground label color.
constexpr SkColor kLabelColor = SK_ColorWHITE;

// Close button color.
constexpr SkColor kCloseButtonColor = SK_ColorWHITE;

// Label background color once in overview mode.
constexpr SkColor kLabelBackgroundColor = SkColorSetARGB(25, 255, 255, 255);

// Label background color when exiting overview mode.
constexpr SkColor kLabelExitColor = SkColorSetARGB(255, 90, 90, 90);

// Corner radius for the selection tiles.
static int kLabelBackgroundRadius = 2;

// Horizontal padding for the label, on both sides.
constexpr int kHorizontalLabelPaddingDp = 12;

// Height of an item header.
constexpr int kHeaderHeightDp = 40;

// Opacity for dimmed items.
constexpr float kDimmedItemOpacity = 0.5f;

// Opacity for fading out during closing a window.
constexpr float kClosingItemOpacity = 0.8f;

// Opacity for the item header.
constexpr float kHeaderOpacity = (SkColorGetA(kLabelBackgroundColor) / 255.f);

// Duration it takes for the header to shift from opaque header color to
// |kLabelBackgroundColor|.
constexpr int kSelectorColorSlideMilliseconds = 240;

// Duration of background opacity transition for the selected label.
constexpr int kSelectorFadeInMilliseconds = 350;

// Duration of background opacity transition when exiting overview mode.
constexpr int kExitFadeInMilliseconds = 30;

// Before closing a window animate both the window and the caption to shrink by
// this fraction of size.
constexpr float kPreCloseScale = 0.02f;

// Before dragging an overview window, the window will scale up |kPreDragScale|
// to indicate its selection.
constexpr float kDragWindowScale = 0.04f;

// The size in dp of the window icon shown on the overview window next to the
// title.
constexpr gfx::Size kIconSize = gfx::Size(24, 24);

// Values for the old overview ui.
// TODO(crbug.com/782320): Delete these values when the old ui becomes obsolete.
constexpr int kOldHeaderHeightDp = 32;
constexpr int kOldHorizontalLabelPaddingDp = 8;

constexpr int kCloseButtonInkDropInsetDp = 2;

// Shift the close button by |kCloseButtonOffsetDp| so that its image aligns
// with the overview window, but its hit bounds exceeds.
constexpr int kCloseButtonOffsetDp = 8;

// The colors of the close button ripple.
constexpr SkColor kCloseButtonInkDropRippleColor =
    SkColorSetARGBMacro(0x0F, 0xFF, 0xFF, 0xFF);
constexpr SkColor kCloseButtonInkDropRippleHighlightColor =
    SkColorSetARGBMacro(0x14, 0xFF, 0xFF, 0xFF);

// The font delta of the overview window title.
constexpr int kLabelFontDelta = 2;

// Convenience method to fade in a Window with predefined animation settings.
// Note: The fade in animation will occur after a delay where the delay is how
// long the lay out animations take.
void SetupFadeInAfterLayout(views::Widget* widget) {
  aura::Window* window = widget->GetNativeWindow();
  window->layer()->SetOpacity(0.0f);
  ScopedOverviewAnimationSettings scoped_overview_animation_settings(
      OverviewAnimationType::OVERVIEW_ANIMATION_ENTER_OVERVIEW_MODE_FADE_IN,
      window);
  window->layer()->SetOpacity(1.0f);
}

// A Button that has a listener and listens to mouse / gesture events on the
// visible part of an overview window. Note that the drag events are only
// handled in maximized mode.
class ShieldButton : public views::Button {
 public:
  ShieldButton(views::ButtonListener* listener, const base::string16& name)
      : views::Button(listener) {
    SetAccessibleName(name);
  }
  ~ShieldButton() override = default;

  // When WindowSelectorItem (which is a ButtonListener) is destroyed, its
  // |item_widget_| is allowed to stay around to complete any animations.
  // Resetting the listener in all views that are targeted by events is
  // necessary to prevent a crash when a user clicks on the fading out widget
  // after the WindowSelectorItem has been destroyed.
  void ResetListener() { listener_ = nullptr; }

  // views::Button:
  bool OnMousePressed(const ui::MouseEvent& event) override {
    if (listener() && SplitViewController::ShouldAllowSplitView()) {
      gfx::Point location(event.location());
      views::View::ConvertPointToScreen(this, &location);
      listener()->HandlePressEvent(location);
      return true;
    }
    return views::Button::OnMousePressed(event);
  }

  void OnMouseReleased(const ui::MouseEvent& event) override {
    if (listener() && SplitViewController::ShouldAllowSplitView()) {
      gfx::Point location(event.location());
      views::View::ConvertPointToScreen(this, &location);
      listener()->HandleReleaseEvent(location);
      return;
    }
    views::Button::OnMouseReleased(event);
  }

  bool OnMouseDragged(const ui::MouseEvent& event) override {
    if (listener() && SplitViewController::ShouldAllowSplitView()) {
      gfx::Point location(event.location());
      views::View::ConvertPointToScreen(this, &location);
      listener()->HandleDragEvent(location);
      return true;
    }
    return views::Button::OnMouseDragged(event);
  }

  void OnGestureEvent(ui::GestureEvent* event) override {
    if (listener() && SplitViewController::ShouldAllowSplitView()) {
      gfx::Point location(event->location());
      views::View::ConvertPointToScreen(this, &location);
      switch (event->type()) {
        case ui::ET_GESTURE_TAP_DOWN:
          listener()->HandlePressEvent(location);
          break;
        case ui::ET_GESTURE_SCROLL_UPDATE:
          listener()->HandleDragEvent(location);
          break;
        case ui::ET_SCROLL_FLING_START:
        case ui::ET_GESTURE_SCROLL_END:
          listener()->HandleReleaseEvent(location);
          break;
        case ui::ET_GESTURE_TAP:
          listener()->ActivateDraggedWindow();
          break;
        case ui::ET_GESTURE_END:
          listener()->ResetDraggedWindowGesture();
          break;
        default:
          break;
      }
      event->SetHandled();
      return;
    }
    views::Button::OnGestureEvent(event);
  }

  WindowSelectorItem* listener() {
    return static_cast<WindowSelectorItem*>(listener_);
  }

 protected:
  // views::View:
  const char* GetClassName() const override { return "ShieldButton"; }

 private:
  DISALLOW_COPY_AND_ASSIGN(ShieldButton);
};

}  // namespace

WindowSelectorItem::OverviewCloseButton::OverviewCloseButton(
    views::ButtonListener* listener)
    : views::ImageButton(listener) {
  if (IsNewOverviewUi())
    SetInkDropMode(InkDropMode::ON_NO_GESTURE_HANDLER);

  SetImage(views::Button::STATE_NORMAL,
           gfx::CreateVectorIcon(IsNewOverviewUi() ? kOverviewWindowCloseIcon
                                                   : kWindowControlCloseIcon,
                                 kCloseButtonColor));
  SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                    views::ImageButton::ALIGN_MIDDLE);
  const int length = IsNewOverviewUi() ? kHeaderHeightDp : kOldHeaderHeightDp;
  SetMinimumImageSize(gfx::Size(length, length));
}

WindowSelectorItem::OverviewCloseButton::~OverviewCloseButton() = default;

std::unique_ptr<views::InkDropRipple>
WindowSelectorItem::OverviewCloseButton::CreateInkDropRipple() const {
  return std::make_unique<views::FloodFillInkDropRipple>(
      size(), gfx::Insets(), GetInkDropCenterBasedOnLastEvent(),
      kCloseButtonInkDropRippleColor, /*visible_opacity=*/1.f);
}

std::unique_ptr<views::InkDropHighlight>
WindowSelectorItem::OverviewCloseButton::CreateInkDropHighlight() const {
  return std::make_unique<views::InkDropHighlight>(
      gfx::PointF(GetLocalBounds().CenterPoint()),
      std::make_unique<views::CircleLayerDelegate>(
          kCloseButtonInkDropRippleHighlightColor, GetInkDropRadius()));
}

std::unique_ptr<views::InkDropMask>
WindowSelectorItem::OverviewCloseButton::CreateInkDropMask() const {
  return std::make_unique<views::CircleInkDropMask>(
      size(), GetLocalBounds().CenterPoint(), GetInkDropRadius());
}

int WindowSelectorItem::OverviewCloseButton::GetInkDropRadius() const {
  return std::min(size().width(), size().height()) / 2 -
         kCloseButtonInkDropInsetDp;
}

// A View having rounded top corners and a specified background color which is
// only painted within the bounds defined by the rounded corners.
// This class coordinates the transitions of the overview mode header when
// entering the overview mode. Those animations are:
// - Opacity animation. The header is initially same color as the original
//   window's header. It starts as transparent and is faded in. When the full
//   opacity is reached the original header is hidden (which is nearly
//   imperceptable because this view obscures the original header) and a color
//   animation starts.
// - Color animation is used to change the color from the opaque color of the
//   original window's header to semi-transparent color of the overview mode
//   header (on entry to overview). It is also used on exit from overview to
//   quickly change the color to a close opaque color in parallel with an
//   opacity transition to mask the original header reappearing.
class WindowSelectorItem::RoundedContainerView
    : public views::View,
      public gfx::AnimationDelegate,
      public ui::ImplicitAnimationObserver {
 public:
  RoundedContainerView(WindowSelectorItem* item,
                       aura::Window* item_window,
                       int corner_radius,
                       SkColor background)
      : item_(item),
        item_window_(item_window),
        corner_radius_(corner_radius),
        initial_color_(background),
        target_color_(background),
        current_value_(0),
        animation_(new gfx::SlideAnimation(this)) {
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
  }

  ~RoundedContainerView() override { StopObservingImplicitAnimations(); }

  void OnItemRestored() {
    item_ = nullptr;
    item_window_ = nullptr;
  }

  // Used by tests to set animation state.
  gfx::SlideAnimation* animation() { return animation_.get(); }

  void set_color(SkColor target_color) { target_color_ = target_color; }

  // Starts a color animation using |tween_type|. The animation will change the
  // color from |initial_color_| to |target_color_| over |duration| specified
  // in milliseconds.
  // This animation can start once the implicit layer fade-in opacity animation
  // is completed. It is used to transition color from the opaque original
  // window header color to |kLabelBackgroundColor| on entry into overview mode
  // and from |kLabelBackgroundColor| back to the original window header color
  // on exit from the overview mode.
  void AnimateColor(gfx::Tween::Type tween_type, int duration) {
    animation_->SetSlideDuration(duration);
    animation_->SetTweenType(tween_type);
    animation_->Reset(0);
    animation_->Show();

    // Tests complete animations immediately. Emulate by invoking the callback.
    if (ui::ScopedAnimationDurationScaleMode::duration_scale_mode() ==
        ui::ScopedAnimationDurationScaleMode::ZERO_DURATION) {
      AnimationEnded(animation_.get());
    }
  }

  // Changes the view opacity by animating its background color. The animation
  // will change the alpha value in |target_color_| from its current value to
  // |opacity| * 255 but preserve the RGB values.
  void AnimateBackgroundOpacity(float opacity) {
    animation_->SetSlideDuration(kSelectorFadeInMilliseconds);
    animation_->SetTweenType(gfx::Tween::EASE_OUT);
    animation_->Reset(0);
    animation_->Show();
    target_color_ = SkColorSetA(target_color_, opacity * 255);
  }

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override {
    views::View::OnPaint(canvas);
    SkScalar radius = SkIntToScalar(corner_radius_);
    const SkScalar kRadius[8] = {radius, radius, radius, radius, 0, 0, 0, 0};
    SkPath path;
    gfx::Rect bounds(size());
    path.addRoundRect(gfx::RectToSkRect(bounds), kRadius);

    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    canvas->ClipPath(path, true);

    SkColor target_color = initial_color_;
    if (target_color_ != target_color) {
      target_color = color_utils::AlphaBlend(target_color_, initial_color_,
                                             current_value_);
    }
    canvas->DrawColor(target_color);
  }

  const char* GetClassName() const override { return "RoundedContainerView"; }

 private:
  // gfx::AnimationDelegate:
  void AnimationEnded(const gfx::Animation* animation) override {
    initial_color_ = target_color_;
    // Tabbed browser windows show the overview mode header behind the window
    // during the initial animation. Once the initial fade-in completes and the
    // overview header is fully exposed update stacking to keep the label above
    // the item which prevents input events from reaching the window.
    aura::Window* widget_window = GetWidget()->GetNativeWindow();
    if (widget_window && item_window_)
      widget_window->parent()->StackChildAbove(widget_window, item_window_);
    item_window_ = nullptr;
  }

  void AnimationProgressed(const gfx::Animation* animation) override {
    current_value_ = animation_->CurrentValueBetween(0, 255);
    SchedulePaint();
  }

  void AnimationCanceled(const gfx::Animation* animation) override {
    item_window_ = nullptr;
    initial_color_ = target_color_;
    current_value_ = 255;
    SchedulePaint();
  }

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override {
    // Return if the fade in animation of |item_->item_widget_| was aborted.
    if (WasAnimationAbortedForProperty(
            ui::LayerAnimationElement::AnimatableProperty::OPACITY)) {
      return;
    }
    DCHECK(WasAnimationCompletedForProperty(
        ui::LayerAnimationElement::AnimatableProperty::OPACITY));

    // Otherwise, animate the color of this view.
    AnimateColor(gfx::Tween::EASE_IN, kSelectorColorSlideMilliseconds);
  }

  WindowSelectorItem* item_;
  aura::Window* item_window_;
  int corner_radius_;
  SkColor initial_color_;
  SkColor target_color_;
  int current_value_;
  std::unique_ptr<gfx::SlideAnimation> animation_;

  DISALLOW_COPY_AND_ASSIGN(RoundedContainerView);
};

// A Container View that has a ShieldButton to listen to events. The
// ShieldButton covers most of the View except for the transparent gap between
// the windows and is visually transparent. The ShieldButton owns a background
// non-transparent view positioned at the ShieldButton top. The background view
// in its turn owns an item text label and a close button.
// The text label does not receive events, however the close button is higher in
// Z-order than its parent and receives events forwarding them to the same
// |listener| (i.e. WindowSelectorItem::ButtonPressed()).
class WindowSelectorItem::CaptionContainerView : public views::View {
 public:
  CaptionContainerView(ButtonListener* listener,
                       views::ImageView* image_view,
                       views::Label* title_label,
                       views::Label* cannot_snap_label,
                       views::ImageButton* close_button,
                       WindowSelectorItem::RoundedContainerView* background)
      : listener_button_(new ShieldButton(listener, title_label->text())),
        background_(background),
        image_view_(image_view),
        title_label_(title_label),
        cannot_snap_label_(cannot_snap_label),
        close_button_(close_button) {
    if (image_view_)
      background_->AddChildView(image_view_);
    background_->AddChildView(title_label_);
    listener_button_->AddChildView(background_);
    AddChildView(listener_button_);
    if (IsNewOverviewUi()) {
      // Do not make |close_button_| a child of |background_| because
      // |close_button_|'s hit radius should extend outside the bounds of
      // |background_|.
      close_button_->SetPaintToLayer();
      close_button_->layer()->SetFillsBoundsOpaquely(false);
      AddChildView(close_button_);
    } else {
      background_->AddChildView(close_button_);
    }

    // Use |cannot_snap_container_| to specify the padding surrounding
    // |cannot_snap_label_| and to give the label rounded corners.
    cannot_snap_container_ = new RoundedRectView(
        kSplitviewLabelRoundRectRadiusDp, kSplitviewLabelBackgroundColor);
    cannot_snap_container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::kVertical,
        gfx::Insets(kSplitviewLabelVerticalInsetDp,
                    kSplitviewLabelHorizontalInsetDp)));
    cannot_snap_container_->AddChildView(cannot_snap_label_);
    cannot_snap_container_->set_can_process_events_within_subtree(false);
    cannot_snap_container_->SetPaintToLayer();
    cannot_snap_container_->layer()->SetFillsBoundsOpaquely(false);
    cannot_snap_container_->layer()->SetOpacity(0.f);
    AddChildView(cannot_snap_container_);
  }

  ShieldButton* listener_button() { return listener_button_; }

  void SetCannotSnapLabelVisibility(bool visible) {
    AnimateSplitviewLabelOpacity(cannot_snap_container_->layer(), visible);
  }

 protected:
  // views::View:
  void Layout() override {
    // Position close button in the top right corner sized to its icon size and
    // the label in the top left corner as tall as the button and extending to
    // the button's left edge. Position the cannot snap label in the center of
    // the window selector item.
    // The rest of this container view serves as a shield to prevent input
    // events from reaching the transformed window in overview.
    gfx::Rect bounds(GetLocalBounds());

    bounds.Inset(kWindowSelectorMargin, kWindowSelectorMargin);
    listener_button_->SetBoundsRect(bounds);

    // Position the cannot snap label.
    gfx::Size label_size = cannot_snap_label_->CalculatePreferredSize();
    label_size.set_width(
        std::min(label_size.width() + 2 * kSplitviewLabelHorizontalInsetDp,
                 bounds.width() - 2 * kSplitviewLabelHorizontalInsetDp));
    label_size.set_height(
        std::max(label_size.height(), kSplitviewLabelPreferredHeightDp));

    gfx::Rect cannot_snap_bounds = GetLocalBounds();
    cannot_snap_bounds.ClampToCenteredSize(label_size);
    cannot_snap_container_->SetBoundsRect(cannot_snap_bounds);

    const int visible_height = close_button_->GetPreferredSize().height();
    gfx::Rect background_bounds(gfx::Rect(bounds.size()));
    background_bounds.set_height(visible_height);
    background_->SetBoundsRect(background_bounds);

    const int label_padding = IsNewOverviewUi() ? kHorizontalLabelPaddingDp
                                                : kOldHorizontalLabelPaddingDp;
    bounds = background_bounds;
    bounds.Inset(
        label_padding + (image_view_ ? image_view_->size().width() : 0), 0,
        label_padding + visible_height, 0);
    title_label_->SetBoundsRect(bounds);

    if (image_view_) {
      bounds.set_x(0);
      bounds.set_width(image_view_->size().width());
      image_view_->SetBoundsRect(bounds);
    }

    bounds = background_bounds;
    // Align the close button so that the right edge of its image is aligned
    // with the right edge of the overview window, but the right edge of its
    // touch target exceeds it.
    bounds.set_x(
        bounds.width() - visible_height +
        (IsNewOverviewUi() ? kCloseButtonOffsetDp + kWindowSelectorMargin : 0));
    if (IsNewOverviewUi())
      bounds.set_y(kWindowSelectorMargin);
    bounds.set_width(visible_height);
    close_button_->SetBoundsRect(bounds);
  }

  const char* GetClassName() const override { return "CaptionContainerView"; }

 private:
  ShieldButton* listener_button_;
  WindowSelectorItem::RoundedContainerView* background_;
  views::ImageView* image_view_;
  views::Label* title_label_;
  views::Label* cannot_snap_label_;
  RoundedRectView* cannot_snap_container_;
  views::ImageButton* close_button_;

  DISALLOW_COPY_AND_ASSIGN(CaptionContainerView);
};

WindowSelectorItem::WindowSelectorItem(aura::Window* window,
                                       WindowSelector* window_selector,
                                       WindowGrid* window_grid)
    : dimmed_(false),
      root_window_(window->GetRootWindow()),
      transform_window_(this, window),
      in_bounds_update_(false),
      selected_(false),
      caption_container_view_(nullptr),
      label_view_(nullptr),
      close_button_(new OverviewCloseButton(this)),
      window_selector_(window_selector),
      background_view_(nullptr),
      window_grid_(window_grid) {
  CreateWindowLabel(window->GetTitle());
  GetWindow()->AddObserver(this);
}

WindowSelectorItem::~WindowSelectorItem() {
  GetWindow()->RemoveObserver(this);
}

aura::Window* WindowSelectorItem::GetWindow() {
  return transform_window_.window();
}

void WindowSelectorItem::RestoreWindow() {
  caption_container_view_->listener_button()->ResetListener();
  close_button_->ResetListener();
  transform_window_.RestoreWindow();
  if (background_view_) {
    background_view_->OnItemRestored();
    background_view_ = nullptr;
  }
  UpdateHeaderLayout(
      HeaderFadeInMode::EXIT,
      OverviewAnimationType::OVERVIEW_ANIMATION_LAY_OUT_SELECTOR_ITEMS);
}

void WindowSelectorItem::EnsureVisible() {
  transform_window_.EnsureVisible();
}

void WindowSelectorItem::Shutdown() {
  if (transform_window_.GetTopInset()) {
    // Activating a window (even when it is the window that was active before
    // overview) results in stacking it at the top. Maintain the label window
    // stacking position above the item to make the header transformation more
    // gradual upon exiting the overview mode.
    aura::Window* widget_window = item_widget_->GetNativeWindow();

    // |widget_window| was originally created in the same container as the
    // |transform_window_| but when closing overview the |transform_window_|
    // could have been reparented if a drag was active. Only change stacking
    // if the windows still belong to the same container.
    if (widget_window->parent() == transform_window_.window()->parent()) {
      widget_window->parent()->StackChildAbove(widget_window,
                                               transform_window_.window());
    }
  }
  if (background_view_) {
    background_view_->OnItemRestored();
    background_view_ = nullptr;
  }
  FadeOut(std::move(item_widget_));
}

void WindowSelectorItem::PrepareForOverview() {
  transform_window_.PrepareForOverview();
  UpdateHeaderLayout(HeaderFadeInMode::ENTER,
                     OverviewAnimationType::OVERVIEW_ANIMATION_NONE);
}

bool WindowSelectorItem::Contains(const aura::Window* target) const {
  return transform_window_.Contains(target);
}

void WindowSelectorItem::SetBounds(const gfx::Rect& target_bounds,
                                   OverviewAnimationType animation_type) {
  if (in_bounds_update_)
    return;
  base::AutoReset<bool> auto_reset_in_bounds_update(&in_bounds_update_, true);
  target_bounds_ = target_bounds;

  gfx::Rect inset_bounds(target_bounds);
  inset_bounds.Inset(kWindowMargin, kWindowMargin);
  SetItemBounds(inset_bounds, animation_type);

  // SetItemBounds is called before UpdateHeaderLayout so the header can
  // properly use the updated windows bounds.
  UpdateHeaderLayout(HeaderFadeInMode::UPDATE, animation_type);
}

void WindowSelectorItem::SetSelected(bool selected) {
  selected_ = selected;
  background_view_->AnimateBackgroundOpacity(selected ? 0.f : kHeaderOpacity);
}

void WindowSelectorItem::SendAccessibleSelectionEvent() {
  caption_container_view_->listener_button()->NotifyAccessibilityEvent(
      ui::AX_EVENT_SELECTION, true);
}

void WindowSelectorItem::CloseWindow() {
  gfx::Rect inset_bounds(target_bounds_);
  inset_bounds.Inset(target_bounds_.width() * kPreCloseScale,
                     target_bounds_.height() * kPreCloseScale);
  OverviewAnimationType animation_type =
      OverviewAnimationType::OVERVIEW_ANIMATION_CLOSING_SELECTOR_ITEM;
  // Scale down both the window and label.
  SetBounds(inset_bounds, animation_type);
  // First animate opacity to an intermediate value concurrently with the
  // scaling animation.
  AnimateOpacity(kClosingItemOpacity, animation_type);

  // Fade out the window and the label, effectively hiding them.
  AnimateOpacity(0.0,
                 OverviewAnimationType::OVERVIEW_ANIMATION_CLOSE_SELECTOR_ITEM);
  transform_window_.Close();
}

void WindowSelectorItem::OnMinimizedStateChanged() {
  transform_window_.UpdateMirrorWindowForMinimizedState();
}

void WindowSelectorItem::UpdateCannotSnapWarningVisibility() {
  // Windows which can snap will never show this warning.
  if (Shell::Get()->split_view_controller()->CanSnap(GetWindow())) {
    caption_container_view_->SetCannotSnapLabelVisibility(false);
    return;
  }

  const SplitViewController::State state =
      Shell::Get()->split_view_controller()->state();
  const bool visible = state == SplitViewController::LEFT_SNAPPED ||
                       state == SplitViewController::RIGHT_SNAPPED;
  caption_container_view_->SetCannotSnapLabelVisibility(visible);
}

void WindowSelectorItem::SetDimmed(bool dimmed) {
  dimmed_ = dimmed;
  SetOpacity(dimmed ? kDimmedItemOpacity : 1.0f);
}

void WindowSelectorItem::ButtonPressed(views::Button* sender,
                                       const ui::Event& event) {
  if (sender == close_button_) {
    base::RecordAction(
        base::UserMetricsAction("WindowSelector_OverviewCloseButton"));
    if (Shell::Get()
            ->tablet_mode_controller()
            ->IsTabletModeWindowManagerEnabled()) {
      base::RecordAction(
          base::UserMetricsAction("Tablet_WindowCloseFromOverviewButton"));
    }
    CloseWindow();
    return;
  }
  CHECK(sender == caption_container_view_->listener_button());

  // For other cases, the event is handled in OverviewWindowDragController.
  if (!SplitViewController::ShouldAllowSplitView())
    window_selector_->SelectWindow(this);
}

void WindowSelectorItem::OnWindowDestroying(aura::Window* window) {
  window->RemoveObserver(this);
  transform_window_.OnWindowDestroyed();
}

void WindowSelectorItem::OnWindowTitleChanged(aura::Window* window) {
  // TODO(flackr): Maybe add the new title to a vector of titles so that we can
  // filter any of the titles the window had while in the overview session.
  label_view_->SetText(window->GetTitle());
  UpdateAccessibilityName();
}

float WindowSelectorItem::GetItemScale(const gfx::Size& size) {
  gfx::Size inset_size(size.width(), size.height() - 2 * kWindowMargin);
  return ScopedTransformOverviewWindow::GetItemScale(
      transform_window_.GetTargetBoundsInScreen().size(), inset_size,
      transform_window_.GetTopInset(),
      close_button_->GetPreferredSize().height());
}

void WindowSelectorItem::HandlePressEvent(
    const gfx::Point& location_in_screen) {
  StartDrag();
  window_selector_->InitiateDrag(this, location_in_screen);
}

void WindowSelectorItem::HandleReleaseEvent(
    const gfx::Point& location_in_screen) {
  EndDrag();
  window_selector_->CompleteDrag(this, location_in_screen);
}

void WindowSelectorItem::HandleDragEvent(const gfx::Point& location_in_screen) {
  window_selector_->Drag(this, location_in_screen);
}

void WindowSelectorItem::ActivateDraggedWindow() {
  DCHECK_EQ(this, window_selector_->window_drag_controller()->item());
  window_selector_->ActivateDraggedWindow();
}

void WindowSelectorItem::ResetDraggedWindowGesture() {
  DCHECK_EQ(this, window_selector_->window_drag_controller()->item());
  window_selector_->ResetDraggedWindowGesture();
}

gfx::Rect WindowSelectorItem::GetTargetBoundsInScreen() const {
  return transform_window_.GetTargetBoundsInScreen();
}

void WindowSelectorItem::SetItemBounds(const gfx::Rect& target_bounds,
                                       OverviewAnimationType animation_type) {
  DCHECK(root_window_ == GetWindow()->GetRootWindow());
  gfx::Rect screen_rect = transform_window_.GetTargetBoundsInScreen();

  // Avoid division by zero by ensuring screen bounds is not empty.
  gfx::Size screen_size(screen_rect.size());
  screen_size.SetToMax(gfx::Size(1, 1));
  screen_rect.set_size(screen_size);

  const int top_view_inset = transform_window_.GetTopInset();
  const int title_height = close_button_->GetPreferredSize().height();
  gfx::Rect selector_item_bounds =
      ScopedTransformOverviewWindow::ShrinkRectToFitPreservingAspectRatio(
          screen_rect, target_bounds, top_view_inset, title_height);
  gfx::Transform transform = ScopedTransformOverviewWindow::GetTransformForRect(
      screen_rect, selector_item_bounds);
  ScopedTransformOverviewWindow::ScopedAnimationSettings animation_settings;
  transform_window_.BeginScopedAnimation(animation_type, &animation_settings);
  transform_window_.SetTransform(root_window_, transform);
}

void WindowSelectorItem::SetOpacity(float opacity) {
  item_widget_->SetOpacity(opacity);
  if (background_view_) {
    background_view_->AnimateBackgroundOpacity(
        selected_ ? 0.f : kHeaderOpacity * opacity);
  }
  transform_window_.SetOpacity(opacity);
}

void WindowSelectorItem::CreateWindowLabel(const base::string16& title) {
  background_view_ = new RoundedContainerView(this, transform_window_.window(),
                                              kLabelBackgroundRadius,
                                              transform_window_.GetTopColor());
  if (IsNewOverviewUi())
    background_view_->set_color(SK_ColorTRANSPARENT);

  // |background_view_| will get added as a child to CaptionContainerView.
  views::Widget::InitParams params_label;
  params_label.type = views::Widget::InitParams::TYPE_POPUP;
  params_label.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params_label.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params_label.visible_on_all_workspaces = true;
  params_label.layer_type = ui::LAYER_NOT_DRAWN;
  params_label.name = "OverviewModeLabel";
  params_label.activatable =
      views::Widget::InitParams::Activatable::ACTIVATABLE_DEFAULT;
  params_label.accept_events = true;
  item_widget_.reset(new views::Widget);
  params_label.parent = transform_window_.window()->parent();
  item_widget_->set_focus_on_creation(false);
  item_widget_->Init(params_label);
  aura::Window* widget_window = item_widget_->GetNativeWindow();
  if (transform_window_.GetTopInset()) {
    // For windows with headers the overview header fades in above the
    // original window header.
    widget_window->parent()->StackChildAbove(widget_window,
                                             transform_window_.window());
  } else {
    // For tabbed windows the overview header slides from behind. The stacking
    // is then corrected when the animation completes.
    widget_window->parent()->StackChildBelow(widget_window,
                                             transform_window_.window());
  }

  // Create an image view for and scale down the windows window icon, if it
  // exists.
  views::ImageView* image_view = nullptr;
  if (IsNewOverviewUi()) {
    gfx::ImageSkia* icon =
        transform_window_.window()->GetProperty(aura::client::kWindowIconKey);
    if (icon && !icon->size().IsEmpty()) {
      image_view = new views::ImageView();
      image_view->SetImage(gfx::ImageSkiaOperations::CreateResizedImage(
          *icon, skia::ImageOperations::RESIZE_GOOD, kIconSize));
      image_view->SetSize(kIconSize);
    }
  }

  label_view_ = new views::Label(title);
  label_view_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label_view_->SetAutoColorReadabilityEnabled(false);
  label_view_->SetEnabledColor(kLabelColor);
  // Tell the label what color it will be drawn onto. It will use whether the
  // background color is opaque or transparent to decide whether to use
  // subpixel rendering. Does not actually set the label's background color.
  label_view_->SetBackgroundColor(kLabelBackgroundColor);
  if (IsNewOverviewUi()) {
    label_view_->SetFontList(gfx::FontList().Derive(
        kLabelFontDelta, gfx::Font::NORMAL, gfx::Font::Weight::MEDIUM));
  }

  cannot_snap_label_view_ = new views::Label(
      l10n_util::GetStringUTF16(IDS_ASH_SPLIT_VIEW_CANNOT_SNAP));
  cannot_snap_label_view_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  cannot_snap_label_view_->SetAutoColorReadabilityEnabled(false);
  cannot_snap_label_view_->SetEnabledColor(kSplitviewLabelEnabledColor);
  cannot_snap_label_view_->SetBackgroundColor(kSplitviewLabelBackgroundColor);

  caption_container_view_ = new CaptionContainerView(
      this, image_view, label_view_, cannot_snap_label_view_, close_button_,
      background_view_);
  UpdateCannotSnapWarningVisibility();

  item_widget_->SetContentsView(caption_container_view_);
  label_view_->SetVisible(false);
  item_widget_->SetOpacity(0);
  item_widget_->Show();
  item_widget_->GetLayer()->SetMasksToBounds(false);
}

void WindowSelectorItem::UpdateHeaderLayout(
    HeaderFadeInMode mode,
    OverviewAnimationType animation_type) {
  gfx::Rect transformed_window_bounds(transform_window_.GetTransformedBounds());
  ::wm::ConvertRectFromScreen(root_window_, &transformed_window_bounds);

  gfx::Rect label_rect(close_button_->GetPreferredSize());
  label_rect.set_width(transformed_window_bounds.width());
  // For tabbed windows the initial bounds of the caption are set such that it
  // appears to be "growing" up from the window content area.
  label_rect.set_y(
      (mode != HeaderFadeInMode::ENTER || transform_window_.GetTopInset())
          ? -label_rect.height()
          : 0);

  {
    ui::ScopedLayerAnimationSettings layer_animation_settings(
        item_widget_->GetLayer()->GetAnimator());
    if (background_view_) {
      if (mode == HeaderFadeInMode::ENTER) {
        // Animate the color of |background_view_| once the fade in animation of
        // |item_widget_| ends.
        layer_animation_settings.AddObserver(background_view_);
        if (!IsNewOverviewUi())
          background_view_->set_color(kLabelBackgroundColor);
      } else if (mode == HeaderFadeInMode::EXIT) {
        // Make the header visible above the window. It will be faded out when
        // the Shutdown() is called.
        background_view_->AnimateColor(gfx::Tween::EASE_OUT,
                                       kExitFadeInMilliseconds);
        if (!IsNewOverviewUi())
          background_view_->set_color(kLabelExitColor);
      }
    }
    if (!label_view_->visible()) {
      label_view_->SetVisible(true);
      SetupFadeInAfterLayout(item_widget_.get());
    }
  }

  aura::Window* widget_window = item_widget_->GetNativeWindow();
  ScopedOverviewAnimationSettings animation_settings(animation_type,
                                                     widget_window);
  // |widget_window| covers both the transformed window and the header
  // as well as the gap between the windows to prevent events from reaching
  // the window including its sizing borders.
  if (mode != HeaderFadeInMode::ENTER) {
    label_rect.set_height(close_button_->GetPreferredSize().height() +
                          transformed_window_bounds.height());
  }
  label_rect.Inset(-kWindowSelectorMargin, -kWindowSelectorMargin);
  widget_window->SetBounds(label_rect);
  gfx::Transform label_transform;
  label_transform.Translate(transformed_window_bounds.x(),
                            transformed_window_bounds.y());
  widget_window->SetTransform(label_transform);
}

void WindowSelectorItem::AnimateOpacity(float opacity,
                                        OverviewAnimationType animation_type) {
  DCHECK_GE(opacity, 0.f);
  DCHECK_LE(opacity, 1.f);
  ScopedTransformOverviewWindow::ScopedAnimationSettings animation_settings;
  transform_window_.BeginScopedAnimation(animation_type, &animation_settings);
  transform_window_.SetOpacity(opacity);

  const float header_opacity = selected_ ? 0.f : kHeaderOpacity * opacity;
  aura::Window* widget_window = item_widget_->GetNativeWindow();
  ScopedOverviewAnimationSettings animation_settings_label(animation_type,
                                                           widget_window);
  widget_window->layer()->SetOpacity(header_opacity);
}

void WindowSelectorItem::UpdateAccessibilityName() {
  caption_container_view_->listener_button()->SetAccessibleName(
      GetWindow()->GetTitle());
}

void WindowSelectorItem::FadeOut(std::unique_ptr<views::Widget> widget) {
  widget->SetOpacity(1.f);

  // Fade out the widget. This animation continues past the lifetime of |this|.
  aura::Window* widget_window = widget->GetNativeWindow();
  ScopedOverviewAnimationSettings animation_settings(
      OverviewAnimationType::OVERVIEW_ANIMATION_EXIT_OVERVIEW_MODE_FADE_OUT,
      widget_window);
  // CleanupAnimationObserver will delete itself (and the widget) when the
  // opacity animation is complete.
  // Ownership over the observer is passed to the window_selector_->delegate()
  // which has longer lifetime so that animations can continue even after the
  // overview mode is shut down.
  views::Widget* widget_ptr = widget.get();
  std::unique_ptr<CleanupAnimationObserver> observer(
      new CleanupAnimationObserver(std::move(widget)));
  animation_settings.AddObserver(observer.get());
  window_selector_->delegate()->AddDelayedAnimationObserver(
      std::move(observer));
  widget_ptr->SetOpacity(0.f);
}

gfx::SlideAnimation* WindowSelectorItem::GetBackgroundViewAnimation() {
  return background_view_ ? background_view_->animation() : nullptr;
}

aura::Window* WindowSelectorItem::GetOverviewWindowForMinimizedStateForTest() {
  return transform_window_.GetOverviewWindowForMinimizedState();
}

void WindowSelectorItem::StartDrag() {
  window_grid_->SetSelectionWidgetVisibility(false);

  gfx::Rect scaled_bounds(target_bounds_);
  scaled_bounds.Inset(-target_bounds_.width() * kDragWindowScale,
                      -target_bounds_.height() * kDragWindowScale);
  OverviewAnimationType animation_type =
      OverviewAnimationType::OVERVIEW_ANIMATION_DRAGGING_SELECTOR_ITEM;
  SetBounds(scaled_bounds, animation_type);

  aura::Window* widget_window = item_widget_->GetNativeWindow();
  if (widget_window && widget_window->parent() == GetWindow()->parent()) {
    // TODO(xdai): This might not work if there is an always on top window.
    // See crbug.com/733760.
    widget_window->parent()->StackChildAtTop(widget_window);
    widget_window->parent()->StackChildBelow(GetWindow(), widget_window);
  }
}

void WindowSelectorItem::EndDrag() {
  window_grid_->SetSelectionWidgetVisibility(true);

  // First stack this item's window below the snapped window if split view mode
  // is active.
  aura::Window* dragged_window = GetWindow();
  aura::Window* dragged_widget_window = item_widget_->GetNativeWindow();
  aura::Window* parent_window = dragged_widget_window->parent();
  if (Shell::Get()->IsSplitViewModeActive()) {
    aura::Window* snapped_window =
        Shell::Get()->split_view_controller()->GetDefaultSnappedWindow();
    if (snapped_window->parent() == parent_window &&
        dragged_window->parent() == parent_window) {
      parent_window->StackChildBelow(dragged_widget_window, snapped_window);
      parent_window->StackChildBelow(dragged_window, dragged_widget_window);
    }
  }

  // Then find the window which was stacked right above this selector item's
  // window before dragging and stack this selector item's window below it.
  const std::vector<std::unique_ptr<WindowSelectorItem>>& selector_items =
      window_grid_->window_list();
  aura::Window* stacking_target = nullptr;
  for (size_t index = 0; index < selector_items.size(); index++) {
    if (index > 0) {
      aura::Window* window = selector_items[index - 1].get()->GetWindow();
      if (window->parent() == parent_window &&
          dragged_window->parent() == parent_window) {
        stacking_target = window;
      }
    }
    if (selector_items[index].get() == this && stacking_target) {
      parent_window->StackChildBelow(dragged_widget_window, stacking_target);
      parent_window->StackChildBelow(dragged_window, dragged_widget_window);
      break;
    }
  }
}

}  // namespace ash
