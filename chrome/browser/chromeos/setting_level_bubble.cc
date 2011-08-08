// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/setting_level_bubble.h"

#include <gdk/gdk.h>

#include "base/timer.h"
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

namespace {

const int kBubbleShowTimeoutSec = 2;
const int kAnimationDurationMs = 200;

// Horizontal position of the center of the bubble on the screen: 0 is left
// edge, 0.5 is center, 1 is right edge.
const double kBubbleXRatio = 0.5;

// Vertical gap from the bottom of the screen in pixels.
const int kBubbleBottomGap = 30;

int LimitPercent(int percent) {
  if (percent < 0)
    percent = 0;
  else if (percent > 100)
    percent = 100;
  return percent;
}

}  // namespace

namespace chromeos {

// Temporary helper routine. Tries to first return the widget from the
// most-recently-focused normal browser window, then from a login
// background, and finally NULL if both of those fail.
// TODO(glotov): remove this in favor of enabling Bubble class act
// without |parent| specified. crosbug.com/4025
static views::Widget* GetToplevelWidget() {
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
}

SettingLevelBubble::SettingLevelBubble(SkBitmap* increase_icon,
                                       SkBitmap* decrease_icon,
                                       SkBitmap* disabled_icon)
    : previous_percent_(-1),
      current_percent_(-1),
      increase_icon_(increase_icon),
      decrease_icon_(decrease_icon),
      disabled_icon_(disabled_icon),
      bubble_(NULL),
      view_(NULL),
      animation_(this) {
  animation_.SetSlideDuration(kAnimationDurationMs);
  animation_.SetTweenType(ui::Tween::LINEAR);
}

SettingLevelBubble::~SettingLevelBubble() {}

void SettingLevelBubble::ShowBubble(int percent, bool enabled) {
  percent = LimitPercent(percent);
  if (previous_percent_ == -1)
    previous_percent_ = percent;
  current_percent_ = percent;

  SkBitmap* icon = increase_icon_;
  if (!enabled || current_percent_ == 0)
    icon = disabled_icon_;
  else if (current_percent_ < previous_percent_)
    icon = decrease_icon_;

  if (!bubble_) {
    views::Widget* parent_widget = GetToplevelWidget();
    if (parent_widget == NULL) {
      LOG(WARNING) << "Unable to locate parent widget to display a bubble";
      return;
    }
    DCHECK(view_ == NULL);
    view_ = new SettingLevelBubbleView;
    view_->Init(icon, previous_percent_, enabled);

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

    // ShowFocusless doesn't set ESC accelerator.
    bubble_ = Bubble::ShowFocusless(parent_widget,
                                    position_relative_to,
                                    BubbleBorder::FLOAT,
                                    view_,  // contents
                                    this,   // delegate
                                    true);  // show while screen is locked
  } else {
    DCHECK(view_);
    timeout_timer_.Stop();
    view_->SetIcon(icon);
  }

  view_->SetEnabled(enabled);

  if (animation_.is_animating())
    animation_.End();
  animation_.Reset();
  animation_.Show();

  timeout_timer_.Start(base::TimeDelta::FromSeconds(kBubbleShowTimeoutSec),
                       this, &SettingLevelBubble::OnTimeout);
}

void SettingLevelBubble::HideBubble() {
  if (bubble_)
    bubble_->Close();
}

void SettingLevelBubble::UpdateWithoutShowingBubble(int percent, bool enabled) {
  if (view_)
    view_->SetEnabled(enabled);

  percent = LimitPercent(percent);

  previous_percent_ =
      animation_.is_animating() ?
      animation_.GetCurrentValue() :
      current_percent_;
  if (previous_percent_ < 0)
    previous_percent_ = percent;
  current_percent_ = percent;

  if (animation_.is_animating())
    animation_.End();
  animation_.Reset();
  animation_.Show();
}

void SettingLevelBubble::OnTimeout() {
  HideBubble();
}

void SettingLevelBubble::BubbleClosing(Bubble* bubble, bool) {
  DCHECK(bubble == bubble_);
  timeout_timer_.Stop();
  animation_.Stop();
  bubble_ = NULL;
  view_ = NULL;
}

bool SettingLevelBubble::CloseOnEscape() {
  return true;
}

bool SettingLevelBubble::FadeInOnShow() {
  return false;
}

void SettingLevelBubble::AnimationEnded(const ui::Animation* animation) {
  previous_percent_ = current_percent_;
}

void SettingLevelBubble::AnimationProgressed(const ui::Animation* animation) {
  if (view_) {
    view_->SetLevel(
        ui::Tween::ValueBetween(animation->GetCurrentValue(),
                                previous_percent_,
                                current_percent_));
  }
}

}  // namespace chromeos
