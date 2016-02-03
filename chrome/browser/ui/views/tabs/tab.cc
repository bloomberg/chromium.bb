// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab.h"

#include <stddef.h>
#include <limits>
#include <utility>

#include "base/command_line.h"
#include "base/debug/alias.h"
#include "base/macros.h"
#include "base/profiler/scoped_tracker.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/tabs/media_indicator_button.h"
#include "chrome/browser/ui/views/tabs/tab_controller.h"
#include "chrome/browser/ui/views/touch_uma/touch_uma.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/user_metrics.h"
#include "grit/components_scaled_resources.h"
#include "grit/components_strings.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "third_party/skia/include/pathops/SkPathOps.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/animation/animation_container.h"
#include "ui/gfx/animation/multi_animation.h"
#include "ui/gfx/animation/throb_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/path.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/vector_icons_public.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/rect_based_targeting_utils.h"
#include "ui/views/view_targeter.h"
#include "ui/views/widget/tooltip_manager.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

#if defined(USE_AURA)
#include "ui/aura/env.h"
#endif

using base::UserMetricsAction;

namespace {

// How long the pulse throb takes.
const int kPulseDurationMs = 200;

// Width of touch tabs.
const int kTouchWidth = 120;

const int kExtraLeftPaddingToBalanceCloseButtonPadding = 2;
const int kAfterTitleSpacing = 4;

// When a non-pinned tab becomes a pinned tab the width of the tab animates. If
// the width of a pinned tab is at least kPinnedTabExtraWidthToRenderAsNormal
// larger than the desired pinned tab width then the tab is rendered as a normal
// tab. This is done to avoid having the title immediately disappear when
// transitioning a tab from normal to pinned tab.
const int kPinnedTabExtraWidthToRenderAsNormal = 30;

// How opaque to make the hover state (out of 1).
const double kHoverOpacity = 0.33;

// Opacity of the active tab background painted over inactive selected tabs.
const double kSelectedTabOpacity = 0.3;

// Inactive selected tabs have their throb value scaled by this.
const double kSelectedTabThrobScale = 0.95 - kSelectedTabOpacity;

// Durations for the various parts of the pinned tab title animation.
const int kPinnedTitleChangeAnimationDuration1MS = 1600;
const int kPinnedTitleChangeAnimationStart1MS = 0;
const int kPinnedTitleChangeAnimationEnd1MS = 1900;
const int kPinnedTitleChangeAnimationDuration2MS = 0;
const int kPinnedTitleChangeAnimationDuration3MS = 550;
const int kPinnedTitleChangeAnimationStart3MS = 150;
const int kPinnedTitleChangeAnimationEnd3MS = 800;
const int kPinnedTitleChangeAnimationIntervalMS = 40;

// Offset from the right edge for the start of the pinned title change
// animation.
const int kPinnedTitleChangeInitialXOffset = 6;

// Max number of images to cache. This has to be at least two since rounding
// errors may lead to tabs in the same tabstrip having different sizes.
const size_t kMaxImageCacheSize = 4;

// Height of the miniature tab strip in immersive mode.
const int kImmersiveTabHeight = 3;

// Height of the small tab indicator rectangles in immersive mode.
const int kImmersiveBarHeight = 2;

// Color for active and inactive tabs in the immersive mode light strip. These
// should be a little brighter than the color of the normal art assets for tabs,
// which for active tabs is 230, 230, 230 and for inactive is 184, 184, 184.
const SkColor kImmersiveActiveTabColor = SkColorSetRGB(235, 235, 235);
const SkColor kImmersiveInactiveTabColor = SkColorSetRGB(190, 190, 190);

// The minimum opacity (out of 1) when a tab (either active or inactive) is
// throbbing in the immersive mode light strip.
const double kImmersiveTabMinThrobOpacity = 0.66;

// Number of steps in the immersive mode loading animation.
const int kImmersiveLoadingStepCount = 32;

const char kTabCloseButtonName[] = "TabCloseButton";
const int kTabCloseButtonSize = 16;

// Returns the width of the tab endcap at scale 1.  More precisely, this is the
// width of the curve making up either the outer or inner edge of the stroke;
// since these two curves are horizontally offset by 1 px (regardless of scale),
// the total width of the endcap from tab outer edge to the inside end of the
// stroke inner edge is (GetUnscaledEndcapWidth() * scale) + 1.
float GetUnscaledEndcapWidth() {
  return GetLayoutInsets(TAB).left() - 0.5f;
}

// Stop()s |animation| and then deletes it. We do this rather than just deleting
// so that the delegate is notified before the destruction.
void StopAndDeleteAnimation(scoped_ptr<gfx::Animation> animation) {
  if (animation)
    animation->Stop();
}

void DrawHighlight(gfx::Canvas* canvas,
                   const SkPoint& p,
                   SkScalar radius,
                   SkAlpha alpha) {
  const SkColor colors[2] = { SkColorSetA(SK_ColorWHITE, alpha),
                              SkColorSetA(SK_ColorWHITE, 0) };
  skia::RefPtr<SkShader> shader = skia::AdoptRef(SkGradientShader::CreateRadial(
      p, radius, colors, nullptr, 2, SkShader::kClamp_TileMode));
  SkPaint paint;
  paint.setStyle(SkPaint::kFill_Style);
  paint.setAntiAlias(true);
  paint.setShader(shader.get());
  canvas->sk_canvas()->drawRect(
      SkRect::MakeXYWH(p.x() - radius, p.y() - radius, radius * 2, radius * 2),
      paint);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// FaviconCrashAnimation
//
//  A custom animation subclass to manage the favicon crash animation.
class Tab::FaviconCrashAnimation : public gfx::LinearAnimation,
                                   public gfx::AnimationDelegate {
 public:
  explicit FaviconCrashAnimation(Tab* target)
      : gfx::LinearAnimation(1000, 25, this),
        target_(target) {
  }
  ~FaviconCrashAnimation() override {}

  // gfx::Animation overrides:
  void AnimateToState(double state) override {
    const double kHidingOffset =
        Tab::GetMinimumInactiveSize().height() - GetLayoutInsets(TAB).height();

    if (state < .5) {
      // Animate the normal icon down.
      target_->SetFaviconHidingOffset(
          static_cast<int>(floor(kHidingOffset * 2.0 * state)));
    } else {
      // Animate the crashed icon up.
      target_->set_should_display_crashed_favicon();
      target_->SetFaviconHidingOffset(
          static_cast<int>(
              floor(kHidingOffset - ((state - .5) * 2.0 * kHidingOffset))));
    }
  }

 private:
  Tab* target_;

  DISALLOW_COPY_AND_ASSIGN(FaviconCrashAnimation);
};

////////////////////////////////////////////////////////////////////////////////
// TabCloseButton
//
//  This is a Button subclass that causes middle clicks to be forwarded to the
//  parent View by explicitly not handling them in OnMousePressed.
class Tab::TabCloseButton : public views::ImageButton,
                            public views::MaskedTargeterDelegate {
 public:
  explicit TabCloseButton(Tab* tab)
      : views::ImageButton(tab),
        tab_(tab) {
    SetEventTargeter(
        scoped_ptr<views::ViewTargeter>(new views::ViewTargeter(this)));
  }

  ~TabCloseButton() override {}

  // views::View:
  View* GetTooltipHandlerForPoint(const gfx::Point& point) override {
    // Tab close button has no children, so tooltip handler should be the same
    // as the event handler.
    // In addition, a hit test has to be performed for the point (as
    // GetTooltipHandlerForPoint() is responsible for it).
    if (!HitTestPoint(point))
      return NULL;
    return GetEventHandlerForPoint(point);
  }

  bool OnMousePressed(const ui::MouseEvent& event) override {
    tab_->controller_->OnMouseEventInTab(this, event);

    bool handled = ImageButton::OnMousePressed(event);
    // Explicitly mark midle-mouse clicks as non-handled to ensure the tab
    // sees them.
    return !event.IsMiddleMouseButton() && handled;
  }

  void OnMouseMoved(const ui::MouseEvent& event) override {
    tab_->controller_->OnMouseEventInTab(this, event);
    CustomButton::OnMouseMoved(event);
  }

  void OnMouseReleased(const ui::MouseEvent& event) override {
    tab_->controller_->OnMouseEventInTab(this, event);
    CustomButton::OnMouseReleased(event);
  }

  void OnGestureEvent(ui::GestureEvent* event) override {
    // Consume all gesture events here so that the parent (Tab) does not
    // start consuming gestures.
    ImageButton::OnGestureEvent(event);
    event->SetHandled();
  }

  const char* GetClassName() const override { return kTabCloseButtonName; }

 private:
  // Returns the rectangular bounds of parent tab's visible region in the
  // local coordinate space of |this|.
  gfx::Rect GetTabBounds() const {
    gfx::Path tab_mask;
    tab_->GetHitTestMask(&tab_mask);

    gfx::RectF tab_bounds_f(gfx::SkRectToRectF(tab_mask.getBounds()));
    views::View::ConvertRectToTarget(tab_, this, &tab_bounds_f);
    return gfx::ToEnclosingRect(tab_bounds_f);
  }

  // Returns the rectangular bounds of the tab close button in the local
  // coordinate space of |this|, not including clipped regions on the top
  // or bottom of the button. |tab_bounds| is the rectangular bounds of
  // the parent tab's visible region in the local coordinate space of |this|.
  gfx::Rect GetTabCloseButtonBounds(const gfx::Rect& tab_bounds) const {
    gfx::Rect button_bounds(GetContentsBounds());
    button_bounds.set_x(GetMirroredXForRect(button_bounds));

    int top_overflow = tab_bounds.y() - button_bounds.y();
    int bottom_overflow = button_bounds.bottom() - tab_bounds.bottom();
    if (top_overflow > 0)
      button_bounds.set_y(tab_bounds.y());
    else if (bottom_overflow > 0)
      button_bounds.set_height(button_bounds.height() - bottom_overflow);

    return button_bounds;
  }

  // views::ViewTargeterDelegate:
  View* TargetForRect(View* root, const gfx::Rect& rect) override {
    CHECK_EQ(root, this);

    if (!views::UsePointBasedTargeting(rect))
      return ViewTargeterDelegate::TargetForRect(root, rect);

    // Ignore the padding set on the button.
    gfx::Rect contents_bounds = GetContentsBounds();
    contents_bounds.set_x(GetMirroredXForRect(contents_bounds));

#if defined(USE_AURA)
    // Include the padding in hit-test for touch events.
    if (aura::Env::GetInstance()->is_touch_down())
      contents_bounds = GetLocalBounds();
#endif

    return contents_bounds.Intersects(rect) ? this : parent();
  }

  // views:MaskedTargeterDelegate:
  bool GetHitTestMask(gfx::Path* mask) const override {
    DCHECK(mask);
    mask->reset();

    // The parent tab may be partially occluded by another tab if we are
    // in stacked tab mode, which means that the tab close button may also
    // be partially occluded. Define the hit test mask of the tab close
    // button to be the intersection of the parent tab's visible bounds
    // and the bounds of the tab close button.
    gfx::Rect tab_bounds(GetTabBounds());
    gfx::Rect button_bounds(GetTabCloseButtonBounds(tab_bounds));
    gfx::Rect intersection(gfx::IntersectRects(tab_bounds, button_bounds));

    if (!intersection.IsEmpty()) {
      mask->addRect(RectToSkRect(intersection));
      return true;
    }

    return false;
  }

  bool DoesIntersectRect(const View* target,
                         const gfx::Rect& rect) const override {
    CHECK_EQ(target, this);

    // If the request is not made in response to a gesture, use the
    // default implementation.
    if (views::UsePointBasedTargeting(rect))
      return MaskedTargeterDelegate::DoesIntersectRect(target, rect);

    // The hit test request is in response to a gesture. Return false if any
    // part of the tab close button is hidden from the user.
    // TODO(tdanderson): Consider always returning the intersection if the
    //                   non-rectangular shape of the tab can be accounted for.
    gfx::Rect tab_bounds(GetTabBounds());
    gfx::Rect button_bounds(GetTabCloseButtonBounds(tab_bounds));
    if (!tab_bounds.Contains(button_bounds))
      return false;

    return MaskedTargeterDelegate::DoesIntersectRect(target, rect);
  }

  Tab* tab_;

  DISALLOW_COPY_AND_ASSIGN(TabCloseButton);
};

////////////////////////////////////////////////////////////////////////////////
// ThrobberView
//
// A Layer-backed view for updating a waiting or loading tab throbber.
class Tab::ThrobberView : public views::View {
 public:
  explicit ThrobberView(Tab* owner);

  // Resets the times tracking when the throbber changes state.
  void ResetStartTimes();

 private:
  // views::View:
  bool CanProcessEventsWithinSubtree() const override;
  void OnPaint(gfx::Canvas* canvas) override;

  Tab* owner_;  // Weak. Owns |this|.

  // The point in time when the tab icon was first painted in the waiting state.
  base::TimeTicks waiting_start_time_;

  // The point in time when the tab icon was first painted in the loading state.
  base::TimeTicks loading_start_time_;

  // Paint state for the throbber after the most recent waiting paint.
  gfx::ThrobberWaitingState waiting_state_;

  DISALLOW_COPY_AND_ASSIGN(ThrobberView);
};

Tab::ThrobberView::ThrobberView(Tab* owner) : owner_(owner) {}

void Tab::ThrobberView::ResetStartTimes() {
  waiting_start_time_ = base::TimeTicks();
  loading_start_time_ = base::TimeTicks();
  waiting_state_ = gfx::ThrobberWaitingState();
}

bool Tab::ThrobberView::CanProcessEventsWithinSubtree() const {
  return false;
}

void Tab::ThrobberView::OnPaint(gfx::Canvas* canvas) {
  const TabRendererData::NetworkState state = owner_->data().network_state;
  if (state == TabRendererData::NETWORK_STATE_NONE)
    return;

  const ui::ThemeProvider* tp = GetThemeProvider();
  const gfx::Rect bounds = GetLocalBounds();
  if (state == TabRendererData::NETWORK_STATE_WAITING) {
    if (waiting_start_time_ == base::TimeTicks())
      waiting_start_time_ = base::TimeTicks::Now();

    waiting_state_.elapsed_time = base::TimeTicks::Now() - waiting_start_time_;
    gfx::PaintThrobberWaiting(
        canvas, bounds,
        tp->GetColor(ThemeProperties::COLOR_TAB_THROBBER_WAITING),
        waiting_state_.elapsed_time);
  } else {
    if (loading_start_time_ == base::TimeTicks())
      loading_start_time_ = base::TimeTicks::Now();

    waiting_state_.color =
        tp->GetColor(ThemeProperties::COLOR_TAB_THROBBER_WAITING);
    gfx::PaintThrobberSpinningAfterWaiting(
        canvas, bounds,
        tp->GetColor(ThemeProperties::COLOR_TAB_THROBBER_SPINNING),
        base::TimeTicks::Now() - loading_start_time_, &waiting_state_);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Tab::ImageCacheEntryMetadata

struct Tab::ImageCacheEntryMetadata {
  ImageCacheEntryMetadata(bool incognito,
                          int resource_id,
                          ui::ScaleFactor scale_factor,
                          const gfx::Size& size);
  ~ImageCacheEntryMetadata();

  // Making this a non-member would require a friend declaration in Tab.  Bleh.
  bool operator==(const ImageCacheEntryMetadata& rhs) const;

  // Whether the resource is drawn in an incognito window.  This is only set to
  // true when Material Design is enabled, since before MD tabs in normal and
  // incognito windows look the same.
  bool incognito;

  int resource_id;
  ui::ScaleFactor scale_factor;
  gfx::Size size;
};

Tab::ImageCacheEntryMetadata::ImageCacheEntryMetadata(
    bool incognito,
    int resource_id,
    ui::ScaleFactor scale_factor,
    const gfx::Size& size)
    : incognito(incognito),
      resource_id(resource_id),
      scale_factor(scale_factor),
      size(size) {
  DCHECK_NE(ui::SCALE_FACTOR_NONE, scale_factor);
}

Tab::ImageCacheEntryMetadata::~ImageCacheEntryMetadata() {}

bool Tab::ImageCacheEntryMetadata::operator==(
    const ImageCacheEntryMetadata& rhs) const {
  return incognito == rhs.incognito && resource_id == rhs.resource_id &&
      scale_factor == rhs.scale_factor && size == rhs.size;
}

////////////////////////////////////////////////////////////////////////////////
// Tab::ImageCacheEntry

struct Tab::ImageCacheEntry {
  ImageCacheEntry(const ImageCacheEntryMetadata& metadata,
                  const gfx::ImageSkia& image);
  ~ImageCacheEntry();

  ImageCacheEntryMetadata metadata;
  gfx::ImageSkia image;
};

Tab::ImageCacheEntry::ImageCacheEntry(const ImageCacheEntryMetadata& metadata,
                                      const gfx::ImageSkia& image)
    : metadata(metadata), image(image) {}

Tab::ImageCacheEntry::~ImageCacheEntry() {}

////////////////////////////////////////////////////////////////////////////////
// Tab, statics:

// static
const char Tab::kViewClassName[] = "Tab";
Tab::TabImages Tab::active_images_ = {0};
Tab::TabImages Tab::inactive_images_ = {0};
Tab::TabImages Tab::mask_images_ = {0};
Tab::ImageCache* Tab::image_cache_ = NULL;

////////////////////////////////////////////////////////////////////////////////
// Tab, public:

Tab::Tab(TabController* controller)
    : controller_(controller),
      closing_(false),
      dragging_(false),
      detached_(false),
      favicon_hiding_offset_(0),
      immersive_loading_step_(0),
      should_display_crashed_favicon_(false),
      throbber_(nullptr),
      media_indicator_button_(nullptr),
      close_button_(nullptr),
      title_(new views::Label()),
      tab_activated_with_last_tap_down_(false),
      hover_controller_(this),
      showing_icon_(false),
      showing_media_indicator_(false),
      showing_close_button_(false),
      button_color_(SK_ColorTRANSPARENT) {
  DCHECK(controller);
  InitTabResources();

  // So we get don't get enter/exit on children and don't prematurely stop the
  // hover.
  set_notify_enter_exit_on_child(true);

  set_id(VIEW_ID_TAB);

  SetBorder(views::Border::CreateEmptyBorder(GetLayoutInsets(TAB)));

  title_->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
  title_->SetElideBehavior(gfx::FADE_TAIL);
  title_->SetHandlesTooltips(false);
  title_->SetAutoColorReadabilityEnabled(false);
  title_->SetText(CoreTabHelper::GetDefaultTitle());
  AddChildView(title_);

  SetEventTargeter(
      scoped_ptr<views::ViewTargeter>(new views::ViewTargeter(this)));

  throbber_ = new ThrobberView(this);
  throbber_->SetVisible(false);
  AddChildView(throbber_);

  media_indicator_button_ = new MediaIndicatorButton(this);
  AddChildView(media_indicator_button_);

  close_button_ = new TabCloseButton(this);
  close_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_CLOSE));
  // The normal image is set by OnButtonColorMaybeChanged() because it depends
  // on the current theme and active state.  The hovered and pressed images
  // don't depend on the these, so we can set them here.
  const gfx::ImageSkia& hovered = gfx::CreateVectorIcon(
      gfx::VectorIconId::TAB_CLOSE_HOVERED_PRESSED, kTabCloseButtonSize,
      SkColorSetRGB(0xDB, 0x44, 0x37));
  const gfx::ImageSkia& pressed = gfx::CreateVectorIcon(
      gfx::VectorIconId::TAB_CLOSE_HOVERED_PRESSED, kTabCloseButtonSize,
      SkColorSetRGB(0xA8, 0x35, 0x2A));
  close_button_->SetImage(views::CustomButton::STATE_HOVERED, &hovered);
  close_button_->SetImage(views::CustomButton::STATE_PRESSED, &pressed);

  // Disable animation so that the red danger sign shows up immediately
  // to help avoid mis-clicks.
  close_button_->SetAnimationDuration(0);
  AddChildView(close_button_);

  set_context_menu_controller(this);
}

Tab::~Tab() {
}

void Tab::SetAnimationContainer(gfx::AnimationContainer* container) {
  animation_container_ = container;
  hover_controller_.SetAnimationContainer(container);
}

bool Tab::IsActive() const {
  return controller_->IsActiveTab(this);
}

void Tab::ActiveStateChanged() {
  OnButtonColorMaybeChanged();
  media_indicator_button_->UpdateEnabledForMuteToggle();
  Layout();
}

void Tab::MediaStateChanged() {
  Layout();
}

bool Tab::IsSelected() const {
  return controller_->IsTabSelected(this);
}

void Tab::SetData(const TabRendererData& data) {
  DCHECK(GetWidget());

  if (data_.Equals(data))
    return;

  TabRendererData old(data_);
  UpdateLoadingAnimation(data.network_state);
  data_ = data;

  base::string16 title = data_.title;
  if (title.empty()) {
    title = data_.loading ?
        l10n_util::GetStringUTF16(IDS_TAB_LOADING_TITLE) :
        CoreTabHelper::GetDefaultTitle();
  } else {
    Browser::FormatTitleForDisplay(&title);
  }
  title_->SetText(title);

  if (data_.IsCrashed()) {
    if (!should_display_crashed_favicon_ && !crash_icon_animation_) {
      data_.media_state = TAB_MEDIA_STATE_NONE;
      crash_icon_animation_.reset(new FaviconCrashAnimation(this));
      crash_icon_animation_->Start();
    }
  } else {
    if (crash_icon_animation_)
        crash_icon_animation_.reset();
    should_display_crashed_favicon_ = false;
    favicon_hiding_offset_ = 0;
  }

  if (data_.media_state != old.media_state)
    media_indicator_button_->TransitionToMediaState(data_.media_state);

  if (old.pinned != data_.pinned)
    StopPinnedTabTitleAnimation();

  DataChanged(old);

  Layout();
  SchedulePaint();
}

void Tab::UpdateLoadingAnimation(TabRendererData::NetworkState state) {
  if (state == data_.network_state &&
      state == TabRendererData::NETWORK_STATE_NONE) {
    // If the network state is none and hasn't changed, do nothing. Otherwise we
    // need to advance the animation frame.
    return;
  }

  data_.network_state = state;
  AdvanceLoadingAnimation();
}

void Tab::StartPulse() {
  pulse_animation_.reset(new gfx::ThrobAnimation(this));
  pulse_animation_->SetSlideDuration(kPulseDurationMs);
  if (animation_container_.get())
    pulse_animation_->SetContainer(animation_container_.get());
  pulse_animation_->StartThrobbing(std::numeric_limits<int>::max());
}

void Tab::StopPulse() {
  StopAndDeleteAnimation(std::move(pulse_animation_));
}

void Tab::StartPinnedTabTitleAnimation() {
  if (!data().pinned)
    return;
  if (!pinned_title_change_animation_) {
    gfx::MultiAnimation::Parts parts;
    parts.push_back(
        gfx::MultiAnimation::Part(kPinnedTitleChangeAnimationDuration1MS,
                                 gfx::Tween::EASE_OUT));
    parts.push_back(
        gfx::MultiAnimation::Part(kPinnedTitleChangeAnimationDuration2MS,
                                 gfx::Tween::ZERO));
    parts.push_back(
        gfx::MultiAnimation::Part(kPinnedTitleChangeAnimationDuration3MS,
                                 gfx::Tween::EASE_IN));
    parts[0].start_time_ms = kPinnedTitleChangeAnimationStart1MS;
    parts[0].end_time_ms = kPinnedTitleChangeAnimationEnd1MS;
    parts[2].start_time_ms = kPinnedTitleChangeAnimationStart3MS;
    parts[2].end_time_ms = kPinnedTitleChangeAnimationEnd3MS;
    base::TimeDelta timeout = base::TimeDelta::FromMilliseconds(
        kPinnedTitleChangeAnimationIntervalMS);
    pinned_title_change_animation_.reset(
        new gfx::MultiAnimation(parts, timeout));
    if (animation_container_.get())
      pinned_title_change_animation_->SetContainer(animation_container_.get());
    pinned_title_change_animation_->set_delegate(this);
  }
  pinned_title_change_animation_->Start();
}

void Tab::StopPinnedTabTitleAnimation() {
  StopAndDeleteAnimation(std::move(pinned_title_change_animation_));
}

int Tab::GetWidthOfLargestSelectableRegion() const {
  // Assume the entire region to the left of the media indicator and/or close
  // buttons is available for click-to-select.  If neither are visible, the
  // entire tab region is available.
  const int indicator_left = showing_media_indicator_ ?
      media_indicator_button_->x() : width();
  const int close_button_left = showing_close_button_ ?
      close_button_->x() : width();
  return std::min(indicator_left, close_button_left);
}

// static
gfx::Size Tab::GetMinimumInactiveSize() {
  return gfx::Size(GetLayoutInsets(TAB).width(), GetLayoutConstant(TAB_HEIGHT));
}

// static
gfx::Size Tab::GetMinimumActiveSize() {
  gfx::Size minimum_size = GetMinimumInactiveSize();
  minimum_size.Enlarge(gfx::kFaviconSize, 0);
  return minimum_size;
}

// static
gfx::Size Tab::GetStandardSize() {
  const int kNetTabWidth = 193;
  return gfx::Size(kNetTabWidth + GetLayoutConstant(TABSTRIP_TAB_OVERLAP),
                   GetMinimumInactiveSize().height());
}

// static
int Tab::GetTouchWidth() {
  return kTouchWidth;
}

// static
int Tab::GetPinnedWidth() {
  return GetMinimumInactiveSize().width() +
         GetLayoutConstant(TAB_PINNED_CONTENT_WIDTH);
}

// static
int Tab::GetImmersiveHeight() {
  return kImmersiveTabHeight;
}

// static
int Tab::GetYInsetForActiveTabBackground() {
  // The computed value here is strangely less than the height of the area atop
  // the tab that doesn't get a background painted; otherwise, we could compute
  // the value by simply using GetLayoutInsets(TAB).top().  My suspicion is that
  // originally there was some sort of off-by-one error in how this background
  // was painted, and theme authors compensated; now we're stuck perpetuating it
  // as a result.
  return GetLayoutConstant(TAB_TOP_EXCLUSION_HEIGHT) + 1;
}

// static
float Tab::GetInverseDiagonalSlope() {
  // This is computed from the border path as follows:
  // * The unscaled endcap width is enough for the whole stroke outer curve,
  //   i.e. the side diagonal plus the curves on both its ends.
  // * The bottom and top curve are each (2 * scale) px wide, so the diagonal is
  //   (unscaled endcap width - 2 - 2) * scale px wide.
  // * The bottom and top curve are each 1.5 px high.  Additionally, there is an
  //   extra 1 px below the bottom curve and (scale - 1) px above the top curve,
  //   so the diagonal is ((height - 1.5 - 1.5) * scale - 1 - (scale - 1)) px
  //   high.
  // Simplifying these gives the expression below.
  return (GetUnscaledEndcapWidth() - 4) /
      (GetMinimumInactiveSize().height() - 4);
}

////////////////////////////////////////////////////////////////////////////////
// Tab, AnimationDelegate overrides:

void Tab::AnimationProgressed(const gfx::Animation* animation) {
  // Ignore if the pulse animation is being performed on active tab because
  // it repaints the same image. See |Tab::PaintTabBackground()|.
  if (animation == pulse_animation_.get() && IsActive())
    return;
  SchedulePaint();
}

void Tab::AnimationCanceled(const gfx::Animation* animation) {
  SchedulePaint();
}

void Tab::AnimationEnded(const gfx::Animation* animation) {
  SchedulePaint();
}

////////////////////////////////////////////////////////////////////////////////
// Tab, views::ButtonListener overrides:

void Tab::ButtonPressed(views::Button* sender, const ui::Event& event) {
  if (media_indicator_button_ && media_indicator_button_->visible()) {
    if (media_indicator_button_->enabled())
      content::RecordAction(UserMetricsAction("CloseTab_MuteToggleAvailable"));
    else if (data_.media_state == TAB_MEDIA_STATE_AUDIO_PLAYING)
      content::RecordAction(UserMetricsAction("CloseTab_AudioIndicator"));
    else
      content::RecordAction(UserMetricsAction("CloseTab_RecordingIndicator"));
  } else {
    content::RecordAction(UserMetricsAction("CloseTab_NoMediaIndicator"));
  }

  const CloseTabSource source =
      (event.type() == ui::ET_MOUSE_RELEASED &&
       (event.flags() & ui::EF_FROM_TOUCH) == 0) ? CLOSE_TAB_FROM_MOUSE :
      CLOSE_TAB_FROM_TOUCH;
  DCHECK_EQ(close_button_, sender);
  controller_->CloseTab(this, source);
  if (event.type() == ui::ET_GESTURE_TAP)
    TouchUMA::RecordGestureAction(TouchUMA::GESTURE_TABCLOSE_TAP);
}

////////////////////////////////////////////////////////////////////////////////
// Tab, views::ContextMenuController overrides:

void Tab::ShowContextMenuForView(views::View* source,
                                 const gfx::Point& point,
                                 ui::MenuSourceType source_type) {
  if (!closing())
    controller_->ShowContextMenuForTab(this, point, source_type);
}

////////////////////////////////////////////////////////////////////////////////
// Tab, views::MaskedTargeterDelegate overrides:

bool Tab::GetHitTestMask(gfx::Path* mask) const {
  DCHECK(mask);

  // When the window is maximized we don't want to shave off the edges or top
  // shadow of the tab, such that the user can click anywhere along the top
  // edge of the screen to select a tab. Ditto for immersive fullscreen.
  const views::Widget* widget = GetWidget();
  const bool extend_to_top =
      widget && (widget->IsMaximized() || widget->IsFullscreen());

  if (ui::MaterialDesignController::IsModeMaterial()) {
    SkPath border;
    const float scale = GetWidget()->GetCompositor()->device_scale_factor();
    GetBorderPath(scale, extend_to_top, &border);
    mask->addPath(border, SkMatrix::MakeScale(1 / scale));
  } else {
    // Hit mask constants.
    const SkScalar kTabCapWidth = 15;
    const SkScalar kTabTopCurveWidth = 4;
    const SkScalar kTabBottomCurveWidth = 3;
#if defined(OS_MACOSX)
    // Mac's Cocoa UI doesn't have shadows.
    const SkScalar kTabInset = 0;
#elif defined(TOOLKIT_VIEWS)
    // The views browser UI has shadows in the left, right and top parts of the
    // tab.
    const SkScalar kTabInset = 6;
#endif

    SkScalar left = kTabInset;
    SkScalar top = GetLayoutConstant(TAB_TOP_EXCLUSION_HEIGHT);
    SkScalar right = SkIntToScalar(width()) - kTabInset;
    SkScalar bottom = SkIntToScalar(height());

    // Start in the lower-left corner.
    mask->moveTo(left, bottom);

    // Left end cap.
    mask->lineTo(left + kTabBottomCurveWidth, bottom - kTabBottomCurveWidth);
    mask->lineTo(left + kTabCapWidth - kTabTopCurveWidth,
                 top + kTabTopCurveWidth);
    mask->lineTo(left + kTabCapWidth, top);

    // Extend over the top shadow area if we have one and the caller wants it.
    if (top > 0 && extend_to_top) {
      mask->lineTo(left + kTabCapWidth, 0);
      mask->lineTo(right - kTabCapWidth, 0);
    }

    // Connect to the right cap.
    mask->lineTo(right - kTabCapWidth, top);

    // Right end cap.
    mask->lineTo(right - kTabCapWidth + kTabTopCurveWidth,
                 top + kTabTopCurveWidth);
    mask->lineTo(right - kTabBottomCurveWidth, bottom - kTabBottomCurveWidth);
    mask->lineTo(right, bottom);

    // Close out the path.
    mask->lineTo(left, bottom);
    mask->close();
  }

  // It is possible for a portion of the tab to be occluded if tabs are
  // stacked, so modify the hit test mask to only include the visible
  // region of the tab.
  gfx::Rect clip;
  controller_->ShouldPaintTab(this, &clip);
  if (clip.size().GetArea()) {
    SkRect intersection(mask->getBounds());
    mask->reset();
    if (!intersection.intersect(RectToSkRect(clip)))
       return false;
    mask->addRect(intersection);
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// Tab, views::View overrides:

void Tab::ViewHierarchyChanged(const ViewHierarchyChangedDetails& details) {
  // If this hierarchy changed has resulted in us being part of a widget
  // hierarchy for the first time, we can now get at the theme provider, and
  // should recalculate the button color.
  if (details.is_add)
    OnButtonColorMaybeChanged();
}

void Tab::OnPaint(gfx::Canvas* canvas) {
  // Don't paint if we're narrower than we can render correctly. (This should
  // only happen during animations).
  if (width() < GetMinimumInactiveSize().width() && !data().pinned)
    return;

  gfx::Rect clip;
  if (!controller_->ShouldPaintTab(this, &clip))
    return;
  if (!clip.IsEmpty()) {
    canvas->Save();
    canvas->ClipRect(clip);
  }

  if (controller_->IsImmersiveStyle())
    PaintImmersiveTab(canvas);
  else
    PaintTab(canvas);

  if (!clip.IsEmpty())
    canvas->Restore();
}

void Tab::Layout() {
  const gfx::Rect lb = GetContentsBounds();
  showing_icon_ = ShouldShowIcon();
  // See comments in IconCapacity().
  const int extra_padding =
      (controller_->ShouldHideCloseButtonForInactiveTabs() ||
       (IconCapacity() < 3)) ? 0 : kExtraLeftPaddingToBalanceCloseButtonPadding;
  const int start = lb.x() + extra_padding;
  favicon_bounds_.SetRect(start, lb.y(), 0, 0);
  if (showing_icon_) {
    favicon_bounds_.set_size(gfx::Size(gfx::kFaviconSize, gfx::kFaviconSize));
    favicon_bounds_.set_y(lb.y() + (lb.height() - gfx::kFaviconSize + 1) / 2);
    MaybeAdjustLeftForPinnedTab(&favicon_bounds_);
  }
  throbber_->SetBoundsRect(favicon_bounds_);

  showing_close_button_ = ShouldShowCloseBox();
  if (showing_close_button_) {
    // If the ratio of the close button size to tab width exceeds the maximum.
    // The close button should be as large as possible so that there is a larger
    // hit-target for touch events. So the close button bounds extends to the
    // edges of the tab. However, the larger hit-target should be active only
    // for mouse events, and the close-image should show up in the right place.
    // So a border is added to the button with necessary padding. The close
    // button (BaseTab::TabCloseButton) makes sure the padding is a hit-target
    // only for touch events.
    close_button_->SetBorder(views::Border::NullBorder());
    const gfx::Size close_button_size(close_button_->GetPreferredSize());
    const int top = lb.y() + (lb.height() - close_button_size.height() + 1) / 2;
    const int left = kAfterTitleSpacing;
    const int close_button_end = lb.right() +
        GetLayoutConstant(TAB_CLOSE_BUTTON_TRAILING_PADDING_OVERLAP);
    close_button_->SetPosition(
        gfx::Point(close_button_end - close_button_size.width() - left, 0));
    const int bottom = height() - close_button_size.height() - top;
    const int right = width() - close_button_end;
    close_button_->SetBorder(
        views::Border::CreateEmptyBorder(top, left, bottom, right));
    close_button_->SizeToPreferredSize();
  }
  close_button_->SetVisible(showing_close_button_);

  showing_media_indicator_ = ShouldShowMediaIndicator();
  if (showing_media_indicator_) {
    const gfx::Size image_size(media_indicator_button_->GetPreferredSize());
    const int right = showing_close_button_ ?
        close_button_->x() + close_button_->GetInsets().left() : lb.right();
    gfx::Rect bounds(
        std::max(lb.x(), right - image_size.width()),
        lb.y() + (lb.height() - image_size.height() + 1) / 2,
        image_size.width(),
        image_size.height());
    MaybeAdjustLeftForPinnedTab(&bounds);
    media_indicator_button_->SetBoundsRect(bounds);
  }
  media_indicator_button_->SetVisible(showing_media_indicator_);

  // Size the title to fill the remaining width and use all available height.
  const bool show_title = ShouldRenderAsNormalTab();
  if (show_title) {
    const int title_spacing = GetLayoutConstant(TAB_FAVICON_TITLE_SPACING);
    int title_left = showing_icon_ ?
        (favicon_bounds_.right() + title_spacing) : start;
    int title_width = lb.right() - title_left;
    if (showing_media_indicator_) {
      title_width =
          media_indicator_button_->x() - kAfterTitleSpacing - title_left;
    } else if (close_button_->visible()) {
      // Allow the title to overlay the close button's empty border padding.
      title_width = close_button_->x() + close_button_->GetInsets().left() -
          kAfterTitleSpacing - title_left;
    }
    // The Label will automatically center the font's cap height within the
    // provided vertical space.
    title_->SetBoundsRect(
        gfx::Rect(title_left, lb.y(), std::max(title_width, 0), lb.height()));
  }
  title_->SetVisible(show_title);
}

void Tab::OnThemeChanged() {
  LoadTabImages();
  OnButtonColorMaybeChanged();
}

const char* Tab::GetClassName() const {
  return kViewClassName;
}

bool Tab::GetTooltipText(const gfx::Point& p, base::string16* tooltip) const {
  // Note: Anything that affects the tooltip text should be accounted for when
  // calling TooltipTextChanged() from Tab::DataChanged().
  *tooltip = chrome::AssembleTabTooltipText(data_.title, data_.media_state);
  return !tooltip->empty();
}

bool Tab::GetTooltipTextOrigin(const gfx::Point& p, gfx::Point* origin) const {
  origin->set_x(title_->x() + 10);
  origin->set_y(-4);
  return true;
}

bool Tab::OnMousePressed(const ui::MouseEvent& event) {
  controller_->OnMouseEventInTab(this, event);

  // Allow a right click from touch to drag, which corresponds to a long click.
  if (event.IsOnlyLeftMouseButton() ||
      (event.IsOnlyRightMouseButton() && event.flags() & ui::EF_FROM_TOUCH)) {
    ui::ListSelectionModel original_selection;
    original_selection.Copy(controller_->GetSelectionModel());
    // Changing the selection may cause our bounds to change. If that happens
    // the location of the event may no longer be valid. Create a copy of the
    // event in the parents coordinate, which won't change, and recreate an
    // event after changing so the coordinates are correct.
    ui::MouseEvent event_in_parent(event, static_cast<View*>(this), parent());
    if (controller_->SupportsMultipleSelection()) {
      if (event.IsShiftDown() && event.IsControlDown()) {
        controller_->AddSelectionFromAnchorTo(this);
      } else if (event.IsShiftDown()) {
        controller_->ExtendSelectionTo(this);
      } else if (event.IsControlDown()) {
        controller_->ToggleSelected(this);
        if (!IsSelected()) {
          // Don't allow dragging non-selected tabs.
          return false;
        }
      } else if (!IsSelected()) {
        controller_->SelectTab(this);
        content::RecordAction(UserMetricsAction("SwitchTab_Click"));
      }
    } else if (!IsSelected()) {
      controller_->SelectTab(this);
      content::RecordAction(UserMetricsAction("SwitchTab_Click"));
    }
    ui::MouseEvent cloned_event(event_in_parent, parent(),
                                static_cast<View*>(this));
    controller_->MaybeStartDrag(this, cloned_event, original_selection);
  }
  return true;
}

bool Tab::OnMouseDragged(const ui::MouseEvent& event) {
  controller_->ContinueDrag(this, event);
  return true;
}

void Tab::OnMouseReleased(const ui::MouseEvent& event) {
  controller_->OnMouseEventInTab(this, event);

  // Notify the drag helper that we're done with any potential drag operations.
  // Clean up the drag helper, which is re-created on the next mouse press.
  // In some cases, ending the drag will schedule the tab for destruction; if
  // so, bail immediately, since our members are already dead and we shouldn't
  // do anything else except drop the tab where it is.
  if (controller_->EndDrag(END_DRAG_COMPLETE))
    return;

  // Close tab on middle click, but only if the button is released over the tab
  // (normal windows behavior is to discard presses of a UI element where the
  // releases happen off the element).
  if (event.IsMiddleMouseButton()) {
    if (HitTestPoint(event.location())) {
      controller_->CloseTab(this, CLOSE_TAB_FROM_MOUSE);
    } else if (closing_) {
      // We're animating closed and a middle mouse button was pushed on us but
      // we don't contain the mouse anymore. We assume the user is clicking
      // quicker than the animation and we should close the tab that falls under
      // the mouse.
      Tab* closest_tab = controller_->GetTabAt(this, event.location());
      if (closest_tab)
        controller_->CloseTab(closest_tab, CLOSE_TAB_FROM_MOUSE);
    }
  } else if (event.IsOnlyLeftMouseButton() && !event.IsShiftDown() &&
             !event.IsControlDown()) {
    // If the tab was already selected mouse pressed doesn't change the
    // selection. Reset it now to handle the case where multiple tabs were
    // selected.
    controller_->SelectTab(this);

    if (media_indicator_button_ && media_indicator_button_->visible() &&
        media_indicator_button_->bounds().Contains(event.location())) {
      content::RecordAction(UserMetricsAction("TabMediaIndicator_Clicked"));
    }
  }
}

void Tab::OnMouseCaptureLost() {
  controller_->EndDrag(END_DRAG_CAPTURE_LOST);
}

void Tab::OnMouseEntered(const ui::MouseEvent& event) {
  hover_controller_.Show(views::GlowHoverController::SUBTLE);
}

void Tab::OnMouseMoved(const ui::MouseEvent& event) {
  hover_controller_.SetLocation(event.location());
  controller_->OnMouseEventInTab(this, event);
}

void Tab::OnMouseExited(const ui::MouseEvent& event) {
  hover_controller_.Hide();
}

void Tab::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::ET_GESTURE_TAP_DOWN: {
      // TAP_DOWN is only dispatched for the first touch point.
      DCHECK_EQ(1, event->details().touch_points());

      // See comment in OnMousePressed() as to why we copy the event.
      ui::GestureEvent event_in_parent(*event, static_cast<View*>(this),
                                       parent());
      ui::ListSelectionModel original_selection;
      original_selection.Copy(controller_->GetSelectionModel());
      tab_activated_with_last_tap_down_ = !IsActive();
      if (!IsSelected())
        controller_->SelectTab(this);
      gfx::Point loc(event->location());
      views::View::ConvertPointToScreen(this, &loc);
      ui::GestureEvent cloned_event(event_in_parent, parent(),
                                    static_cast<View*>(this));
      controller_->MaybeStartDrag(this, cloned_event, original_selection);
      break;
    }

    case ui::ET_GESTURE_END:
      controller_->EndDrag(END_DRAG_COMPLETE);
      break;

    case ui::ET_GESTURE_SCROLL_UPDATE:
      controller_->ContinueDrag(this, *event);
      break;

    default:
      break;
  }
  event->SetHandled();
}

void Tab::GetAccessibleState(ui::AXViewState* state) {
  state->role = ui::AX_ROLE_TAB;
  state->name = data_.title;
  state->AddStateFlag(ui::AX_STATE_MULTISELECTABLE);
  state->AddStateFlag(ui::AX_STATE_SELECTABLE);
  controller_->UpdateTabAccessibilityState(this, state);
  if (IsSelected())
    state->AddStateFlag(ui::AX_STATE_SELECTED);
}

////////////////////////////////////////////////////////////////////////////////
// Tab, private

void Tab::MaybeAdjustLeftForPinnedTab(gfx::Rect* bounds) const {
  if (ShouldRenderAsNormalTab())
    return;
  const int ideal_delta = width() - GetPinnedWidth();
  const int ideal_x = (GetPinnedWidth() - bounds->width()) / 2;
  bounds->set_x(
      bounds->x() + static_cast<int>(
          (1 - static_cast<float>(ideal_delta) /
              static_cast<float>(kPinnedTabExtraWidthToRenderAsNormal)) *
          (ideal_x - bounds->x())));
}

void Tab::DataChanged(const TabRendererData& old) {
  if (data().media_state != old.media_state || data().title != old.title)
    TooltipTextChanged();

  if (data().blocked == old.blocked)
    return;

  if (data().blocked)
    StartPulse();
  else
    StopPulse();
}

void Tab::PaintTab(gfx::Canvas* canvas) {
  PaintTabBackground(canvas);

  if (showing_icon_)
    PaintIcon(canvas);
}

void Tab::PaintImmersiveTab(gfx::Canvas* canvas) {
  // Use transparency for the draw-attention animation.
  int alpha = 255;
  if (pulse_animation_ && pulse_animation_->is_animating() && !data().pinned) {
    alpha = pulse_animation_->CurrentValueBetween(
        255, static_cast<int>(255 * kImmersiveTabMinThrobOpacity));
  }

  // Draw a gray rectangle to represent the tab. This works for pinned tabs as
  // well as regular ones. The active tab has a brigher bar.
  SkColor color =
      IsActive() ? kImmersiveActiveTabColor : kImmersiveInactiveTabColor;
  gfx::Rect bar_rect = GetImmersiveBarRect();
  canvas->FillRect(bar_rect, SkColorSetA(color, alpha));

  // Paint network activity indicator.
  // TODO(jamescook): Replace this placeholder animation with a real one.
  // For now, let's go with a Cylon eye effect, but in blue.
  if (data().network_state != TabRendererData::NETWORK_STATE_NONE) {
    const SkColor kEyeColor = SkColorSetARGB(alpha, 71, 138, 217);
    int eye_width = bar_rect.width() / 3;
    int eye_offset = bar_rect.width() * immersive_loading_step_ /
        kImmersiveLoadingStepCount;
    if (eye_offset + eye_width < bar_rect.width()) {
      // Draw a single indicator strip because it fits inside |bar_rect|.
      gfx::Rect eye_rect(
          bar_rect.x() + eye_offset, 0, eye_width, kImmersiveBarHeight);
      canvas->FillRect(eye_rect, kEyeColor);
    } else {
      // Draw two indicators to simulate the eye "wrapping around" to the left
      // side. The first part fills the remainder of the bar.
      int right_eye_width = bar_rect.width() - eye_offset;
      gfx::Rect right_eye_rect(
          bar_rect.x() + eye_offset, 0, right_eye_width, kImmersiveBarHeight);
      canvas->FillRect(right_eye_rect, kEyeColor);
      // The second part parts the remaining |eye_width| on the left.
      int left_eye_width = eye_offset + eye_width - bar_rect.width();
      gfx::Rect left_eye_rect(
          bar_rect.x(), 0, left_eye_width, kImmersiveBarHeight);
      canvas->FillRect(left_eye_rect, kEyeColor);
    }
  }
}

void Tab::PaintTabBackground(gfx::Canvas* canvas) {
  const int kActiveTabFillId = IDR_THEME_TOOLBAR;
  const bool has_custom_image =
      GetThemeProvider()->HasCustomImage(kActiveTabFillId);
  const int y_offset = -GetYInsetForActiveTabBackground();
  if (IsActive()) {
    PaintTabBackgroundUsingFillId(canvas, true, kActiveTabFillId,
                                  has_custom_image, y_offset);
  } else {
    if (pinned_title_change_animation_ &&
        pinned_title_change_animation_->is_animating())
      PaintInactiveTabBackgroundWithTitleChange(canvas);
    else
      PaintInactiveTabBackground(canvas);

    const double throb_value = GetThrobValue();
    if (throb_value > 0) {
      canvas->SaveLayerAlpha(gfx::ToRoundedInt(throb_value * 0xff),
                             GetLocalBounds());
      PaintTabBackgroundUsingFillId(canvas, true, kActiveTabFillId,
                                    has_custom_image, y_offset);
      canvas->Restore();
    }
  }
}

void Tab::PaintInactiveTabBackgroundWithTitleChange(gfx::Canvas* canvas) {
  const int kPinnedTitleChangeGradientRadius = 20;
  const float radius = kPinnedTitleChangeGradientRadius;
  double x = radius;
  int alpha = 255;
  if (pinned_title_change_animation_->current_part_index() == 0) {
    x = pinned_title_change_animation_->CurrentValueBetween(
            width() + radius - kPinnedTitleChangeInitialXOffset, radius);
  } else if (pinned_title_change_animation_->current_part_index() == 2) {
    x = pinned_title_change_animation_->CurrentValueBetween(radius, -radius);
    alpha = pinned_title_change_animation_->CurrentValueBetween(255, 0);
  }
  SkPoint p;
  p.set(SkDoubleToScalar(x), 0);
  if (ui::MaterialDesignController::IsModeMaterial()) {
    PaintInactiveTabBackground(canvas);
    gfx::ScopedCanvas scoped_canvas(canvas);
    const float scale = canvas->UndoDeviceScaleFactor();
    SkPath fill;
    GetFillPath(scale, &fill);
    canvas->ClipPath(fill, true);
    p.scale(SkFloatToScalar(scale));
    DrawHighlight(canvas, p, SkFloatToScalar(radius * scale), alpha);
  } else {
    gfx::Canvas background_canvas(size(), canvas->image_scale(), false);
    PaintInactiveTabBackground(&background_canvas);
    gfx::ImageSkia background_image(background_canvas.ExtractImageRep());
    canvas->DrawImageInt(background_image, 0, 0);
    gfx::Canvas hover_canvas(size(), canvas->image_scale(), false);
    DrawHighlight(&hover_canvas, p, SkFloatToScalar(radius), alpha);
    gfx::ImageSkia hover_image = gfx::ImageSkiaOperations::CreateMaskedImage(
        gfx::ImageSkia(hover_canvas.ExtractImageRep()), background_image);
    canvas->DrawImageInt(hover_image, 0, 0);
  }
}

void Tab::PaintInactiveTabBackground(gfx::Canvas* canvas) {
  bool has_custom_image;
  int fill_id = controller_->GetBackgroundResourceId(&has_custom_image);

  // If the theme is providing a custom background image, then its top edge
  // should be at the top of the tab. Otherwise, we assume that the background
  // image is a composited foreground + frame image.  Note that if the theme is
  // only providing a custom frame image, |has_custom_image| will be true, but
  // we should use the |background_offset_| here.
  const int y_offset = GetThemeProvider()->HasCustomImage(fill_id) ?
      -GetLayoutConstant(TAB_TOP_EXCLUSION_HEIGHT) : background_offset_.y();

  // We only cache the image when it's the default image and we're not hovered,
  // to avoid caching a background image that isn't the same for all tabs.
  if (has_custom_image || hover_controller_.ShouldDraw()) {
    PaintTabBackgroundUsingFillId(canvas, false, fill_id, has_custom_image,
                                  y_offset);
    return;
  }

  const ImageCacheEntryMetadata metadata(
      ui::MaterialDesignController::IsModeMaterial() &&
          controller_->IsIncognito(),
      fill_id, ui::GetSupportedScaleFactor(canvas->image_scale()), size());
  auto it = std::find_if(
      image_cache_->begin(), image_cache_->end(),
      [&metadata](const ImageCacheEntry& e) { return e.metadata == metadata; });
  if (it == image_cache_->end()) {
    gfx::Canvas tmp_canvas(size(), canvas->image_scale(), false);
    PaintTabBackgroundUsingFillId(&tmp_canvas, false, fill_id, false, y_offset);
    image_cache_->emplace_front(metadata,
                                gfx::ImageSkia(tmp_canvas.ExtractImageRep()));
    if (image_cache_->size() > kMaxImageCacheSize)
      image_cache_->pop_back();
    it = image_cache_->begin();
  }
  canvas->DrawImageInt(it->image, 0, 0);
}

void Tab::PaintTabBackgroundUsingFillId(gfx::Canvas* canvas,
                                        bool is_active,
                                        int fill_id,
                                        bool has_custom_image,
                                        int y_offset) {
  const ui::ThemeProvider* tp = GetThemeProvider();
  gfx::ImageSkia* fill_image = tp->GetImageSkiaNamed(fill_id);
  // The tab image needs to be lined up with the background image
  // so that it feels partially transparent.  These offsets represent the tab
  // position within the frame background image.
  const int x_offset = GetMirroredX() + background_offset_.x();

  const SkScalar kMinHoverRadius = 16;
  const SkScalar radius =
      std::max(SkFloatToScalar(width() / 4.f), kMinHoverRadius);
  const bool draw_hover = !is_active && hover_controller_.ShouldDraw();
  SkPoint hover_location(PointToSkPoint(hover_controller_.location()));
  const SkAlpha hover_alpha = hover_controller_.GetAlpha();

  if (ui::MaterialDesignController::IsModeMaterial()) {
    gfx::ScopedCanvas scoped_canvas(canvas);
    const float scale = canvas->UndoDeviceScaleFactor();

    // Draw the fill.
    SkPath fill;
    GetFillPath(scale, &fill);
    SkPaint paint;
    paint.setAntiAlias(true);
    {
      gfx::ScopedCanvas clip_scoper(canvas);
      canvas->ClipPath(fill, true);
      if (has_custom_image) {
        gfx::ScopedCanvas scale_scoper(canvas);
        canvas->sk_canvas()->scale(scale, scale);
        canvas->TileImageInt(*fill_image, x_offset, y_offset, 0, 0, width(),
                             height());
      } else {
        paint.setColor(tp->GetColor(is_active ?
            static_cast<int>(ThemeProperties::COLOR_TOOLBAR) :
            ThemeProperties::COLOR_BACKGROUND_TAB));
        canvas->DrawRect(gfx::ScaleToEnclosingRect(GetLocalBounds(), scale),
                         paint);
      }
      if (draw_hover) {
        hover_location.scale(SkFloatToScalar(scale));
        DrawHighlight(canvas, hover_location, radius * scale, hover_alpha);
      }
    }

    // Draw the stroke.
    SkPath stroke;
    GetBorderPath(scale, false, &stroke);
    Op(stroke, fill, kDifference_SkPathOp, &stroke);
    if (!is_active) {
      // Clip out the bottom line; this will be drawn for us by
      // TabStrip::PaintChildren().
      canvas->sk_canvas()->clipRect(
          SkRect::MakeWH(width() * scale, height() * scale - 1));
    }
    paint.setARGB(0x40, 0x00, 0x00, 0x00);
    canvas->DrawPath(stroke, paint);
  } else {
    if (draw_hover) {
      // Draw everything to a temporary canvas so we can extract an image for
      // use in masking the hover glow.
      gfx::Canvas background_canvas(size(), canvas->image_scale(), false);
      PaintTabFill(&background_canvas, fill_image, x_offset, y_offset,
                   is_active);
      gfx::ImageSkia background_image(background_canvas.ExtractImageRep());
      canvas->DrawImageInt(background_image, 0, 0);

      gfx::Canvas hover_canvas(size(), canvas->image_scale(), false);
      DrawHighlight(&hover_canvas, hover_location, radius, hover_alpha);
      gfx::ImageSkia result = gfx::ImageSkiaOperations::CreateMaskedImage(
          gfx::ImageSkia(hover_canvas.ExtractImageRep()), background_image);
      canvas->DrawImageInt(result, 0, 0);
    } else {
      PaintTabFill(canvas, fill_image, x_offset, y_offset, is_active);
    }

    // Now draw the stroke, highlights, and shadows around the tab edge.
    TabImages* stroke_images = is_active ? &active_images_ : &inactive_images_;
    canvas->DrawImageInt(*stroke_images->image_l, 0, 0);
    canvas->TileImageInt(
        *stroke_images->image_c, stroke_images->l_width, 0,
        width() - stroke_images->l_width - stroke_images->r_width, height());
    canvas->DrawImageInt(*stroke_images->image_r,
                         width() - stroke_images->r_width, 0);
  }
}

void Tab::PaintTabFill(gfx::Canvas* canvas,
                       gfx::ImageSkia* fill_image,
                       int x_offset,
                       int y_offset,
                       bool is_active) {
  const gfx::Insets tab_insets(GetLayoutInsets(TAB));
  // If this isn't the foreground tab, don't draw over the toolbar, but do
  // include the 1 px divider stroke at the bottom.
  const int toolbar_overlap = is_active ? 0 : (tab_insets.bottom() - 1);

  // Draw left edge.
  gfx::ImageSkia tab_l = gfx::ImageSkiaOperations::CreateTiledImage(
      *fill_image, x_offset, y_offset, mask_images_.l_width, height());
  gfx::ImageSkia theme_l =
      gfx::ImageSkiaOperations::CreateMaskedImage(tab_l, *mask_images_.image_l);
  canvas->DrawImageInt(
      theme_l, 0, 0, theme_l.width(), theme_l.height() - toolbar_overlap, 0, 0,
      theme_l.width(), theme_l.height() - toolbar_overlap, false);

  // Draw right edge.
  gfx::ImageSkia tab_r = gfx::ImageSkiaOperations::CreateTiledImage(
      *fill_image, x_offset + width() - mask_images_.r_width, y_offset,
      mask_images_.r_width, height());
  gfx::ImageSkia theme_r =
      gfx::ImageSkiaOperations::CreateMaskedImage(tab_r, *mask_images_.image_r);
  canvas->DrawImageInt(theme_r, 0, 0, theme_r.width(),
                       theme_r.height() - toolbar_overlap,
                       width() - theme_r.width(), 0, theme_r.width(),
                       theme_r.height() - toolbar_overlap, false);

  // Draw center.  Instead of masking out the top portion we simply skip over it
  // by incrementing by the top padding, since it's a simple rectangle.
  canvas->TileImageInt(*fill_image, x_offset + mask_images_.l_width,
                       y_offset + tab_insets.top(), mask_images_.l_width,
                       tab_insets.top(),
                       width() - mask_images_.l_width - mask_images_.r_width,
                       height() - tab_insets.top() - toolbar_overlap);
}

void Tab::PaintIcon(gfx::Canvas* canvas) {
  gfx::Rect bounds = favicon_bounds_;
  bounds.set_x(GetMirroredXForRect(bounds));
  bounds.Offset(0, favicon_hiding_offset_);
  bounds.Intersect(GetContentsBounds());
  if (bounds.IsEmpty())
    return;

  if (data().network_state != TabRendererData::NETWORK_STATE_NONE) {
    // Throbber will do its own painting.
  } else {
    const gfx::ImageSkia& favicon = should_display_crashed_favicon_ ?
        *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            IDR_CRASH_SAD_FAVICON) :
        data().favicon;
    if (!favicon.isNull()) {
      canvas->DrawImageInt(favicon, 0, 0, bounds.width(), bounds.height(),
                           bounds.x(), bounds.y(), bounds.width(),
                           bounds.height(), false);
    }
  }
}

void Tab::AdvanceLoadingAnimation() {
  const TabRendererData::NetworkState state = data().network_state;
  if (controller_->IsImmersiveStyle()) {
    if (state == TabRendererData::NETWORK_STATE_WAITING) {
      // Waiting steps backwards.
      immersive_loading_step_ =
          (immersive_loading_step_ - 1 + kImmersiveLoadingStepCount) %
          kImmersiveLoadingStepCount;
    } else if (state == TabRendererData::NETWORK_STATE_LOADING) {
      immersive_loading_step_ =
          (immersive_loading_step_ + 1) % kImmersiveLoadingStepCount;
    } else {
      immersive_loading_step_ = 0;
    }

    SchedulePaintInRect(GetImmersiveBarRect());
    return;
  }

  if (state == TabRendererData::NETWORK_STATE_NONE) {
    throbber_->ResetStartTimes();
    throbber_->SetVisible(false);
    ScheduleIconPaint();
    return;
  }

  // Since the throbber can animate for a long time, paint to a separate layer
  // when possible to reduce repaint overhead.
  const bool paint_to_layer = controller_->CanPaintThrobberToLayer();
  if (paint_to_layer != !!throbber_->layer()) {
    throbber_->SetPaintToLayer(paint_to_layer);
    throbber_->SetFillsBoundsOpaquely(false);
    if (paint_to_layer)
      ScheduleIconPaint();  // Ensure the non-layered throbber goes away.
  }
  if (!throbber_->visible()) {
    ScheduleIconPaint();  // Repaint the icon area to hide the favicon.
    throbber_->SetVisible(true);
  }
  throbber_->SchedulePaint();
}

int Tab::IconCapacity() const {
  const gfx::Size min_size(GetMinimumInactiveSize());
  if (height() < min_size.height())
    return 0;
  const int available_width = std::max(0, width() - min_size.width());
  // All icons are the same size as the favicon.
  const int icon_width = gfx::kFaviconSize;
  // We need enough space to display the icons flush against each other.
  const int visible_icons = available_width / icon_width;
  // When the close button will be visible on inactive tabs, we add additional
  // padding to the left of the favicon to balance the whitespace inside the
  // non-hovered close button image; otherwise, the tab contents look too close
  // to the left edge.  If the tab close button isn't visible on inactive tabs,
  // we let the tab contents take the full width of the tab, to maximize visible
  // content on tiny tabs.  We base the determination on the inactive tab close
  // button state so that when a tab is activated its contents don't suddenly
  // shift.
  if (visible_icons < 3)
    return visible_icons;
  const int padding = controller_->ShouldHideCloseButtonForInactiveTabs() ?
      0 : kExtraLeftPaddingToBalanceCloseButtonPadding;
  return (available_width - padding) / icon_width;
}

bool Tab::ShouldShowIcon() const {
  return chrome::ShouldTabShowFavicon(
      IconCapacity(), data().pinned, IsActive(), data().show_icon,
      media_indicator_button_ ? media_indicator_button_->showing_media_state() :
                                data_.media_state);
}

bool Tab::ShouldShowMediaIndicator() const {
  return chrome::ShouldTabShowMediaIndicator(
      IconCapacity(), data().pinned, IsActive(), data().show_icon,
      media_indicator_button_ ? media_indicator_button_->showing_media_state() :
                                data_.media_state);
}

bool Tab::ShouldShowCloseBox() const {
  if (!IsActive() && controller_->ShouldHideCloseButtonForInactiveTabs())
    return false;

  return chrome::ShouldTabShowCloseButton(
      IconCapacity(), data().pinned, IsActive());
}

bool Tab::ShouldRenderAsNormalTab() const {
  return !data().pinned ||
      (width() >= (GetPinnedWidth() + kPinnedTabExtraWidthToRenderAsNormal));
}

double Tab::GetThrobValue() {
  const bool is_selected = IsSelected();
  double val = is_selected ? kSelectedTabOpacity : 0;
  const double offset =
      is_selected ? (kSelectedTabThrobScale * kHoverOpacity) : kHoverOpacity;

  // Showing both the pulse and title change animation at the same time is too
  // much.
  if (pulse_animation_ && pulse_animation_->is_animating() &&
      (!pinned_title_change_animation_ ||
       !pinned_title_change_animation_->is_animating())) {
    val += pulse_animation_->GetCurrentValue() * offset;
  } else if (hover_controller_.ShouldDraw()) {
    val += hover_controller_.GetAnimationValue() * offset;
  }
  return val;
}

void Tab::SetFaviconHidingOffset(int offset) {
  favicon_hiding_offset_ = offset;
  ScheduleIconPaint();
}

void Tab::OnButtonColorMaybeChanged() {
  // The theme provider may be null if we're not currently in a widget
  // hierarchy.
  const ui::ThemeProvider* theme_provider = GetThemeProvider();
  if (!theme_provider)
    return;

  const SkColor title_color = theme_provider->GetColor(IsActive() ?
      ThemeProperties::COLOR_TAB_TEXT :
      ThemeProperties::COLOR_BACKGROUND_TAB_TEXT);
  const SkColor new_button_color = SkColorSetA(title_color, 0xA0);
  if (button_color_ != new_button_color) {
    button_color_ = new_button_color;
    title_->SetEnabledColor(title_color);
    media_indicator_button_->OnParentTabButtonColorChanged();
    const gfx::ImageSkia& close_button_normal_image = gfx::CreateVectorIcon(
        gfx::VectorIconId::TAB_CLOSE_NORMAL, kTabCloseButtonSize,
        button_color_);
    close_button_->SetImage(views::CustomButton::STATE_NORMAL,
                            &close_button_normal_image);
  }
}

void Tab::ScheduleIconPaint() {
  gfx::Rect bounds = favicon_bounds_;
  if (bounds.IsEmpty())
    return;

  // Extends the area to the bottom when the crash animation is in progress.
  if (crash_icon_animation_)
    bounds.set_height(height() - bounds.y());
  bounds.set_x(GetMirroredXForRect(bounds));
  SchedulePaintInRect(bounds);
}

void Tab::GetFillPath(float scale, SkPath* fill) const {
  const float right = width() * scale;
  const float bottom = height() * scale;
  const float unscaled_endcap_width = GetUnscaledEndcapWidth();

  fill->moveTo(right - 1, bottom);
  fill->rCubicTo(-0.75 * scale, 0, -1.625 * scale, -0.5 * scale, -2 * scale,
                 -1.5 * scale);
  fill->lineTo(right - 1 - (unscaled_endcap_width - 2) * scale, 2.5 * scale);
  // Prevent overdraw in the center near minimum width (only happens if
  // scale < 2).  We could instead avoid this by increasing the tab inset
  // values, but that would shift all the content inward as well, unless we
  // then overlapped the content on the endcaps, by which point we'd have a
  // huge mess.
  const float scaled_endcap_width = 1 + unscaled_endcap_width * scale;
  const float overlap = scaled_endcap_width * 2 - right;
  const float offset = (overlap > 0) ? (overlap / 2) : 0;
  fill->rCubicTo(-0.375 * scale, -1 * scale, -1.25 * scale + offset,
                 -1.5 * scale, -2 * scale + offset, -1.5 * scale);
  if (overlap < 0)
    fill->lineTo(scaled_endcap_width, scale);
  fill->rCubicTo(-0.75 * scale, 0, -1.625 * scale - offset, 0.5 * scale,
                 -2 * scale - offset, 1.5 * scale);
  fill->lineTo(1 + 2 * scale, bottom - 1.5 * scale);
  fill->rCubicTo(-0.375 * scale, scale, -1.25 * scale, 1.5 * scale, -2 * scale,
                 1.5 * scale);
  fill->close();
}

void Tab::GetBorderPath(float scale, bool extend_to_top, SkPath* path) const {
  const float top = scale - 1;
  const float right = width() * scale;
  const float bottom = height() * scale;
  const float unscaled_endcap_width = GetUnscaledEndcapWidth();

  path->moveTo(0, bottom);
  path->rLineTo(0, -1);
  path->rCubicTo(0.75 * scale, 0, 1.625 * scale, -0.5 * scale, 2 * scale,
                 -1.5 * scale);
  path->lineTo((unscaled_endcap_width - 2) * scale, top + 1.5 * scale);
  if (extend_to_top) {
    // Create the vertical extension by extending the side diagonals until they
    // reach the top of the bounds.
    const float dy = 2.5 * scale - 1;
    const float dx = GetInverseDiagonalSlope() * dy;
    path->rLineTo(dx, -dy);
    path->lineTo(right - (unscaled_endcap_width - 2) * scale - dx, 0);
    path->rLineTo(dx, dy);
  } else {
    path->rCubicTo(0.375 * scale, -scale, 1.25 * scale, -1.5 * scale, 2 * scale,
                   -1.5 * scale);
    path->lineTo(right - unscaled_endcap_width * scale, top);
    path->rCubicTo(0.75 * scale, 0, 1.625 * scale, 0.5 * scale, 2 * scale,
                   1.5 * scale);
  }
  path->lineTo(right - 2 * scale, bottom - 1 - 1.5 * scale);
  path->rCubicTo(0.375 * scale, scale, 1.25 * scale, 1.5 * scale, 2 * scale,
                 1.5 * scale);
  path->rLineTo(0, 1);
  path->close();
}

gfx::Rect Tab::GetImmersiveBarRect() const {
  // The main bar is as wide as the normal tab's horizontal top line.
  // This top line of the tab extends a few pixels left and right of the
  // center image due to pixels in the rounded corner images.
  const int kBarPadding = 1;
  int main_bar_left = active_images_.l_width - kBarPadding;
  int main_bar_right = width() - active_images_.r_width + kBarPadding;
  return gfx::Rect(
      main_bar_left, 0, main_bar_right - main_bar_left, kImmersiveBarHeight);
}

////////////////////////////////////////////////////////////////////////////////
// Tab, private static:

// static
void Tab::InitTabResources() {
  static bool initialized = false;
  if (initialized)
    return;

  initialized = true;
  image_cache_ = new ImageCache();

  // Load the tab images once now, and maybe again later if the theme changes.
  LoadTabImages();
}

// static
void Tab::LoadTabImages() {
  // We're not letting people override tab images just yet.
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

  active_images_.image_l = rb.GetImageSkiaNamed(IDR_TAB_ACTIVE_LEFT);
  active_images_.image_c = rb.GetImageSkiaNamed(IDR_TAB_ACTIVE_CENTER);
  active_images_.image_r = rb.GetImageSkiaNamed(IDR_TAB_ACTIVE_RIGHT);
  active_images_.l_width = active_images_.image_l->width();
  active_images_.r_width = active_images_.image_r->width();

  inactive_images_.image_l = rb.GetImageSkiaNamed(IDR_TAB_INACTIVE_LEFT);
  inactive_images_.image_c = rb.GetImageSkiaNamed(IDR_TAB_INACTIVE_CENTER);
  inactive_images_.image_r = rb.GetImageSkiaNamed(IDR_TAB_INACTIVE_RIGHT);
  inactive_images_.l_width = inactive_images_.image_l->width();
  inactive_images_.r_width = inactive_images_.image_r->width();

  mask_images_.image_l = rb.GetImageSkiaNamed(IDR_TAB_ALPHA_LEFT);
  mask_images_.image_r = rb.GetImageSkiaNamed(IDR_TAB_ALPHA_RIGHT);
  mask_images_.l_width = mask_images_.image_l->width();
  mask_images_.r_width = mask_images_.image_r->width();
}
