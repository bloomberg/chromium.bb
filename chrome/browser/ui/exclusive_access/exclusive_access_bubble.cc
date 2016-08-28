// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/exclusive_access/exclusive_access_bubble.h"

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "extensions/browser/extension_registry.h"
#include "ui/gfx/geometry/rect.h"

// NOTE(koz): Linux doesn't use the thick shadowed border, so we add padding
// here.
#if defined(OS_LINUX)
const int ExclusiveAccessBubble::kPaddingPx = 8;
#else
const int ExclusiveAccessBubble::kPaddingPx = 15;
#endif
const int ExclusiveAccessBubble::kInitialDelayMs = 3800;
const int ExclusiveAccessBubble::kIdleTimeMs = 2300;
const int ExclusiveAccessBubble::kSnoozeNotificationsTimeMs = 900000;  // 15m.
const int ExclusiveAccessBubble::kPositionCheckHz = 10;
const int ExclusiveAccessBubble::kSlideInRegionHeightPx = 4;
const int ExclusiveAccessBubble::kSlideInDurationMs = 350;
const int ExclusiveAccessBubble::kSlideOutDurationMs = 700;
const int ExclusiveAccessBubble::kPopupTopPx = 45;
const int ExclusiveAccessBubble::kSimplifiedPopupTopPx = 45;

ExclusiveAccessBubble::ExclusiveAccessBubble(
    ExclusiveAccessManager* manager,
    const GURL& url,
    ExclusiveAccessBubbleType bubble_type)
    : manager_(manager), url_(url), bubble_type_(bubble_type) {
  DCHECK_NE(EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE, bubble_type_);
}

ExclusiveAccessBubble::~ExclusiveAccessBubble() {
}

void ExclusiveAccessBubble::OnUserInput() {
  if (!ExclusiveAccessManager::IsSimplifiedFullscreenUIEnabled())
    return;

  // We got some user input; reset the idle timer.
  idle_timeout_.Stop();  // If the timer isn't running, this is a no-op.
  idle_timeout_.Start(FROM_HERE,
                      base::TimeDelta::FromMilliseconds(kIdleTimeMs), this,
                      &ExclusiveAccessBubble::CheckMousePosition);

  // If the notification suppression timer has elapsed, re-show it.
  if (!suppress_notify_timeout_.IsRunning()) {
    manager_->RecordBubbleReshownUMA(bubble_type_);

    ShowAndStartTimers();
    return;
  }

  // The timer has not elapsed, but the user provided some input. Reset the
  // timer. (We only want to re-show the message after a period of inactivity.)
  suppress_notify_timeout_.Reset();
}

void ExclusiveAccessBubble::StartWatchingMouse() {
  // Start the initial delay timer and begin watching the mouse.
  if (ExclusiveAccessManager::IsSimplifiedFullscreenUIEnabled()) {
    ShowAndStartTimers();
  } else {
    hide_timeout_.Start(FROM_HERE,
                        base::TimeDelta::FromMilliseconds(kInitialDelayMs),
                        this, &ExclusiveAccessBubble::CheckMousePosition);

    last_mouse_pos_ = GetCursorScreenPoint();
  }
  mouse_position_checker_.Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(1000 / kPositionCheckHz),
      this, &ExclusiveAccessBubble::CheckMousePosition);
}

void ExclusiveAccessBubble::StopWatchingMouse() {
  hide_timeout_.Stop();
  idle_timeout_.Stop();
  mouse_position_checker_.Stop();
}

bool ExclusiveAccessBubble::IsWatchingMouse() const {
  return mouse_position_checker_.IsRunning();
}

