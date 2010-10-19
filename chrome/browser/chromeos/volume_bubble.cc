// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/volume_bubble.h"

#include <gdk/gdk.h>

#include "base/timer.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/browser/chromeos/volume_bubble_view.h"
#include "chrome/browser/views/info_bubble.h"
#include "views/widget/root_view.h"

namespace {

const int kBubbleShowTimeoutSec = 2;
const int kAnimationDurationMs = 200;

// Horizontal relative position: 0 - leftmost, 0.5 - center, 1 - rightmost.
const double kVolumeBubbleXRatio = 0.5;

// Vertical gap from the bottom of the screen in pixels.
const int kVolumeBubbleBottomGap = 30;

}  // namespace

namespace chromeos {

// Temporary helper routine. Returns the widget from the most-recently-focused
// normal browser window or NULL.
// TODO(glotov): remove this in favor of enabling InfoBubble class act
// without |parent| specified. crosbug.com/4025
static views::Widget* GetToplevelWidget() {
  // We just use the default profile here -- this gets overridden as needed
  // in Chrome OS depending on whether the user is logged in or not.
  Browser* browser =
      BrowserList::FindBrowserWithType(
          ProfileManager::GetDefaultProfile(),
          Browser::TYPE_NORMAL,
          true);  // match_incognito
  if (!browser)
    return NULL;

  views::RootView* root =
      views::Widget::FindRootView(
          GTK_WINDOW(browser->window()->GetNativeHandle()));
  DCHECK(root);
  if (!root)
    return NULL;

  return root->GetWidget();
}

VolumeBubble::VolumeBubble()
    : previous_percent_(-1),
      current_percent_(-1),
      bubble_(NULL),
      view_(NULL),
      animation_(this) {
  animation_.SetSlideDuration(kAnimationDurationMs);
  animation_.SetTweenType(Tween::LINEAR);
}

void VolumeBubble::ShowVolumeBubble(int percent) {
  if (percent < 0)
    percent = 0;
  if (percent > 100)
    percent = 100;
  if (previous_percent_ == -1)
    previous_percent_ = percent;
  current_percent_ = percent;
  if (!bubble_) {
    views::Widget* widget = GetToplevelWidget();
    if (widget == NULL)
      return;
    DCHECK(view_ == NULL);
    view_ = new VolumeBubbleView;
    view_->Init(previous_percent_);
    // Calculate position of the volume bubble.
    // TODO(glotov): Place volume bubble over the keys initiated the
    // volume change. This metric must be specific to the given
    // architecture. crosbug.com/4028
    gfx::Rect bounds;
    widget->GetBounds(&bounds, false);
    const gfx::Size view_size = view_->GetPreferredSize();
    // Note that (x, y) is the point of the center of the bubble.
    const int x = view_size.width() / 2 +
        kVolumeBubbleXRatio * (bounds.width() - view_size.width());
    const int y = bounds.height() - view_size.height() / 2 -
        kVolumeBubbleBottomGap;
    bubble_ = InfoBubble::ShowFocusless(widget, gfx::Rect(x, y, 0, 20),
                                        BubbleBorder::FLOAT, view_, this);
  } else {
    DCHECK(view_);
    timeout_timer_.Stop();
  }
  if (animation_.is_animating())
    animation_.End();
  animation_.Reset();
  animation_.Show();
  timeout_timer_.Start(base::TimeDelta::FromSeconds(kBubbleShowTimeoutSec),
                       this, &VolumeBubble::OnTimeout);
}

void VolumeBubble::OnTimeout() {
  if (bubble_)
    bubble_->Close();
}

void VolumeBubble::InfoBubbleClosing(InfoBubble* info_bubble, bool) {
  DCHECK(info_bubble == bubble_);
  timeout_timer_.Stop();
  animation_.Stop();
  bubble_ = NULL;
  view_ = NULL;
}

void VolumeBubble::AnimationEnded(const Animation* animation) {
  previous_percent_ = current_percent_;
}

void VolumeBubble::AnimationProgressed(const Animation* animation) {
  if (view_) {
    view_->Update(
        Tween::ValueBetween(animation->GetCurrentValue(),
                            previous_percent_,
                            current_percent_));
  }
}

}  // namespace chromeos
