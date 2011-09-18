// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/setting_level_bubble.h"

#include <algorithm>

#include <gdk/gdk.h>

#include "chrome/browser/chromeos/login/background_view.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/webui_login_display.h"
#include "chrome/browser/chromeos/setting_level_bubble_view.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/bubble/bubble.h"
#include "ui/gfx/screen.h"
#include "views/widget/root_view.h"

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

// Temporary helper routine. Tries to first return the widget from the
// most-recently-focused normal browser window, then from a login
// background, and finally NULL if both of those fail.
// TODO(glotov): remove this in favor of enabling Bubble class act
// without |parent| specified. crosbug.com/4025
static views::Widget* GetToplevelWidget() {
#if defined(USE_AURA)
  // TODO(saintlou): Need to fix in PureViews.
  return WebUILoginDisplay::GetLoginWindow();
#else
  GtkWindow* window = NULL;

  // We just use the default profile here -- this gets overridden as needed
  // in Chrome OS depending on whether the user is logged in or not.
  Browser* browser =
      BrowserList::FindTabbedBrowser(
          ProfileManager::GetDefaultProfile(),
          true);  // match_incognito
  if (browser) {
    window = GTK_WINDOW(browser->window()->GetNativeHandle());
  } else {
    // Otherwise, see if there's a background window that we can use.
    BackgroundView* background = LoginUtils::Get()->GetBackgroundView();
    if (background)
      window = GTK_WINDOW(background->GetNativeWindow());
  }

  if (window)
    return views::Widget::GetWidgetForNativeWindow(window);
  else
    return WebUILoginDisplay::GetLoginWindow();
#endif
}

SettingLevelBubble::SettingLevelBubble(SkBitmap* increase_icon,
                                       SkBitmap* decrease_icon,
                                       SkBitmap* disabled_icon)
    : current_percent_(-1.0),
      target_percent_(-1.0),
      increase_icon_(increase_icon),
      decrease_icon_(decrease_icon),
      disabled_icon_(disabled_icon),
      bubble_(NULL),
      view_(NULL),
      is_animating_(false) {
}

SettingLevelBubble::~SettingLevelBubble() {}

void SettingLevelBubble::ShowBubble(double percent, bool enabled) {
  const double old_target_percent = target_percent_;
  UpdateTargetPercent(percent);

  SkBitmap* icon = increase_icon_;
  if (!enabled || target_percent_ == 0)
    icon = disabled_icon_;
  else if (old_target_percent >= 0 && target_percent_ < old_target_percent)
    icon = decrease_icon_;

  if (!bubble_) {
    views::Widget* parent_widget = GetToplevelWidget();
    if (parent_widget == NULL) {
      LOG(WARNING) << "Unable to locate parent widget to display a bubble";
      return;
    }
    DCHECK(view_ == NULL);
    view_ = new SettingLevelBubbleView;
    view_->Init(icon, current_percent_, enabled);

    // Calculate the position in screen coordinates that the bubble should
    // "point" at (since we use BubbleBorder::FLOAT, this position actually
    // specifies the center of the bubble).
    const gfx::Rect monitor_area =
        gfx::Screen::GetMonitorAreaNearestWindow(
            GTK_WIDGET(parent_widget->GetNativeWindow()));
    const gfx::Size view_size = view_->GetPreferredSize();
    const gfx::Rect position_relative_to(
        monitor_area.x() + kBubbleXRatio * monitor_area.width(),
        monitor_area.bottom() - view_size.height() / 2 - kBubbleBottomGap,
        0, 0);

    bubble_ = Bubble::ShowFocusless(parent_widget,
                                    position_relative_to,
                                    views::BubbleBorder::FLOAT,
                                    view_,  // contents
                                    this,   // delegate
                                    true);  // show while screen is locked
    // TODO(derat): We probably shouldn't be using Bubble.  It'd be nice to call
    // bubble_->set_fade_away_on_close(true) here, but then, if ShowBubble()
    // gets called while the bubble is fading away, we end up just adjusting the
    // value on the disappearing bubble; ideally we'd have a way to cancel the
    // fade and show the bubble at full opacity for another
    // kBubbleShowTimeoutMs.
  } else {
    DCHECK(view_);
    hide_timer_.Stop();
    view_->SetIcon(icon);
    view_->SetEnabled(enabled);
  }

  hide_timer_.Start(FROM_HERE,
                    base::TimeDelta::FromMilliseconds(kBubbleShowTimeoutMs),
                    this, &SettingLevelBubble::OnHideTimeout);
}

void SettingLevelBubble::HideBubble() {
  if (bubble_)
    bubble_->Close();
}

void SettingLevelBubble::UpdateWithoutShowingBubble(double percent,
                                                    bool enabled) {
  UpdateTargetPercent(percent);
  if (view_)
    view_->SetEnabled(enabled);
}

void SettingLevelBubble::OnHideTimeout() {
  HideBubble();
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

void SettingLevelBubble::BubbleClosing(Bubble* bubble, bool) {
  DCHECK(bubble == bubble_);
  hide_timer_.Stop();
  StopAnimation();
  bubble_ = NULL;
  view_ = NULL;
  current_percent_ = target_percent_;
  target_time_ = TimeTicks();
  last_animation_update_time_ = TimeTicks();
  last_target_update_time_ = TimeTicks();
}

bool SettingLevelBubble::CloseOnEscape() {
  return true;
}

bool SettingLevelBubble::FadeInOnShow() {
  return false;
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
