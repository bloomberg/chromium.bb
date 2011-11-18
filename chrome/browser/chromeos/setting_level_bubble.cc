// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/setting_level_bubble.h"

#include <algorithm>

#include "chrome/browser/chromeos/login/background_view.h"
#include "chrome/browser/chromeos/login/base_login_display_host.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/login_display_host.h"
#include "chrome/browser/chromeos/login/webui_login_display.h"
#include "chrome/browser/chromeos/setting_level_bubble_view.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "ui/gfx/screen.h"
#include "views/bubble/bubble_delegate.h"
#include "views/layout/fill_layout.h"
#include "views/widget/root_view.h"

#if !defined(USE_AURA)
#include "chrome/browser/chromeos/legacy_window_manager/wm_ipc.h"
#include "third_party/cros_system_api/window_manager/chromeos_wm_ipc_enums.h"
#endif

using base::TimeDelta;
using base::TimeTicks;
using std::max;
using std::min;

namespace {

// How long should the bubble be shown onscreen whenever the setting changes?
const int kBubbleShowTimeoutMs = 1000;

// How long should the level initially take to move up or down when it changes?
// (The rate adapts to handle keyboard autorepeat.)
const int64 kInitialAnimationDurationMs = 200;

// Horizontal position of the center of the bubble on the screen: 0 is left
// edge, 0.5 is center, 1 is right edge.
const double kBubbleXRatio = 0.5;

// Vertical gap from the bottom of the screen in pixels.
const int kBubbleBottomGap = 30;

// Duration between animation frames.
// Chosen to match ui::SlideAnimation's kDefaultFramerateHz.
const int kAnimationIntervalMs = 1000 / 50;

double LimitPercent(double percent) {
  return min(max(percent, 0.0), 100.0);
}

}  // namespace

namespace chromeos {

// SettingLevelBubbleDelegateView ----------------------------------------------
class SettingLevelBubbleDelegateView : public views::BubbleDelegateView {
 public:
  // BubbleDelegate overrides:
  virtual gfx::Point GetAnchorPoint() OVERRIDE;

  // Create the bubble delegate view.
  SettingLevelBubbleDelegateView();
  virtual ~SettingLevelBubbleDelegateView();

  SettingLevelBubbleView* view() { return view_; }

 protected:
  // BubbleDelegate overrides:
  virtual void Init() OVERRIDE;

 private:
  SettingLevelBubbleView* view_;

