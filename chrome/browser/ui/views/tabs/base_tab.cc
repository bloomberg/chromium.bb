// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/base_tab.h"

#include <limits>

#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/tabs/tab_controller.h"
#include "chrome/common/chrome_switches.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/animation/animation_container.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/base/animation/throb_animation.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/text/text_elider.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/font.h"
#include "views/controls/button/image_button.h"

// How long the pulse throb takes.
static const int kPulseDurationMs = 200;

// How long the hover state takes.
static const int kHoverDurationMs = 400;

namespace {

////////////////////////////////////////////////////////////////////////////////
// TabCloseButton
//
//  This is a Button subclass that causes middle clicks to be forwarded to the
//  parent View by explicitly not handling them in OnMousePressed.
class TabCloseButton : public views::ImageButton {
 public:
  explicit TabCloseButton(views::ButtonListener* listener)
      : views::ImageButton(listener) {
  }
  virtual ~TabCloseButton() {}

  virtual bool OnMousePressed(const views::MouseEvent& event) OVERRIDE {
    bool handled = ImageButton::OnMousePressed(event);
    // Explicitly mark midle-mouse clicks as non-handled to ensure the tab
    // sees them.
    return event.IsOnlyMiddleMouseButton() ? false : handled;
  }

  // We need to let the parent know about mouse state so that it
  // can highlight itself appropriately. Note that Exit events
  // fire before Enter events, so this works.
  virtual void OnMouseEntered(const views::MouseEvent& event) OVERRIDE {
    CustomButton::OnMouseEntered(event);
    parent()->OnMouseEntered(event);
  }

  virtual void OnMouseExited(const views::MouseEvent& event) OVERRIDE {
    CustomButton::OnMouseExited(event);
    parent()->OnMouseExited(event);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TabCloseButton);
};

// Draws the icon image at the center of |bounds|.
void DrawIconCenter(gfx::Canvas* canvas,
                    const SkBitmap& image,
                    int image_offset,
                    int icon_width,
                    int icon_height,
                    const gfx::Rect& bounds,
                    bool filter) {
  // Center the image within bounds.
  int dst_x = bounds.x() - (icon_width - bounds.width()) / 2;
  int dst_y = bounds.y() - (icon_height - bounds.height()) / 2;
  // NOTE: the clipping is a work around for 69528, it shouldn't be necessary.
  canvas->Save();
  canvas->ClipRectInt(dst_x, dst_y, icon_width, icon_height);
  canvas->DrawBitmapInt(image,
                        image_offset, 0, icon_width, icon_height,
                        dst_x, dst_y, icon_width, icon_height,
                        filter);
  canvas->Restore();
}

}  // namespace

// static
gfx::Font* BaseTab::font_ = NULL;
// static
int BaseTab::font_height_ = 0;

////////////////////////////////////////////////////////////////////////////////
// FaviconCrashAnimation
//
//  A custom animation subclass to manage the favicon crash animation.
class BaseTab::FaviconCrashAnimation : public ui::LinearAnimation,
                                       public ui::AnimationDelegate {
 public:
  explicit FaviconCrashAnimation(BaseTab* target)
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
  BaseTab* target_;

  DISALLOW_COPY_AND_ASSIGN(FaviconCrashAnimation);
};

BaseTab::BaseTab(TabController* controller)
    : controller_(controller),
      closing_(false),
      dragging_(false),
      favicon_hiding_offset_(0),
      loading_animation_frame_(0),
      should_display_crashed_favicon_(false),
      throbber_disabled_(false),
      theme_provider_(NULL) {
  BaseTab::InitResources();

  SetID(VIEW_ID_TAB);

  // Add the Close Button.
  close_button_ = new TabCloseButton(this);
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  close_button_->SetImage(views::CustomButton::BS_NORMAL,
                          rb.GetBitmapNamed(IDR_TAB_CLOSE));
  close_button_->SetImage(views::CustomButton::BS_HOT,
                          rb.GetBitmapNamed(IDR_TAB_CLOSE_H));
  close_button_->SetImage(views::CustomButton::BS_PUSHED,
                          rb.GetBitmapNamed(IDR_TAB_CLOSE_P));
  close_button_->SetTooltipText(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_TOOLTIP_CLOSE_TAB)));
  close_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_CLOSE));
  // Disable animation so that the red danger sign shows up immediately
  // to help avoid mis-clicks.
  close_button_->SetAnimationDuration(0);
  AddChildView(close_button_);

  SetContextMenuController(this);
}