void ExclusiveAccessBubble::CheckMousePosition() {
  // Desired behavior (without the "simplified" flag):
  //
  // +------------+-----------------------------+------------+
  // | _  _  _  _ | Exit full screen mode (F11) | _  _  _  _ |  Slide-in region
  // | _  _  _  _ \_____________________________/ _  _  _  _ |  Neutral region
  // |                                                       |  Slide-out region
  // :                                                       :
  //
  // * If app is not active, we hide the popup.
  // * If the mouse is offscreen or in the slide-out region, we hide the popup.
  // * If the mouse goes idle, we hide the popup.
  // * If the mouse is in the slide-in-region and not idle, we show the popup.
  // * If the mouse is in the neutral region and not idle, and the popup is
  //   currently sliding out, we show it again.  This facilitates users
  //   correcting us if they try to mouse horizontally towards the popup and
  //   unintentionally drop too low.
  // * Otherwise, we do nothing, because the mouse is in the neutral region and
  //   either the popup is hidden or the mouse is not idle, so we don't want to
  //   change anything's state.
  //
  // With the "simplified" flag, we ignore all this and just show and hide based
  // on timers (not mouse position).

  gfx::Point cursor_pos;
  if (!ExclusiveAccessManager::IsSimplifiedFullscreenUIEnabled()) {
    cursor_pos = GetCursorScreenPoint();

    // Check to see whether the mouse is idle.
    if (cursor_pos != last_mouse_pos_) {
      // OnUserInput() will reset the idle timer in simplified mode. In classic
      // mode, we need to do this here.
      idle_timeout_.Stop();  // If the timer isn't running, this is a no-op.
      idle_timeout_.Start(FROM_HERE,
                          base::TimeDelta::FromMilliseconds(kIdleTimeMs), this,
                          &ExclusiveAccessBubble::CheckMousePosition);
    }
    last_mouse_pos_ = cursor_pos;
  }

  if (ExclusiveAccessManager::IsSimplifiedFullscreenUIEnabled() ||
      !IsWindowActive() || !WindowContainsPoint(cursor_pos) ||
      cursor_pos.y() >= GetPopupRect(true).bottom() ||
      !idle_timeout_.IsRunning()) {
    // Classic mode: The cursor is offscreen, in the slide-out region, or idle.
    // Simplified mode: Always come here (never check for mouse entering the top
    // of screen).
    if (!hide_timeout_.IsRunning())
      Hide();
  } else if (cursor_pos.y() < kSlideInRegionHeightPx &&
             CanMouseTriggerSlideIn()) {
    Show();
  } else if (IsAnimating()) {
    // The cursor is not idle and either it's in the slide-in region or it's in
    // the neutral region and we're sliding in or out.
    Show();
  }
}

void ExclusiveAccessBubble::ExitExclusiveAccess() {
  manager_->ExitExclusiveAccess();
}

base::string16 ExclusiveAccessBubble::GetCurrentMessageText() const {
  return exclusive_access_bubble::GetLabelTextForType(
      bubble_type_, url_,
      extensions::ExtensionRegistry::Get(manager_->context()->GetProfile()));
}

base::string16 ExclusiveAccessBubble::GetCurrentDenyButtonText() const {
  return exclusive_access_bubble::GetDenyButtonTextForType(bubble_type_);
}

base::string16 ExclusiveAccessBubble::GetCurrentAllowButtonText() const {
  return exclusive_access_bubble::GetAllowButtonTextForType(bubble_type_, url_);
}

base::string16 ExclusiveAccessBubble::GetInstructionText(
    const base::string16& accelerator) const {
  return exclusive_access_bubble::GetInstructionTextForType(bubble_type_,
                                                            accelerator);
}

void ExclusiveAccessBubble::ShowAndStartTimers() {
  Show();

  // Do not allow the notification to hide for a few seconds.
  hide_timeout_.Start(FROM_HERE,
                      base::TimeDelta::FromMilliseconds(kInitialDelayMs), this,
                      &ExclusiveAccessBubble::CheckMousePosition);

  // Do not show the notification again until a long time has elapsed.
  suppress_notify_timeout_.Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(kSnoozeNotificationsTimeMs),
      this, &ExclusiveAccessBubble::CheckMousePosition);
}
