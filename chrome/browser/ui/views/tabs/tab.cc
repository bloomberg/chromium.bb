// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab.h"

#include <limits>

#include "base/command_line.h"
#include "base/debug/alias.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_resources.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/tabs/tab_controller.h"
#include "chrome/common/chrome_switches.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/ui_resources.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/animation/animation_container.h"
#include "ui/base/animation/multi_animation.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/base/animation/throb_animation.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/layout.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/text/text_elider.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/font.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/path.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/widget/tooltip_manager.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

#if defined(OS_WIN)
#include "win8/util/win8_util.h"
#endif

#if defined(USE_ASH)
#include "ui/aura/env.h"
#endif

namespace {

// Padding around the "content" of a tab, occupied by the tab border graphics.

int left_padding() {
  static int value = -1;
  if (value == -1) {
    switch (ui::GetDisplayLayout()) {
      case ui::LAYOUT_ASH:
      case ui::LAYOUT_DESKTOP:
        value = 22;
        break;
      case ui::LAYOUT_TOUCH:
        value = 30;
        break;
      default:
        NOTREACHED();
    }
  }
  return value;
}

int top_padding() {
  static int value = -1;
  if (value == -1) {
    switch (ui::GetDisplayLayout()) {
      case ui::LAYOUT_ASH:
      case ui::LAYOUT_DESKTOP:
        value = 7;
        break;
      case ui::LAYOUT_TOUCH:
        value = 10;
        break;
      default:
        NOTREACHED();
    }
  }
  return value;
}

int right_padding() {
  static int value = -1;
  if (value == -1) {
    switch (ui::GetDisplayLayout()) {
      case ui::LAYOUT_ASH:
      case ui::LAYOUT_DESKTOP:
        value = 19;
        break;
      case ui::LAYOUT_TOUCH:
        value = 23;
        break;
      default:
        NOTREACHED();
    }
  }
  return value;
}

int bottom_padding() {
  static int value = -1;
  if (value == -1) {
    switch (ui::GetDisplayLayout()) {
      case ui::LAYOUT_ASH:
      case ui::LAYOUT_DESKTOP:
        value = 5;
        break;
      case ui::LAYOUT_TOUCH:
        value = 7;
        break;
      default:
        NOTREACHED();
    }
  }
  return value;
}

// Height of the shadow at the top of the tab image assets.
int drop_shadow_height() {
  static int value = -1;
  if (value == -1) {
    switch (ui::GetDisplayLayout()) {
      case ui::LAYOUT_ASH:
      case ui::LAYOUT_DESKTOP:
        value = 4;
        break;
      case ui::LAYOUT_TOUCH:
        value = 5;
        break;
      default:
        NOTREACHED();
    }
  }
  return value;
}

// Size of icon used for throbber and favicon next to tab title.
int tab_icon_size() {
  static int value = -1;
  if (value == -1) {
    switch (ui::GetDisplayLayout()) {
      case ui::LAYOUT_ASH:
      case ui::LAYOUT_DESKTOP:
        value = gfx::kFaviconSize;
        break;
      case ui::LAYOUT_TOUCH:
        value = 20;
        break;
      default:
        NOTREACHED();
    }
  }
  return value;
}

// How long the pulse throb takes.
const int kPulseDurationMs = 200;

// How long the recording button takes to fade in/out.
const int kRecordingDurationMs = 1000;

// Width of touch tabs.
static const int kTouchWidth = 120;

static const int kToolbarOverlap = 1;
static const int kFaviconTitleSpacing = 4;
#if defined(USE_ASH)
// Additional vertical offset for title text relative to top of tab.
// Ash text rendering may be different than Windows.
// TODO(jamescook): Make this Chrome OS or Linux only?
static const int kTitleTextOffsetY = 1;
#else
static const int kTitleTextOffsetY = 0;
#endif
static const int kTitleCloseButtonSpacing = 3;
static const int kStandardTitleWidth = 175;
#if defined(USE_ASH)
// Additional vertical offset for close button relative to top of tab.
// Ash needs this to match the text vertical position.
static const int kCloseButtonVertFuzz = 1;
#else
static const int kCloseButtonVertFuzz = 0;
#endif
// Additional horizontal offset for close button relative to title text.
static const int kCloseButtonHorzFuzz = 3;

// When a non-mini-tab becomes a mini-tab the width of the tab animates. If
// the width of a mini-tab is >= kMiniTabRendererAsNormalTabWidth then the tab
// is rendered as a normal tab. This is done to avoid having the title
// immediately disappear when transitioning a tab from normal to mini-tab.
static const int kMiniTabRendererAsNormalTabWidth =
    browser_defaults::kMiniTabWidth + 30;

// How opaque to make the hover state (out of 1).
static const double kHoverOpacity = 0.33;

// Opacity for non-active selected tabs.
static const double kSelectedTabOpacity = .45;

// Selected (but not active) tabs have their throb value scaled down by this.
static const double kSelectedTabThrobScale = .5;

// Durations for the various parts of the mini tab title animation.
static const int kMiniTitleChangeAnimationDuration1MS = 1600;
static const int kMiniTitleChangeAnimationStart1MS = 0;
static const int kMiniTitleChangeAnimationEnd1MS = 1900;
static const int kMiniTitleChangeAnimationDuration2MS = 0;
static const int kMiniTitleChangeAnimationDuration3MS = 550;
static const int kMiniTitleChangeAnimationStart3MS = 150;
static const int kMiniTitleChangeAnimationEnd3MS = 800;
static const int kMiniTitleChangeAnimationIntervalMS = 40;

// Offset from the right edge for the start of the mini title change animation.
static const int kMiniTitleChangeInitialXOffset = 6;

// Radius of the radial gradient used for mini title change animation.
static const int kMiniTitleChangeGradientRadius = 20;

// Colors of the gradient used during the mini title change animation.
static const SkColor kMiniTitleChangeGradientColor1 = SK_ColorWHITE;
static const SkColor kMiniTitleChangeGradientColor2 =
    SkColorSetARGB(0, 255, 255, 255);

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

// Scale to resize the current favicon by when projecting.
const double kProjectingFaviconResizeScale = 0.75;

// Scale to resize the projection sheet glow by.
const double kProjectingGlowResizeScale = 2.0;

// Draws the icon image at the center of |bounds|.
void DrawIconCenter(gfx::Canvas* canvas,
                    const gfx::ImageSkia& image,
                    int image_offset,
                    int icon_width,
                    int icon_height,
                    const gfx::Rect& bounds,
                    bool filter,
                    const SkPaint& paint) {
  // Center the image within bounds.
  int dst_x = bounds.x() - (icon_width - bounds.width()) / 2;
  int dst_y = bounds.y() - (icon_height - bounds.height()) / 2;
  // NOTE: the clipping is a work around for 69528, it shouldn't be necessary.
  canvas->Save();
  canvas->ClipRect(gfx::Rect(dst_x, dst_y, icon_width, icon_height));
  canvas->DrawImageInt(image,
                       image_offset, 0, icon_width, icon_height,
                       dst_x, dst_y, icon_width, icon_height,
                       filter, paint);
  canvas->Restore();
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// FaviconCrashAnimation
//
//  A custom animation subclass to manage the favicon crash animation.
class Tab::FaviconCrashAnimation : public ui::LinearAnimation,
                                   public ui::AnimationDelegate {
 public:
  explicit FaviconCrashAnimation(Tab* target)
      : ALLOW_THIS_IN_INITIALIZER_LIST(ui::LinearAnimation(1000, 25, this)),
        target_(target) {
  }
  virtual ~FaviconCrashAnimation() {}

  // ui::Animation overrides:
  virtual void AnimateToState(double state) {
    const double kHidingOffset = 27;

    if (state < .5) {
      target_->SetFaviconHidingOffset(
          static_cast<int>(floor(kHidingOffset * 2.0 * state)));
    } else {
      target_->DisplayCrashedFavicon();
      target_->SetFaviconHidingOffset(
          static_cast<int>(
              floor(kHidingOffset - ((state - .5) * 2.0 * kHidingOffset))));
    }
  }

  // ui::AnimationDelegate overrides:
  virtual void AnimationCanceled(const ui::Animation* animation) {
    target_->SetFaviconHidingOffset(0);
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
class Tab::TabCloseButton : public views::ImageButton {
 public:
  explicit TabCloseButton(Tab* tab) : views::ImageButton(tab), tab_(tab) {}
  virtual ~TabCloseButton() {}

  // Overridden from views::View.
  virtual View* GetEventHandlerForPoint(const gfx::Point& point) OVERRIDE {
    // Ignore the padding set on the button.
    gfx::Rect rect = GetContentsBounds();
    rect.set_x(GetMirroredXForRect(rect));

#if defined(USE_ASH)
    // Include the padding in hit-test for touch events.
    if (aura::Env::GetInstance()->is_touch_down())
      rect = GetLocalBounds();
#elif defined(OS_WIN)
    // TODO(sky): Use local-bounds if a touch-point is active.
    // http://crbug.com/145258
#endif

    return rect.Contains(point) ? this : parent();
  }

  virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE {
    if (tab_->controller())
      tab_->controller()->OnMouseEventInTab(this, event);

    bool handled = ImageButton::OnMousePressed(event);
    // Explicitly mark midle-mouse clicks as non-handled to ensure the tab
    // sees them.
    return event.IsOnlyMiddleMouseButton() ? false : handled;
  }

  virtual void OnMouseMoved(const ui::MouseEvent& event) OVERRIDE {
    if (tab_->controller())
      tab_->controller()->OnMouseEventInTab(this, event);
    CustomButton::OnMouseMoved(event);
  }

  virtual void OnMouseReleased(const ui::MouseEvent& event) OVERRIDE {
    if (tab_->controller())
      tab_->controller()->OnMouseEventInTab(this, event);
    CustomButton::OnMouseReleased(event);
  }

  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE {
    // Consume all gesture events here so that the parent (Tab) does not
    // start consuming gestures.
    ImageButton::OnGestureEvent(event);
    event->SetHandled();
  }

 private:
  Tab* tab_;

  DISALLOW_COPY_AND_ASSIGN(TabCloseButton);
};

////////////////////////////////////////////////////////////////////////////////
// ImageCacheEntry

Tab::ImageCacheEntry::ImageCacheEntry()
    : resource_id(-1),
      scale_factor(ui::SCALE_FACTOR_NONE) {
}

Tab::ImageCacheEntry::~ImageCacheEntry() {}

////////////////////////////////////////////////////////////////////////////////
// Tab, statics:

// static
const char Tab::kViewClassName[] = "BrowserTab";
// static
Tab::TabImage Tab::tab_alpha_ = {0};
Tab::TabImage Tab::tab_active_ = {0};
Tab::TabImage Tab::tab_inactive_ = {0};
// static
gfx::Font* Tab::font_ = NULL;
// static
int Tab::font_height_ = 0;
// static
Tab::ImageCache* Tab::image_cache_ = NULL;

////////////////////////////////////////////////////////////////////////////////
// Tab, public:

Tab::Tab(TabController* controller)
    : controller_(controller),
      closing_(false),
      dragging_(false),
      favicon_hiding_offset_(0),
      loading_animation_frame_(0),
      should_display_crashed_favicon_(false),
      throbber_disabled_(false),
      theme_provider_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(hover_controller_(this)),
      showing_icon_(false),
      showing_close_button_(false),
      close_button_color_(0),
      icon_dominant_color_(SK_ColorWHITE) {
  InitTabResources();

  // So we get don't get enter/exit on children and don't prematurely stop the
  // hover.
  set_notify_enter_exit_on_child(true);

  set_id(VIEW_ID_TAB);

  // Add the Close Button.
  close_button_ = new TabCloseButton(this);
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  close_button_->SetImage(views::CustomButton::STATE_NORMAL,
                          rb.GetImageSkiaNamed(IDR_TAB_CLOSE));
  close_button_->SetImage(views::CustomButton::STATE_HOVERED,
                          rb.GetImageSkiaNamed(IDR_TAB_CLOSE_H));
  close_button_->SetImage(views::CustomButton::STATE_PRESSED,
                          rb.GetImageSkiaNamed(IDR_TAB_CLOSE_P));
  close_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_CLOSE));
  // Disable animation so that the red danger sign shows up immediately
  // to help avoid mis-clicks.
  close_button_->SetAnimationDuration(0);
  AddChildView(close_button_);

  set_context_menu_controller(this);
}

Tab::~Tab() {
}

void Tab::set_animation_container(ui::AnimationContainer* container) {
  animation_container_ = container;
  hover_controller_.SetAnimationContainer(container);
}

bool Tab::IsActive() const {
  return controller() ? controller()->IsActiveTab(this) : true;
}

bool Tab::IsSelected() const {
  return controller() ? controller()->IsTabSelected(this) : true;
}

void Tab::SetData(const TabRendererData& data) {
  if (data_.Equals(data))
    return;

  TabRendererData old(data_);
  data_ = data;

  if (data_.IsCrashed()) {
    if (!should_display_crashed_favicon_ && !IsPerformingCrashAnimation()) {
#if defined(OS_CHROMEOS)
      // On Chrome OS, we reload killed tabs automatically when the user
      // switches to them.  Don't display animations for these unless they're
      // selected (i.e. in the foreground) -- we won't reload these
      // automatically since we don't want to get into a crash loop.
      if (IsSelected() ||
          data_.crashed_status != base::TERMINATION_STATUS_PROCESS_WAS_KILLED)
        StartCrashAnimation();
#else
      StartCrashAnimation();
#endif
    }

  } else if ((data_.capture_state == TabRendererData::CAPTURE_STATE_NONE) &&
             (old.capture_state != TabRendererData::CAPTURE_STATE_NONE)) {
    StopRecordingAnimation();

  } else if ((data_.capture_state != TabRendererData::CAPTURE_STATE_NONE) &&
             (old.capture_state == TabRendererData::CAPTURE_STATE_NONE)) {
    StartRecordingAnimation();
  } else {
    if (IsPerformingCrashAnimation())
      StopCrashAnimation();
    ResetCrashedFavicon();
  }

  // If the favicon changed, re-compute its dominant color.
  if (controller() &&
      controller()->IsImmersiveStyle() &&
      !data_.favicon.isNull() &&
      !data_.favicon.BackedBySameObjectAs(old.favicon))
    UpdateIconDominantColor();

  DataChanged(old);

  Layout();
  SchedulePaint();
}

void Tab::UpdateLoadingAnimation(TabRendererData::NetworkState state) {
  // If this is an extension app and a command line flag is set,
  // then disable the throbber.
  throbber_disabled_ = data().app &&
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kAppsNoThrob);

  if (throbber_disabled_)
    return;

  if (state == data_.network_state &&
      state == TabRendererData::NETWORK_STATE_NONE) {
    // If the network state is none and hasn't changed, do nothing. Otherwise we
    // need to advance the animation frame.
    return;
  }

  TabRendererData::NetworkState old_state = data_.network_state;
  data_.network_state = state;
  AdvanceLoadingAnimation(old_state, state);
}

void Tab::StartPulse() {
  if (!tab_animation_.get()) {
    tab_animation_.reset(new ui::ThrobAnimation(this));
    tab_animation_->SetSlideDuration(kPulseDurationMs);
    if (animation_container_.get())
      tab_animation_->SetContainer(animation_container_.get());
  }
  tab_animation_->Reset();
  tab_animation_->StartThrobbing(std::numeric_limits<int>::max());
}

void Tab::StopPulse() {
  if (!tab_animation_.get())
    return;

  tab_animation_->Stop();  // Do stop so we get notified.
  tab_animation_.reset(NULL);
}

void Tab::StartMiniTabTitleAnimation() {
  if (!mini_title_animation_.get()) {
    ui::MultiAnimation::Parts parts;
    parts.push_back(
        ui::MultiAnimation::Part(kMiniTitleChangeAnimationDuration1MS,
                                 ui::Tween::EASE_OUT));
    parts.push_back(
        ui::MultiAnimation::Part(kMiniTitleChangeAnimationDuration2MS,
                                 ui::Tween::ZERO));
    parts.push_back(
        ui::MultiAnimation::Part(kMiniTitleChangeAnimationDuration3MS,
                                 ui::Tween::EASE_IN));
    parts[0].start_time_ms = kMiniTitleChangeAnimationStart1MS;
    parts[0].end_time_ms = kMiniTitleChangeAnimationEnd1MS;
    parts[2].start_time_ms = kMiniTitleChangeAnimationStart3MS;
    parts[2].end_time_ms = kMiniTitleChangeAnimationEnd3MS;
    mini_title_animation_.reset(new ui::MultiAnimation(
        parts,
        base::TimeDelta::FromMilliseconds(
            kMiniTitleChangeAnimationIntervalMS)));
    mini_title_animation_->SetContainer(animation_container());
    mini_title_animation_->set_delegate(this);
  }
  mini_title_animation_->Start();
}

void Tab::StopMiniTabTitleAnimation() {
  if (mini_title_animation_.get())
    mini_title_animation_->Stop();
}

void Tab::UpdateIconDominantColor() {
  icon_dominant_color_ =
      color_utils::CalculateKMeanColorOfBitmap(*data_.favicon.bitmap());
}

// static
gfx::Size Tab::GetBasicMinimumUnselectedSize() {
  InitTabResources();

  gfx::Size minimum_size;
  minimum_size.set_width(left_padding() + right_padding());
  // Since we use image images, the real minimum height of the image is
  // defined most accurately by the height of the end cap images.
  minimum_size.set_height(tab_active_.image_l->height());
  return minimum_size;
}

gfx::Size Tab::GetMinimumUnselectedSize() {
  return GetBasicMinimumUnselectedSize();
}

// static
gfx::Size Tab::GetMinimumSelectedSize() {
  gfx::Size minimum_size = GetBasicMinimumUnselectedSize();
  minimum_size.set_width(
      left_padding() + gfx::kFaviconSize + right_padding());
  return minimum_size;
}

// static
gfx::Size Tab::GetStandardSize() {
  gfx::Size standard_size = GetBasicMinimumUnselectedSize();
  standard_size.set_width(
      standard_size.width() + kFaviconTitleSpacing + kStandardTitleWidth);
  return standard_size;
}

// static
int Tab::GetTouchWidth() {
  return kTouchWidth;
}

// static
int Tab::GetMiniWidth() {
  return browser_defaults::kMiniTabWidth;
}

// static
int Tab::GetImmersiveHeight() {
  return kImmersiveTabHeight;
}

////////////////////////////////////////////////////////////////////////////////
// Tab, AnimationDelegate overrides:

void Tab::AnimationProgressed(const ui::Animation* animation) {
  // Ignore if the pulse animation is being performed on active tab because
  // it repaints the same image. See |Tab::PaintTabBackground()|.
  if (animation == tab_animation_.get() && IsActive())
    return;
  SchedulePaint();
}

void Tab::AnimationCanceled(const ui::Animation* animation) {
  SchedulePaint();
}

void Tab::AnimationEnded(const ui::Animation* animation) {
  SchedulePaint();
}

////////////////////////////////////////////////////////////////////////////////
// Tab, views::ButtonListener overrides:

void Tab::ButtonPressed(views::Button* sender, const ui::Event& event) {
  const CloseTabSource source =
      (event.type() == ui::ET_MOUSE_RELEASED &&
       (event.flags() & ui::EF_FROM_TOUCH) == 0) ? CLOSE_TAB_FROM_MOUSE :
      CLOSE_TAB_FROM_TOUCH;
  DCHECK_EQ(close_button_, sender);
  controller()->CloseTab(this, source);
}

////////////////////////////////////////////////////////////////////////////////
// Tab, views::ContextMenuController overrides:

void Tab::ShowContextMenuForView(views::View* source,
                                     const gfx::Point& point) {
  if (controller() && !closing())
    controller()->ShowContextMenuForTab(this, point);
}

////////////////////////////////////////////////////////////////////////////////
// Tab, views::View overrides:

void Tab::OnPaint(gfx::Canvas* canvas) {
  // Don't paint if we're narrower than we can render correctly. (This should
  // only happen during animations).
  if (width() < GetMinimumUnselectedSize().width() && !data().mini)
    return;

  gfx::Rect clip;
  if (controller()) {
    if (!controller()->ShouldPaintTab(this, &clip))
      return;
    if (!clip.IsEmpty()) {
      canvas->Save();
      canvas->ClipRect(clip);
    }
  }

  if (controller() && controller()->IsImmersiveStyle())
    PaintTabImmersive(canvas);
  else
    PaintTab(canvas);

  if (!clip.IsEmpty())
    canvas->Restore();
}

void Tab::Layout() {
  gfx::Rect lb = GetContentsBounds();
  if (lb.IsEmpty())
    return;
  lb.Inset(
      left_padding(), top_padding(), right_padding(), bottom_padding());

  // The height of the content of the Tab is the largest of the favicon,
  // the title text and the close button graphic.
  int content_height = std::max(tab_icon_size(), font_height_);
  gfx::Size close_button_size(close_button_->GetPreferredSize());
  content_height = std::max(content_height, close_button_size.height());

  // Size the Favicon.
  showing_icon_ = ShouldShowIcon();
  if (showing_icon_) {
    // Use the size of the favicon as apps use a bigger favicon size.
    int favicon_top = top_padding() + content_height / 2 - tab_icon_size() / 2;
    int favicon_left = lb.x();
    favicon_bounds_.SetRect(favicon_left, favicon_top,
                            tab_icon_size(), tab_icon_size());
    if (data().mini && width() < kMiniTabRendererAsNormalTabWidth) {
      // Adjust the location of the favicon when transitioning from a normal
      // tab to a mini-tab.
      int mini_delta = kMiniTabRendererAsNormalTabWidth - GetMiniWidth();
      int ideal_delta = width() - GetMiniWidth();
      if (ideal_delta < mini_delta) {
        int ideal_x = (GetMiniWidth() - tab_icon_size()) / 2;
        int x = favicon_bounds_.x() + static_cast<int>(
            (1 - static_cast<float>(ideal_delta) /
             static_cast<float>(mini_delta)) *
            (ideal_x - favicon_bounds_.x()));
        favicon_bounds_.set_x(x);
      }
    }
  } else {
    favicon_bounds_.SetRect(lb.x(), lb.y(), 0, 0);
  }

  // Size the Close button.
  showing_close_button_ = ShouldShowCloseBox();
  if (showing_close_button_) {
    int close_button_top = top_padding() + kCloseButtonVertFuzz +
        (content_height - close_button_size.height()) / 2;
    // If the ratio of the close button size to tab width exceeds the maximum.
    // The close button should be as large as possible so that there is a larger
    // hit-target for touch events. So the close button bounds extends to the
    // edges of the tab. However, the larger hit-target should be active only
    // for mouse events, and the close-image should show up in the right place.
    // So a border is added to the button with necessary padding. The close
    // button (BaseTab::TabCloseButton) makes sure the padding is a hit-target
    // only for touch events.
    int top_border = close_button_top;
    int bottom_border = height() - (close_button_size.height() + top_border);
    int left_border = kCloseButtonHorzFuzz;
    int right_border = width() - (lb.width() + close_button_size.width() +
        left_border);
    close_button_->set_border(views::Border::CreateEmptyBorder(top_border,
        left_border, bottom_border, right_border));
    close_button_->SetBounds(lb.width(), 0,
        close_button_size.width() + left_border + right_border,
        close_button_size.height() + top_border + bottom_border);
    close_button_->SetVisible(true);
  } else {
    close_button_->SetBounds(0, 0, 0, 0);
    close_button_->SetVisible(false);
  }

  int title_left = favicon_bounds_.right() + kFaviconTitleSpacing;
  int title_top = top_padding() + kTitleTextOffsetY +
      (content_height - font_height_) / 2;
  // Size the Title text to fill the remaining space.
  if (!data().mini || width() >= kMiniTabRendererAsNormalTabWidth) {
    // If the user has big fonts, the title will appear rendered too far down
    // on the y-axis if we use the regular top padding, so we need to adjust it
    // so that the text appears centered.
    gfx::Size minimum_size = GetMinimumUnselectedSize();
    int text_height = title_top + font_height_ + bottom_padding();
    if (text_height > minimum_size.height())
      title_top -= (text_height - minimum_size.height()) / 2;

    int title_width;
    if (close_button_->visible()) {
      // The close button has an empty border with some padding (see details
      // above where the close-button's bounds is set). Allow the title to
      // overlap the empty padding.
      title_width = std::max(close_button_->x() +
                             close_button_->GetInsets().left() -
                             kTitleCloseButtonSpacing - title_left, 0);
    } else {
      title_width = std::max(lb.width() - title_left, 0);
    }
    title_bounds_.SetRect(title_left, title_top, title_width, font_height_);
  } else {
    title_bounds_.SetRect(title_left, title_top, 0, 0);
  }

  // Certain UI elements within the Tab (the favicon, etc.) are not represented
  // as child Views (which is the preferred method).  Instead, these UI elements
  // are drawn directly on the canvas from within Tab::OnPaint(). The Tab's
  // child Views (for example, the Tab's close button which is a views::Button
  // instance) are automatically mirrored by the mirroring infrastructure in
  // views. The elements Tab draws directly on the canvas need to be manually
  // mirrored if the View's layout is right-to-left.
  title_bounds_.set_x(GetMirroredXForRect(title_bounds_));
}

void Tab::OnThemeChanged() {
  LoadTabImages();
}

std::string Tab::GetClassName() const {
  return kViewClassName;
}

bool Tab::HasHitTestMask() const {
  return true;
}

void Tab::GetHitTestMask(gfx::Path* path) const {
  // When the window is maximized we don't want to shave off the edges or top
  // shadow of the tab, such that the user can click anywhere along the top
  // edge of the screen to select a tab.
  bool include_top_shadow = GetWidget() && GetWidget()->IsMaximized();
  TabResources::GetHitTestMask(width(), height(), include_top_shadow, path);
}

bool Tab::GetTooltipText(const gfx::Point& p, string16* tooltip) const {
  if (data_.title.empty())
    return false;

  // Only show the tooltip if the title is truncated.
  if (font_->GetStringWidth(data_.title) > GetTitleBounds().width()) {
    *tooltip = data_.title;
    return true;
  }
  return false;
}

bool Tab::GetTooltipTextOrigin(const gfx::Point& p, gfx::Point* origin) const {
  origin->set_x(title_bounds_.x() + 10);
  origin->set_y(-views::TooltipManager::GetTooltipHeight() - 4);
  return true;
}

ui::ThemeProvider* Tab::GetThemeProvider() const {
  ui::ThemeProvider* tp = View::GetThemeProvider();
  return tp ? tp : theme_provider_;
}

bool Tab::OnMousePressed(const ui::MouseEvent& event) {
  if (!controller())
    return false;

  controller()->OnMouseEventInTab(this, event);

  // Allow a right click from touch to drag, which corresponds to a long click.
  if (event.IsOnlyLeftMouseButton() ||
      (event.IsOnlyRightMouseButton() && event.flags() & ui::EF_FROM_TOUCH)) {
    ui::ListSelectionModel original_selection;
    original_selection.Copy(controller()->GetSelectionModel());
    // Changing the selection may cause our bounds to change. If that happens
    // the location of the event may no longer be valid. Create a copy of the
    // event in the parents coordinate, which won't change, and recreate an
    // event after changing so the coordinates are correct.
    ui::MouseEvent event_in_parent(event, static_cast<View*>(this), parent());
    if (controller()->SupportsMultipleSelection()) {
      if (event.IsShiftDown() && event.IsControlDown()) {
        controller()->AddSelectionFromAnchorTo(this);
      } else if (event.IsShiftDown()) {
        controller()->ExtendSelectionTo(this);
      } else if (event.IsControlDown()) {
        controller()->ToggleSelected(this);
        if (!IsSelected()) {
          // Don't allow dragging non-selected tabs.
          return false;
        }
      } else if (!IsSelected()) {
        controller()->SelectTab(this);
      }
    } else if (!IsSelected()) {
      controller()->SelectTab(this);
    }
    ui::MouseEvent cloned_event(event_in_parent, parent(),
                                static_cast<View*>(this));
    controller()->MaybeStartDrag(this, cloned_event, original_selection);
  }
  return true;
}

bool Tab::OnMouseDragged(const ui::MouseEvent& event) {
  if (controller())
    controller()->ContinueDrag(this, event.location());
  return true;
}

void Tab::OnMouseReleased(const ui::MouseEvent& event) {
  if (!controller())
    return;

  controller()->OnMouseEventInTab(this, event);

  // Notify the drag helper that we're done with any potential drag operations.
  // Clean up the drag helper, which is re-created on the next mouse press.
  // In some cases, ending the drag will schedule the tab for destruction; if
  // so, bail immediately, since our members are already dead and we shouldn't
  // do anything else except drop the tab where it is.
  if (controller()->EndDrag(END_DRAG_COMPLETE))
    return;

  // Close tab on middle click, but only if the button is released over the tab
  // (normal windows behavior is to discard presses of a UI element where the
  // releases happen off the element).
  if (event.IsMiddleMouseButton()) {
    if (HitTestPoint(event.location())) {
      controller()->CloseTab(this, CLOSE_TAB_FROM_MOUSE);
    } else if (closing_) {
      // We're animating closed and a middle mouse button was pushed on us but
      // we don't contain the mouse anymore. We assume the user is clicking
      // quicker than the animation and we should close the tab that falls under
      // the mouse.
      Tab* closest_tab = controller()->GetTabAt(this, event.location());
      if (closest_tab)
        controller()->CloseTab(closest_tab, CLOSE_TAB_FROM_MOUSE);
    }
  } else if (event.IsOnlyLeftMouseButton() && !event.IsShiftDown() &&
             !event.IsControlDown()) {
    // If the tab was already selected mouse pressed doesn't change the
    // selection. Reset it now to handle the case where multiple tabs were
    // selected.
    controller()->SelectTab(this);
  }
}

void Tab::OnMouseCaptureLost() {
  if (controller())
    controller()->EndDrag(END_DRAG_CAPTURE_LOST);
}

void Tab::OnMouseEntered(const ui::MouseEvent& event) {
  hover_controller_.Show();
}

void Tab::OnMouseMoved(const ui::MouseEvent& event) {
  hover_controller_.SetLocation(event.location());
  if (controller())
    controller()->OnMouseEventInTab(this, event);
}

void Tab::OnMouseExited(const ui::MouseEvent& event) {
  hover_controller_.Hide();
}

void Tab::OnGestureEvent(ui::GestureEvent* event) {
  if (!controller()) {
    event->SetHandled();
    return;
  }

  switch (event->type()) {
    case ui::ET_GESTURE_BEGIN: {
      if (event->details().touch_points() != 1)
        return;

      // See comment in OnMousePressed() as to why we copy the event.
      ui::GestureEvent event_in_parent(*event, static_cast<View*>(this),
                                       parent());
      ui::ListSelectionModel original_selection;
      original_selection.Copy(controller()->GetSelectionModel());
      if (!IsSelected())
        controller()->SelectTab(this);
      gfx::Point loc(event->location());
      views::View::ConvertPointToScreen(this, &loc);
      ui::GestureEvent cloned_event(event_in_parent, parent(),
                                    static_cast<View*>(this));
      controller()->MaybeStartDrag(this, cloned_event, original_selection);
      break;
    }

    case ui::ET_GESTURE_END:
      controller()->EndDrag(END_DRAG_COMPLETE);
      break;

    case ui::ET_GESTURE_SCROLL_UPDATE:
      controller()->ContinueDrag(this, event->location());
      break;

    default:
      break;
  }
  event->SetHandled();
}

void Tab::GetAccessibleState(ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_PAGETAB;
  state->name = data_.title;
}

////////////////////////////////////////////////////////////////////////////////
// Tab, private

const gfx::Rect& Tab::GetTitleBounds() const {
  return title_bounds_;
}

const gfx::Rect& Tab::GetIconBounds() const {
  return favicon_bounds_;
}

void Tab::DataChanged(const TabRendererData& old) {
  if (data().blocked == old.blocked)
    return;

  if (data().blocked)
    StartPulse();
  else
    StopPulse();
}

void Tab::PaintTab(gfx::Canvas* canvas) {
  // See if the model changes whether the icons should be painted.
  const bool show_icon = ShouldShowIcon();
  const bool show_close_button = ShouldShowCloseBox();
  if (show_icon != showing_icon_ || show_close_button != showing_close_button_)
    Layout();

  PaintTabBackground(canvas);

  SkColor title_color = GetThemeProvider()->
      GetColor(IsSelected() ?
          ThemeService::COLOR_TAB_TEXT :
          ThemeService::COLOR_BACKGROUND_TAB_TEXT);

  if (!data().mini || width() > kMiniTabRendererAsNormalTabWidth)
    PaintTitle(canvas, title_color);

  if (show_icon)
    PaintIcon(canvas);

  // If the close button color has changed, generate a new one.
  if (!close_button_color_ || title_color != close_button_color_) {
    close_button_color_ = title_color;
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    close_button_->SetBackground(close_button_color_,
        rb.GetImageSkiaNamed(IDR_TAB_CLOSE),
        rb.GetImageSkiaNamed(IDR_TAB_CLOSE_MASK));
  }
}

void Tab::PaintTabImmersive(gfx::Canvas* canvas) {
  // The main bar is as wide as the normal tab's horizontal top line.
  // This top line of the tab extends a few pixels left and right of the
  // center image due to pixels in the rounded corner images.
  const int kBarPadding = 1;
  int main_bar_left = tab_active_.l_width - kBarPadding;
  int main_bar_right = width() - tab_active_.r_width + kBarPadding;

  // Draw a gray rectangle to represent the tab. This works for mini-tabs as
  // well as regular ones. The active tab has a brigher bar.
  SkColor color =
      IsActive() ? kImmersiveActiveTabColor : kImmersiveInactiveTabColor;
  gfx::Rect main_bar_rect(
      main_bar_left, 0, main_bar_right - main_bar_left, kImmersiveBarHeight);
  canvas->FillRect(main_bar_rect, color);
}

void Tab::PaintTabBackground(gfx::Canvas* canvas) {
  if (IsActive()) {
    PaintActiveTabBackground(canvas);
  } else {
    if (mini_title_animation_.get() && mini_title_animation_->is_animating())
      PaintInactiveTabBackgroundWithTitleChange(canvas);
    else
      PaintInactiveTabBackground(canvas);

    double throb_value = GetThrobValue();
    if (throb_value > 0) {
      canvas->SaveLayerAlpha(static_cast<int>(throb_value * 0xff),
                             GetLocalBounds());
      PaintActiveTabBackground(canvas);
      canvas->Restore();
    }
  }
}

void Tab::PaintInactiveTabBackgroundWithTitleChange(gfx::Canvas* canvas) {
  // Render the inactive tab background. We'll use this for clipping.
  gfx::Canvas background_canvas(size(), canvas->scale_factor(), false);
  PaintInactiveTabBackground(&background_canvas);

  gfx::ImageSkia background_image(background_canvas.ExtractImageRep());

  // Draw a radial gradient to hover_canvas.
  gfx::Canvas hover_canvas(size(), canvas->scale_factor(), false);
  int radius = kMiniTitleChangeGradientRadius;
  int x0 = width() + radius - kMiniTitleChangeInitialXOffset;
  int x1 = radius;
  int x2 = -radius;
  int x;
  if (mini_title_animation_->current_part_index() == 0) {
    x = mini_title_animation_->CurrentValueBetween(x0, x1);
  } else if (mini_title_animation_->current_part_index() == 1) {
    x = x1;
  } else {
    x = mini_title_animation_->CurrentValueBetween(x1, x2);
  }
  SkPoint center_point;
  center_point.iset(x, 0);
  SkColor colors[2] = { kMiniTitleChangeGradientColor1,
                        kMiniTitleChangeGradientColor2 };
  skia::RefPtr<SkShader> shader = skia::AdoptRef(
      SkGradientShader::CreateRadial(
          center_point, SkIntToScalar(radius), colors, NULL, 2,
          SkShader::kClamp_TileMode));
  SkPaint paint;
  paint.setShader(shader.get());
  hover_canvas.DrawRect(gfx::Rect(x - radius, -radius, radius * 2, radius * 2),
                        paint);

  // Draw the radial gradient clipped to the background into hover_image.
  gfx::ImageSkia hover_image = gfx::ImageSkiaOperations::CreateMaskedImage(
      gfx::ImageSkia(hover_canvas.ExtractImageRep()), background_image);

  // Draw the tab background to the canvas.
  canvas->DrawImageInt(background_image, 0, 0);

  // And then the gradient on top of that.
  if (mini_title_animation_->current_part_index() == 2) {
    uint8 alpha = mini_title_animation_->CurrentValueBetween(255, 0);
    canvas->DrawImageInt(hover_image, 0, 0, alpha);
  } else {
    canvas->DrawImageInt(hover_image, 0, 0);
  }
}

void Tab::PaintInactiveTabBackground(gfx::Canvas* canvas) {
  int tab_id;
  if (GetWidget() && GetWidget()->GetTopLevelWidget()->ShouldUseNativeFrame()) {
    tab_id = IDR_THEME_TAB_BACKGROUND_V;
  } else if (data().incognito) {
    tab_id = IDR_THEME_TAB_BACKGROUND_INCOGNITO;
#if defined(OS_WIN)
  } else if (win8::IsSingleWindowMetroMode()) {
    tab_id = IDR_THEME_TAB_BACKGROUND_V;
#endif
  } else {
    tab_id = IDR_THEME_TAB_BACKGROUND;
  }

  const bool can_cache = !GetThemeProvider()->HasCustomImage(tab_id) &&
      !hover_controller_.ShouldDraw();

  if (can_cache) {
    gfx::ImageSkia cached_image(
        GetCachedImage(tab_id, size(), canvas->scale_factor()));
    if (cached_image.width() == 0) {
      gfx::Canvas tmp_canvas(size(), canvas->scale_factor(), false);
      PaintInactiveTabBackgroundUsingResourceId(&tmp_canvas, tab_id);
      cached_image = gfx::ImageSkia(tmp_canvas.ExtractImageRep());
      SetCachedImage(tab_id, canvas->scale_factor(), cached_image);
    }
    canvas->DrawImageInt(cached_image, 0, 0);
  } else {
    PaintInactiveTabBackgroundUsingResourceId(canvas, tab_id);
  }
}

void Tab::PaintInactiveTabBackgroundUsingResourceId(gfx::Canvas* canvas,
                                                    int tab_id) {
  // WARNING: the inactive tab background may be cached. If you change what it
  // is drawn from you may need to update whether it can be cached.

  // The tab image needs to be lined up with the background image
  // so that it feels partially transparent.  These offsets represent the tab
  // position within the frame background image.
  int offset = GetMirroredX() + background_offset_.x();

  gfx::ImageSkia* tab_bg = GetThemeProvider()->GetImageSkiaNamed(tab_id);

  TabImage* tab_image = &tab_active_;
  TabImage* tab_inactive_image = &tab_inactive_;
  TabImage* alpha = &tab_alpha_;

  // If the theme is providing a custom background image, then its top edge
  // should be at the top of the tab. Otherwise, we assume that the background
  // image is a composited foreground + frame image.
  int bg_offset_y = GetThemeProvider()->HasCustomImage(tab_id) ?
      0 : background_offset_.y();

  // We need a gfx::Canvas object to be able to extract the image from.
  // We draw everything to this canvas and then output it to the canvas
  // parameter in addition to using it to mask the hover glow if needed.
  gfx::Canvas background_canvas(size(), canvas->scale_factor(), false);

  // Draw left edge.  Don't draw over the toolbar, as we're not the foreground
  // tab.
  gfx::ImageSkia tab_l = gfx::ImageSkiaOperations::CreateTiledImage(
      *tab_bg, offset, bg_offset_y, tab_image->l_width, height());
  gfx::ImageSkia theme_l =
      gfx::ImageSkiaOperations::CreateMaskedImage(tab_l, *alpha->image_l);
  background_canvas.DrawImageInt(theme_l,
      0, 0, theme_l.width(), theme_l.height() - kToolbarOverlap,
      0, 0, theme_l.width(), theme_l.height() - kToolbarOverlap,
      false);

  // Draw right edge.  Again, don't draw over the toolbar.
  gfx::ImageSkia tab_r = gfx::ImageSkiaOperations::CreateTiledImage(*tab_bg,
      offset + width() - tab_image->r_width, bg_offset_y,
      tab_image->r_width, height());
  gfx::ImageSkia theme_r =
      gfx::ImageSkiaOperations::CreateMaskedImage(tab_r, *alpha->image_r);
  background_canvas.DrawImageInt(theme_r,
      0, 0, theme_r.width(), theme_r.height() - kToolbarOverlap,
      width() - theme_r.width(), 0, theme_r.width(),
      theme_r.height() - kToolbarOverlap, false);

  // Draw center.  Instead of masking out the top portion we simply skip over
  // it by incrementing by GetDropShadowHeight(), since it's a simple
  // rectangle. And again, don't draw over the toolbar.
  background_canvas.TileImageInt(*tab_bg,
     offset + tab_image->l_width,
     bg_offset_y + drop_shadow_height() + tab_image->y_offset,
     tab_image->l_width,
     drop_shadow_height() + tab_image->y_offset,
     width() - tab_image->l_width - tab_image->r_width,
     height() - drop_shadow_height() - kToolbarOverlap - tab_image->y_offset);

  canvas->DrawImageInt(
      gfx::ImageSkia(background_canvas.ExtractImageRep()), 0, 0);

  if (!GetThemeProvider()->HasCustomImage(tab_id) &&
      hover_controller_.ShouldDraw()) {
    hover_controller_.Draw(canvas, gfx::ImageSkia(
        background_canvas.ExtractImageRep()));
  }

  // Now draw the highlights/shadows around the tab edge.
  canvas->DrawImageInt(*tab_inactive_image->image_l, 0, 0);
  canvas->TileImageInt(*tab_inactive_image->image_c,
                       tab_inactive_image->l_width, 0,
                       width() - tab_inactive_image->l_width -
                           tab_inactive_image->r_width,
                       height());
  canvas->DrawImageInt(*tab_inactive_image->image_r,
                       width() - tab_inactive_image->r_width, 0);
}

void Tab::PaintActiveTabBackground(gfx::Canvas* canvas) {
  gfx::ImageSkia* tab_background =
      GetThemeProvider()->GetImageSkiaNamed(IDR_THEME_TOOLBAR);
  int offset = GetMirroredX() + background_offset_.x();

  TabImage* tab_image = &tab_active_;
  TabImage* alpha = &tab_alpha_;

  // Draw left edge.
  gfx::ImageSkia tab_l = gfx::ImageSkiaOperations::CreateTiledImage(
      *tab_background, offset, 0, tab_image->l_width, height());
  gfx::ImageSkia theme_l =
      gfx::ImageSkiaOperations::CreateMaskedImage(tab_l, *alpha->image_l);
  canvas->DrawImageInt(theme_l, 0, 0);

  // Draw right edge.
  gfx::ImageSkia tab_r = gfx::ImageSkiaOperations::CreateTiledImage(
      *tab_background,
      offset + width() - tab_image->r_width, 0, tab_image->r_width, height());
  gfx::ImageSkia theme_r =
      gfx::ImageSkiaOperations::CreateMaskedImage(tab_r, *alpha->image_r);
  canvas->DrawImageInt(theme_r, width() - tab_image->r_width, 0);

  // Draw center.  Instead of masking out the top portion we simply skip over it
  // by incrementing by GetDropShadowHeight(), since it's a simple rectangle.
  canvas->TileImageInt(*tab_background,
     offset + tab_image->l_width,
     drop_shadow_height() + tab_image->y_offset,
     tab_image->l_width,
     drop_shadow_height() + tab_image->y_offset,
     width() - tab_image->l_width - tab_image->r_width,
     height() - drop_shadow_height() - tab_image->y_offset);

  // Now draw the highlights/shadows around the tab edge.
  canvas->DrawImageInt(*tab_image->image_l, 0, 0);
  canvas->TileImageInt(*tab_image->image_c, tab_image->l_width, 0,
      width() - tab_image->l_width - tab_image->r_width, height());
  canvas->DrawImageInt(*tab_image->image_r, width() - tab_image->r_width, 0);
}

void Tab::PaintIcon(gfx::Canvas* canvas) {
  gfx::Rect bounds = GetIconBounds();
  if (bounds.IsEmpty())
    return;

  bounds.set_x(GetMirroredXForRect(bounds));

  // Paint network activity (aka throbber) animation frame.
  if (data().network_state != TabRendererData::NETWORK_STATE_NONE) {
    ui::ThemeProvider* tp = GetThemeProvider();
    gfx::ImageSkia frames(*tp->GetImageSkiaNamed(
        (data().network_state == TabRendererData::NETWORK_STATE_WAITING) ?
        IDR_THROBBER_WAITING : IDR_THROBBER));

    int icon_size = frames.height();
    int image_offset = loading_animation_frame_ * icon_size;
    DrawIconCenter(canvas, frames, image_offset,
                   icon_size, icon_size, bounds, false, SkPaint());
    return;
  }

  // Paint regular icon and potentially overlays.
  canvas->Save();
  canvas->ClipRect(GetLocalBounds());
  if (should_display_crashed_favicon_) {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    gfx::ImageSkia crashed_favicon(*rb.GetImageSkiaNamed(IDR_SAD_FAVICON));
    bounds.set_y(bounds.y() + favicon_hiding_offset_);
    DrawIconCenter(canvas, crashed_favicon, 0,
                    crashed_favicon.width(),
                    crashed_favicon.height(), bounds, true, SkPaint());
  } else {
    if (!data().favicon.isNull()) {
      if (data().capture_state == TabRendererData::CAPTURE_STATE_PROJECTING) {
        // If projecting, shrink favicon and add projection screen instead.
        gfx::ImageSkia resized_icon =
            gfx::ImageSkiaOperations::CreateResizedImage(
                data().favicon,
                skia::ImageOperations::RESIZE_BEST,
                gfx::Size(data().favicon.width() *
                          kProjectingFaviconResizeScale,
                          data().favicon.height() *
                          kProjectingFaviconResizeScale));

        gfx::Rect resized_bounds(bounds);
        // Need to shift it up a bit vertically because the projection screen
        // is thinner on the top and bottom.
        resized_bounds.set_y(resized_bounds.y() - 1);

        DrawIconCenter(canvas, resized_icon, 0,
                        resized_icon.width(),
                        resized_icon.height(),
                        resized_bounds, true, SkPaint());

        ui::ThemeProvider* tp = GetThemeProvider();
        gfx::ImageSkia projection_screen(
            *tp->GetImageSkiaNamed(IDR_TAB_CAPTURE));

        DrawIconCenter(canvas, projection_screen, 0,
                        data().favicon.width(),
                        data().favicon.height(),
                        bounds, true, SkPaint());
      } else {
        // TODO(pkasting): Use code in tab_icon_view.cc:PaintIcon() (or switch
        // to using that class to render the favicon).
        DrawIconCenter(canvas, data().favicon, 0,
                        data().favicon.width(),
                        data().favicon.height(),
                        bounds, true, SkPaint());
      }
    }
  }
  canvas->Restore();

  // Paint recording or projecting animation overlay.
  if (data().capture_state != TabRendererData::CAPTURE_STATE_NONE) {
    SkPaint paint;
    paint.setAntiAlias(true);
    U8CPU alpha = icon_animation_->GetCurrentValue() * 0xff;
    paint.setAlpha(alpha);
    ui::ThemeProvider* tp = GetThemeProvider();

    if (data().capture_state == TabRendererData::CAPTURE_STATE_PROJECTING) {
      // If projecting, add projection glow animation.
      gfx::Rect glow_bounds(bounds);
      glow_bounds.set_x(glow_bounds.x() - (32 - 24));
      glow_bounds.set_y(0);
      glow_bounds.set_width(glow_bounds.width() *
                            kProjectingGlowResizeScale);
      glow_bounds.set_height(glow_bounds.height() *
                              kProjectingGlowResizeScale);

      gfx::ImageSkia projection_glow(
          *tp->GetImageSkiaNamed(IDR_TAB_CAPTURE_GLOW));
      DrawIconCenter(canvas, projection_glow, 0,
                      projection_glow.width(), projection_glow.height(),
                      glow_bounds, false, paint);
    } else if (data().capture_state ==
                TabRendererData::CAPTURE_STATE_RECORDING) {
      // If recording, fade the recording icon on top of the favicon.
      gfx::ImageSkia recording_dot(*tp->GetImageSkiaNamed(IDR_TAB_RECORDING));
      DrawIconCenter(canvas, recording_dot, 0,
                      recording_dot.width(), recording_dot.height(),
                      bounds, false, paint);
    } else {
      NOTREACHED();
    }
  }
}

void Tab::PaintTitle(gfx::Canvas* canvas, SkColor title_color) {
  // Paint the Title.
  const gfx::Rect& title_bounds = GetTitleBounds();
  string16 title = data().title;

  if (title.empty()) {
    title = data().loading ?
        l10n_util::GetStringUTF16(IDS_TAB_LOADING_TITLE) :
        CoreTabHelper::GetDefaultTitle();
  } else {
    Browser::FormatTitleForDisplay(&title);
  }

  canvas->DrawFadeTruncatingString(title, gfx::Canvas::TruncateFadeTail, 0,
                                   *font_, title_color, title_bounds);
}

void Tab::AdvanceLoadingAnimation(TabRendererData::NetworkState old_state,
                                      TabRendererData::NetworkState state) {
  static bool initialized = false;
  static int loading_animation_frame_count = 0;
  static int waiting_animation_frame_count = 0;
  static int waiting_to_loading_frame_count_ratio = 0;
  if (!initialized) {
    initialized = true;
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    gfx::ImageSkia loading_animation(*rb.GetImageSkiaNamed(IDR_THROBBER));
    loading_animation_frame_count =
        loading_animation.width() / loading_animation.height();
    gfx::ImageSkia waiting_animation(*rb.GetImageSkiaNamed(
        IDR_THROBBER_WAITING));
    waiting_animation_frame_count =
        waiting_animation.width() / waiting_animation.height();
    waiting_to_loading_frame_count_ratio =
        waiting_animation_frame_count / loading_animation_frame_count;

    base::debug::Alias(&loading_animation_frame_count);
    base::debug::Alias(&waiting_animation_frame_count);
    CHECK_NE(0, waiting_to_loading_frame_count_ratio) <<
        "Number of frames in IDR_THROBBER must be equal to or greater " <<
        "than the number of frames in IDR_THROBBER_WAITING. Please " <<
        "investigate how this happened and update http://crbug.com/132590, " <<
        "this is causing crashes in the wild.";
  }

  // The waiting animation is the reverse of the loading animation, but at a
  // different rate - the following reverses and scales the animation_frame_
  // so that the frame is at an equivalent position when going from one
  // animation to the other.
  if (state != old_state) {
    loading_animation_frame_ = loading_animation_frame_count -
        (loading_animation_frame_ / waiting_to_loading_frame_count_ratio);
  }

  if (state != TabRendererData::NETWORK_STATE_NONE) {
    loading_animation_frame_ = (loading_animation_frame_ + 1) %
        ((state == TabRendererData::NETWORK_STATE_WAITING) ?
            waiting_animation_frame_count : loading_animation_frame_count);
  } else {
    loading_animation_frame_ = 0;
  }
  ScheduleIconPaint();
}

int Tab::IconCapacity() const {
  if (height() < GetMinimumUnselectedSize().height())
    return 0;
  return (width() - left_padding() - right_padding()) / tab_icon_size();
}

bool Tab::ShouldShowIcon() const {
  if (data().mini && height() >= GetMinimumUnselectedSize().height())
    return true;
  if (!data().show_icon) {
    return false;
  } else if (IsActive()) {
    // The active tab clips favicon before close button.
    return IconCapacity() >= 2;
  }
  // Non-selected tabs clip close button before favicon.
  return IconCapacity() >= 1;
}

bool Tab::ShouldShowCloseBox() const {
  // The active tab never clips close button.
  return !data().mini && (IsActive() || IconCapacity() >= 3);
}

double Tab::GetThrobValue() {
  bool is_selected = IsSelected();
  double min = is_selected ? kSelectedTabOpacity : 0;
  double scale = is_selected ? kSelectedTabThrobScale : 1;

  if (tab_animation_.get() && tab_animation_->is_animating())
    return tab_animation_->GetCurrentValue() * kHoverOpacity * scale + min;

  if (hover_controller_.ShouldDraw()) {
    return kHoverOpacity * hover_controller_.GetAnimationValue() * scale +
        min;
  }

  return is_selected ? kSelectedTabOpacity : 0;
}

void Tab::SetFaviconHidingOffset(int offset) {
  favicon_hiding_offset_ = offset;
  ScheduleIconPaint();
}

void Tab::DisplayCrashedFavicon() {
  should_display_crashed_favicon_ = true;
}

void Tab::ResetCrashedFavicon() {
  should_display_crashed_favicon_ = false;
}

void Tab::StartCrashAnimation() {
  icon_animation_.reset(new FaviconCrashAnimation(this));
  icon_animation_->Start();
}

void Tab::StopCrashAnimation() {
  if (!icon_animation_.get())
    return;
  icon_animation_.reset();
}

void Tab::StartRecordingAnimation() {
  ui::ThrobAnimation* animation = new ui::ThrobAnimation(this);
  animation->SetTweenType(ui::Tween::EASE_IN_OUT);
  animation->SetThrobDuration(kRecordingDurationMs);
  animation->StartThrobbing(-1);
  icon_animation_.reset(animation);
}

void Tab::StopRecordingAnimation() {
  if (!icon_animation_.get())
    return;
  icon_animation_->Stop();
  icon_animation_.reset();
}

bool Tab::IsPerformingCrashAnimation() const {
  return icon_animation_.get() && data_.IsCrashed();
}

void Tab::ScheduleIconPaint() {
  gfx::Rect bounds = GetIconBounds();
  if (bounds.IsEmpty())
    return;

  // Extends the area to the bottom when sad_favicon is
  // animating.
  if (IsPerformingCrashAnimation())
    bounds.set_height(height() - bounds.y());
  bounds.set_x(GetMirroredXForRect(bounds));
  SchedulePaintInRect(bounds);
}

////////////////////////////////////////////////////////////////////////////////
// Tab, private static:

// static
void Tab::InitTabResources() {
  static bool initialized = false;
  if (initialized)
    return;

  initialized = true;

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  font_ = new gfx::Font(rb.GetFont(ui::ResourceBundle::BaseFont));
  font_height_ = font_->GetHeight();

  image_cache_ = new ImageCache();

  // Load the tab images once now, and maybe again later if the theme changes.
  LoadTabImages();
}

// static
void Tab::LoadTabImages() {
  // We're not letting people override tab images just yet.
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

  tab_alpha_.image_l = rb.GetImageSkiaNamed(IDR_TAB_ALPHA_LEFT);
  tab_alpha_.image_r = rb.GetImageSkiaNamed(IDR_TAB_ALPHA_RIGHT);

  tab_active_.image_l = rb.GetImageSkiaNamed(IDR_TAB_ACTIVE_LEFT);
  tab_active_.image_c = rb.GetImageSkiaNamed(IDR_TAB_ACTIVE_CENTER);
  tab_active_.image_r = rb.GetImageSkiaNamed(IDR_TAB_ACTIVE_RIGHT);
  tab_active_.l_width = tab_active_.image_l->width();
  tab_active_.r_width = tab_active_.image_r->width();

  tab_inactive_.image_l = rb.GetImageSkiaNamed(IDR_TAB_INACTIVE_LEFT);
  tab_inactive_.image_c = rb.GetImageSkiaNamed(IDR_TAB_INACTIVE_CENTER);
  tab_inactive_.image_r = rb.GetImageSkiaNamed(IDR_TAB_INACTIVE_RIGHT);
  tab_inactive_.l_width = tab_inactive_.image_l->width();
  tab_inactive_.r_width = tab_inactive_.image_r->width();
}

// static
gfx::ImageSkia Tab::GetCachedImage(int resource_id,
                                   const gfx::Size& size,
                                   ui::ScaleFactor scale_factor) {
  for (ImageCache::const_iterator i = image_cache_->begin();
       i != image_cache_->end(); ++i) {
    if (i->resource_id == resource_id && i->scale_factor == scale_factor &&
        i->image.size() == size) {
      return i->image;
    }
  }
  return gfx::ImageSkia();
}

// static
void Tab::SetCachedImage(int resource_id,
                         ui::ScaleFactor scale_factor,
                         const gfx::ImageSkia& image) {
  DCHECK_NE(scale_factor, ui::SCALE_FACTOR_NONE);
  ImageCacheEntry entry;
  entry.resource_id = resource_id;
  entry.scale_factor = scale_factor;
  entry.image = image;
  image_cache_->push_front(entry);
  if (image_cache_->size() > kMaxImageCacheSize)
    image_cache_->pop_back();
}