BaseTab::~BaseTab() {
}

void BaseTab::SetData(const TabRendererData& data) {
  if (data_.Equals(data))
    return;

  TabRendererData old(data_);
  data_ = data;

  if (data_.IsCrashed()) {
    if (!should_display_crashed_favicon_ && !IsPerformingCrashAnimation()) {
      // When --reload-killed-tabs is specified, then the idea is that
      // when tab is killed, the tab has no visual indication that it
      // died and should reload when the tab is next focused without
      // the user seeing the killed tab page.
      //
      // The only exception to this is when the tab is in the
      // foreground (i.e. when it's the selected tab), because we
      // don't want to go into an infinite loop reloading a page that
      // will constantly get killed, or if it's the only tab.  So this
      // code makes it so that the favicon will only be shown for
      // killed tabs when the tab is currently selected.
      if (CommandLine::ForCurrentProcess()->
          HasSwitch(switches::kReloadKilledTabs) && !IsSelected()) {
        // If we're reloading killed tabs, we don't want to display
        // the crashed animation at all if the process was killed and
        // the tab wasn't the current tab.
        if (data_.crashed_status != base::TERMINATION_STATUS_PROCESS_WAS_KILLED)
          StartCrashAnimation();
      } else {
        StartCrashAnimation();
      }
    }
  } else {
    if (IsPerformingCrashAnimation())
      StopCrashAnimation();
    ResetCrashedFavicon();
  }

  DataChanged(old);

  Layout();
  SchedulePaint();
}