  DISALLOW_COPY_AND_ASSIGN(SettingLevelBubbleDelegateView);
};

gfx::Point SettingLevelBubbleDelegateView::GetAnchorPoint() {
  gfx::Size view_size = GetPreferredSize();
  // Calculate the position in screen coordinates that the bubble should
  // "point" at (since we use BubbleBorder::FLOAT, this position actually
  // specifies the center of the bubble).
  gfx::Rect monitor_area = gfx::Screen::GetMonitorAreaNearestWindow(NULL);
  return (gfx::Point(
      monitor_area.x() + kBubbleXRatio * monitor_area.width(),
      monitor_area.bottom() - view_size.height() / 2 - kBubbleBottomGap));
}

SettingLevelBubbleDelegateView::SettingLevelBubbleDelegateView()
    : BubbleDelegateView(NULL, views::BubbleBorder::FLOAT, SK_ColorWHITE),
      view_(NULL) {
  set_close_on_esc(false);
  set_use_focusless(true);
}

SettingLevelBubbleDelegateView::~SettingLevelBubbleDelegateView() {
  view_ = NULL;
}

void SettingLevelBubbleDelegateView::Init() {
  SetLayoutManager(new views::FillLayout());
  view_ = new SettingLevelBubbleView();
  AddChildView(view_);
}

// SettingLevelBubble ----------------------------------------------------------
void SettingLevelBubble::ShowBubble(double percent, bool enabled) {
  hide_timer_.Stop();

  // Set up target percent and icon.
  const double old_target_percent = target_percent_;
  UpdateTargetPercent(percent);
  SkBitmap* current_icon = increase_icon_;
  if (!enabled || target_percent_ == 0)
    current_icon = disabled_icon_;
  else if (old_target_percent >= 0 && target_percent_ < old_target_percent)
    current_icon = decrease_icon_;

  if (!view_) {
    view_ = CreateView();
    view_->Init(current_icon, percent, enabled);
  } else {
    // Reset fade sequence, if the bubble is already fading.
    SettingLevelBubbleDelegateView* delegate =
        static_cast<SettingLevelBubbleDelegateView*>
        (view_->GetWidget()->widget_delegate());
    delegate->ResetFade();
    view_->SetIcon(current_icon);
    view_->SetEnabled(enabled);
  }
  view_->GetWidget()->Show();
  // When the timer runs out, start the fade sequence.
  hide_timer_.Start(FROM_HERE,
                    base::TimeDelta::FromMilliseconds(kBubbleShowTimeoutMs),
                    this, &SettingLevelBubble::OnHideTimeout);
}

void SettingLevelBubble::HideBubble() {
  hide_timer_.Stop();
  if (view_) {
    view_->GetWidget()->Close();
    view_ = NULL;
  }
}

void SettingLevelBubble::UpdateWithoutShowingBubble(double percent,
                                                    bool enabled) {
  UpdateTargetPercent(percent);
  if (view_)
    view_->SetEnabled(enabled);
}

SettingLevelBubble::SettingLevelBubble(SkBitmap* increase_icon,
                                       SkBitmap* decrease_icon,
                                       SkBitmap* disabled_icon)
    :  current_percent_(-1.0),
       target_percent_(-1.0),
       increase_icon_(increase_icon),
       decrease_icon_(decrease_icon),
       disabled_icon_(disabled_icon),
       view_(NULL),
       is_animating_(false) {
}

SettingLevelBubble::~SettingLevelBubble() {
  view_ = NULL;
}

void SettingLevelBubble::OnWidgetClosing(views::Widget* widget) {
  if (view_ && view_->GetWidget() == widget) {
    view_->GetWidget()->RemoveObserver(this);
    view_ = NULL;
  }
  // Update states.
  current_percent_ = target_percent_;
  target_time_ = TimeTicks();
  last_animation_update_time_ = TimeTicks();
  last_target_update_time_ = TimeTicks();
  hide_timer_.Stop();
  StopAnimation();
}

SettingLevelBubbleView* SettingLevelBubble::CreateView() {
  SettingLevelBubbleDelegateView* delegate = new SettingLevelBubbleDelegateView;
  views::Widget* widget = views::BubbleDelegateView::CreateBubble(delegate);
  widget->AddObserver(this);

#if !defined(USE_AURA)
  {
    std::vector<int> params;
    params.push_back(1);  // show_while_screen_is_locked_
    chromeos::WmIpc::instance()->SetWindowType(
        widget->GetNativeView(),
        chromeos::WM_IPC_WINDOW_CHROME_INFO_BUBBLE,
        &params);
  }
#endif

  // Hold on to the content view.
  return delegate->view();
}

void SettingLevelBubble::OnHideTimeout() {
  // Start fading away.
  if (view_) {
    SettingLevelBubbleDelegateView* delegate =
        static_cast<SettingLevelBubbleDelegateView*>
        (view_->GetWidget()->widget_delegate());
    delegate->StartFade(false);
  }
}

void SettingLevelBubble::OnAnimationTimeout() {
  const TimeTicks now = TimeTicks::Now();
  const int64 remaining_ms = (target_time_ - now).InMilliseconds();

  if (remaining_ms <= 0) {
    current_percent_ = target_percent_;
    StopAnimation();
  } else {
    // Figure out what fraction of the total time until we want to reach the
    // target has elapsed since the last update.
    const double remaining_percent = target_percent_ - current_percent_;
    const int64 elapsed_ms =
        (now - last_animation_update_time_).InMilliseconds();
    current_percent_ +=
        remaining_percent *
        (static_cast<double>(elapsed_ms) / (elapsed_ms + remaining_ms));
  }
  last_animation_update_time_ = now;

  if (view_)
    view_->SetLevel(current_percent_);
}

void SettingLevelBubble::UpdateTargetPercent(double percent) {
  target_percent_ = LimitPercent(percent);
  const TimeTicks now = TimeTicks::Now();

  if (current_percent_ < 0.0) {
    // If we're setting the level for the first time, no need to animate.
    current_percent_ = target_percent_;
    if (view_)
      view_->SetLevel(current_percent_);
  } else {
    // Use the time since the last request as a hint for the duration of the
    // animation.  This makes us automatically adapt to the repeat rate if a key
    // is being held down to change a setting (which prevents us from lagging
    // behind when the key is finally released).
    int64 duration_ms = kInitialAnimationDurationMs;
    if (!last_target_update_time_.is_null())
      duration_ms = min(kInitialAnimationDurationMs,
                        (now - last_target_update_time_).InMilliseconds());
    target_time_ = now + TimeDelta::FromMilliseconds(duration_ms);

    if (!is_animating_) {
      animation_timer_.Start(FROM_HERE,
                             TimeDelta::FromMilliseconds(kAnimationIntervalMs),
                             this,
                             &SettingLevelBubble::OnAnimationTimeout);
      is_animating_ = true;
      last_animation_update_time_ = now;
    }
  }

  last_target_update_time_ = now;
}

void SettingLevelBubble::StopAnimation() {
  animation_timer_.Stop();
  is_animating_ = false;
}

}  // namespace chromeos
