// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/new_back_shortcut_bubble.h"

#include <utility>

#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/ui/views/exclusive_access_bubble_views_context.h"
#include "chrome/browser/ui/views/subtle_notification_view.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/border.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace {

const int kPopupTopPx = 45;
const int kSlideInDurationMs = 350;
const int kSlideOutDurationMs = 700;
const int kShowDurationMs = 3800;

}

NewBackShortcutBubble::NewBackShortcutBubble(
    ExclusiveAccessBubbleViewsContext* context)
    : bubble_view_context_(context),
      animation_(new gfx::SlideAnimation(this)),
      view_(new SubtleNotificationView(nullptr)),
      popup_(SubtleNotificationView::CreatePopupWidget(
          bubble_view_context_->GetBubbleParentView(),
          view_,
          false)) {
}

NewBackShortcutBubble::~NewBackShortcutBubble() {
  // We might need to delete the widget asynchronously. See rationale in
  // ~ExclusiveAccessBubbleViews.
  popup_->Close();
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, popup_);
}

bool NewBackShortcutBubble::IsVisible() const {
  return popup_->IsVisible();
}

void NewBackShortcutBubble::UpdateContent(bool forward) {
  // Note: The key names are parameters so that we can vary by operating system
  // or change the direction of the arrow as necessary (see
  // https://crbug.com/612685).

#if defined(OS_MACOSX)
  // U+2318 = PLACE OF INTEREST SIGN (Mac Command symbol).
  base::string16 accelerator = base::WideToUTF16(L"\x2318");
#else
  base::string16 accelerator = l10n_util::GetStringUTF16(IDS_APP_ALT_KEY);
#endif

  int message_id = forward ? IDS_PRESS_ALT_RIGHT_TO_GO_FORWARD
                           : IDS_PRESS_ALT_LEFT_TO_GO_BACK;
  // U+2192 = RIGHTWARDS ARROW; U+2190 = LEFTWARDS ARROW.
  base::string16 arrow_key = base::WideToUTF16(forward ? L"\x2192" : L"\x2190");
  view_->UpdateContent(
      l10n_util::GetStringFUTF16(message_id, accelerator, arrow_key),
      base::string16());

  view_->SetSize(GetPopupRect(true).size());
  popup_->SetBounds(GetPopupRect(false));

  // Show the bubble.
  animation_->SetSlideDuration(kSlideInDurationMs);
  animation_->Show();

  // Wait a few seconds before hiding.
  hide_timeout_.Start(FROM_HERE,
                      base::TimeDelta::FromMilliseconds(kShowDurationMs), this,
                      &NewBackShortcutBubble::OnTimerElapsed);
}

void NewBackShortcutBubble::Hide() {
  hide_timeout_.Stop();
  OnTimerElapsed();
}

void NewBackShortcutBubble::AnimationProgressed(
    const gfx::Animation* animation) {
  float opacity = static_cast<float>(animation_->CurrentValueBetween(0.0, 1.0));
  if (opacity == 0) {
    popup_->Hide();
  } else {
    if (!popup_->IsVisible())
      popup_->Show();

    popup_->SetOpacity(opacity);
  }
}

void NewBackShortcutBubble::AnimationEnded(const gfx::Animation* animation) {
  AnimationProgressed(animation);
}

gfx::Rect NewBackShortcutBubble::GetPopupRect(
    bool ignore_animation_state) const {
  gfx::Size size = view_->GetPreferredSize();
  gfx::Rect widget_bounds = bubble_view_context_->GetClientAreaBoundsInScreen();
  int x = widget_bounds.x() + (widget_bounds.width() - size.width()) / 2;
  // |desired_top| is the top of the bubble area including the shadow.
  int desired_top = kPopupTopPx - view_->border()->GetInsets().top();
  int y = widget_bounds.y() + desired_top;
  return gfx::Rect(gfx::Point(x, y), size);
}

void NewBackShortcutBubble::OnTimerElapsed() {
  // Hide the bubble.
  animation_->SetSlideDuration(kSlideOutDurationMs);
  animation_->Hide();
}