void BaseTab::UpdateLoadingAnimation(TabRendererData::NetworkState state) {
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

void BaseTab::StartPulse() {
  if (!pulse_animation_.get()) {
    pulse_animation_.reset(new ui::ThrobAnimation(this));
    pulse_animation_->SetSlideDuration(kPulseDurationMs);
    if (animation_container_.get())
      pulse_animation_->SetContainer(animation_container_.get());
  }
  pulse_animation_->Reset();
  pulse_animation_->StartThrobbing(std::numeric_limits<int>::max());
}

void BaseTab::StopPulse() {
  if (!pulse_animation_.get())
    return;

  pulse_animation_->Stop();  // Do stop so we get notified.
  pulse_animation_.reset(NULL);
}

void BaseTab::set_animation_container(ui::AnimationContainer* container) {
  animation_container_ = container;
}

bool BaseTab::IsCloseable() const {
  return controller() ? controller()->IsTabCloseable(this) : true;
}

bool BaseTab::IsSelected() const {
  return controller() ? controller()->IsTabSelected(this) : true;
}

ui::ThemeProvider* BaseTab::GetThemeProvider() const {
  ui::ThemeProvider* tp = View::GetThemeProvider();
  return tp ? tp : theme_provider_;
}

bool BaseTab::OnMousePressed(const views::MouseEvent& event) {
  if (!controller())
    return false;

  if (event.IsOnlyLeftMouseButton()) {
    if (CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kEnableMultiTabSelection)) {
      if (event.IsShiftDown()) {
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
      controller()->MaybeStartDrag(this, event);
    } else {
      controller()->SelectTab(this);
      controller()->MaybeStartDrag(this, event);
    }
  }
  return true;
}

bool BaseTab::OnMouseDragged(const views::MouseEvent& event) {
  if (controller())
    controller()->ContinueDrag(event);
  return true;
}

void BaseTab::OnMouseReleased(const views::MouseEvent& event, bool canceled) {
  if (!controller())
    return;

  // Notify the drag helper that we're done with any potential drag operations.
  // Clean up the drag helper, which is re-created on the next mouse press.
  // In some cases, ending the drag will schedule the tab for destruction; if
  // so, bail immediately, since our members are already dead and we shouldn't
  // do anything else except drop the tab where it is.
  if (controller()->EndDrag(canceled))
    return;

  // Close tab on middle click, but only if the button is released over the tab
  // (normal windows behavior is to discard presses of a UI element where the
  // releases happen off the element).
  if (event.IsMiddleMouseButton()) {
    if (HitTest(event.location())) {
      controller()->CloseTab(this);
    } else if (closing_) {
      // We're animating closed and a middle mouse button was pushed on us but
      // we don't contain the mouse anymore. We assume the user is clicking
      // quicker than the animation and we should close the tab that falls under
      // the mouse.
      BaseTab* closest_tab = controller()->GetTabAt(this, event.location());
      if (closest_tab)
        controller()->CloseTab(closest_tab);
    }
  } else if (CommandLine::ForCurrentProcess()->HasSwitch(
                 switches::kEnableMultiTabSelection) &&
             event.IsOnlyLeftMouseButton() && !event.IsShiftDown() &&
             !event.IsControlDown()) {
    // If the tab was already selected mouse pressed doesn't change the
    // selection. Reset it now.
    controller()->SelectTab(this);
  }
}

void BaseTab::OnMouseEntered(const views::MouseEvent& event) {
  if (!hover_animation_.get()) {
    hover_animation_.reset(new ui::SlideAnimation(this));
    hover_animation_->SetContainer(animation_container_.get());
    hover_animation_->SetSlideDuration(kHoverDurationMs);
  }
  hover_animation_->SetTweenType(ui::Tween::EASE_OUT);
  hover_animation_->Show();
}

void BaseTab::OnMouseExited(const views::MouseEvent& event) {
  hover_animation_->SetTweenType(ui::Tween::EASE_IN);
  hover_animation_->Hide();
}

bool BaseTab::GetTooltipText(const gfx::Point& p, std::wstring* tooltip) {
  if (data_.title.empty())
    return false;

  // Only show the tooltip if the title is truncated.
  if (font_->GetStringWidth(data_.title) > GetTitleBounds().width()) {
    *tooltip = UTF16ToWide(data_.title);
    return true;
  }
  return false;
}

void BaseTab::GetAccessibleState(ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_PAGETAB;
  state->name = data_.title;
}

void BaseTab::AdvanceLoadingAnimation(TabRendererData::NetworkState old_state,
                                      TabRendererData::NetworkState state) {
  static bool initialized = false;
  static int loading_animation_frame_count = 0;
  static int waiting_animation_frame_count = 0;
  static int waiting_to_loading_frame_count_ratio = 0;
  if (!initialized) {
    initialized = true;
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    SkBitmap loading_animation(*rb.GetBitmapNamed(IDR_THROBBER));
    loading_animation_frame_count =
        loading_animation.width() / loading_animation.height();
    SkBitmap waiting_animation(*rb.GetBitmapNamed(IDR_THROBBER_WAITING));
    waiting_animation_frame_count =
        waiting_animation.width() / waiting_animation.height();
    waiting_to_loading_frame_count_ratio =
        waiting_animation_frame_count / loading_animation_frame_count;
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

void BaseTab::PaintIcon(gfx::Canvas* canvas) {
  gfx::Rect bounds = GetIconBounds();
  if (bounds.IsEmpty())
    return;

  // The size of bounds has to be kFaviconSize x kFaviconSize.
  DCHECK_EQ(kFaviconSize, bounds.width());
  DCHECK_EQ(kFaviconSize, bounds.height());

  bounds.set_x(GetMirroredXForRect(bounds));

  if (data().network_state != TabRendererData::NETWORK_STATE_NONE) {
    ui::ThemeProvider* tp = GetThemeProvider();
    SkBitmap frames(*tp->GetBitmapNamed(
        (data().network_state == TabRendererData::NETWORK_STATE_WAITING) ?
        IDR_THROBBER_WAITING : IDR_THROBBER));

    int icon_size = frames.height();
    int image_offset = loading_animation_frame_ * icon_size;
    DrawIconCenter(canvas, frames, image_offset,
                   icon_size, icon_size, bounds, false);
  } else {
    canvas->Save();
    canvas->ClipRectInt(0, 0, width(), height());
    if (should_display_crashed_favicon_) {
      ResourceBundle& rb = ResourceBundle::GetSharedInstance();
      SkBitmap crashed_favicon(*rb.GetBitmapNamed(IDR_SAD_FAVICON));
      bounds.set_y(bounds.y() + favicon_hiding_offset_);
      DrawIconCenter(canvas, crashed_favicon, 0,
                     crashed_favicon.width(),
                     crashed_favicon.height(), bounds, true);
    } else {
      if (!data().favicon.isNull()) {
        // TODO(pkasting): Use code in tab_icon_view.cc:PaintIcon() (or switch
        // to using that class to render the favicon).
        DrawIconCenter(canvas, data().favicon, 0,
                       data().favicon.width(),
                       data().favicon.height(),
                       bounds, true);
      }
    }
    canvas->Restore();
  }
}

void BaseTab::PaintTitle(gfx::Canvas* canvas, SkColor title_color) {
  // Paint the Title.
  const gfx::Rect& title_bounds = GetTitleBounds();
  string16 title = data().title;
  if (title.empty()) {
    title = data().loading ?
        l10n_util::GetStringUTF16(IDS_TAB_LOADING_TITLE) :
        TabContentsWrapper::GetDefaultTitle();
  } else {
    Browser::FormatTitleForDisplay(&title);
    // If we'll need to truncate, check if we should also truncate
    // a common prefix, but only if there is enough room for it.
    // We arbitrarily choose to request enough room for 10 average chars.
    if (data().common_prefix_length > 0 &&
        font_->GetExpectedTextWidth(10) < title_bounds.width() &&
        font_->GetStringWidth(title) > title_bounds.width()) {
      title.replace(0, data().common_prefix_length, UTF8ToUTF16(ui::kEllipsis));
    }
  }
  canvas->DrawStringInt(title, *font_, title_color,
                        title_bounds.x(), title_bounds.y(),
                        title_bounds.width(), title_bounds.height());
}

void BaseTab::AnimationProgressed(const ui::Animation* animation) {
  SchedulePaint();
}

void BaseTab::AnimationCanceled(const ui::Animation* animation) {
  SchedulePaint();
}

void BaseTab::AnimationEnded(const ui::Animation* animation) {
  SchedulePaint();
}

void BaseTab::ButtonPressed(views::Button* sender, const views::Event& event) {
  DCHECK(sender == close_button_);
  controller()->CloseTab(this);
}

void BaseTab::ShowContextMenuForView(views::View* source,
                                     const gfx::Point& p,
                                     bool is_mouse_gesture) {
  if (controller())
    controller()->ShowContextMenuForTab(this, p);
}

int BaseTab::loading_animation_frame() const {
  return loading_animation_frame_;
}

bool BaseTab::should_display_crashed_favicon() const {
  return should_display_crashed_favicon_;
}

int BaseTab::favicon_hiding_offset() const {
  return favicon_hiding_offset_;
}

void BaseTab::SetFaviconHidingOffset(int offset) {
  favicon_hiding_offset_ = offset;
  ScheduleIconPaint();
}

void BaseTab::DisplayCrashedFavicon() {
  should_display_crashed_favicon_ = true;
}

void BaseTab::ResetCrashedFavicon() {
  should_display_crashed_favicon_ = false;
}

void BaseTab::StartCrashAnimation() {
  if (!crash_animation_.get())
    crash_animation_.reset(new FaviconCrashAnimation(this));
  crash_animation_->Stop();
  crash_animation_->Start();
}

void BaseTab::StopCrashAnimation() {
  if (!crash_animation_.get())
    return;
  crash_animation_->Stop();
}

bool BaseTab::IsPerformingCrashAnimation() const {
  return crash_animation_.get() && crash_animation_->is_animating();
}

void BaseTab::ScheduleIconPaint() {
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

// static
void BaseTab::InitResources() {
  static bool initialized = false;
  if (!initialized) {
    initialized = true;
    font_ = new gfx::Font(
        ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::BaseFont));
    font_height_ = font_->GetHeight();
  }
}
