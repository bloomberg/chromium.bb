// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/fullscreen_exit_bubble.h"

#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "grit/generated_resources.h"
#include "grit/ui_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/rect.h"

const int FullscreenExitBubble::kPaddingPx = 0;
const int FullscreenExitBubble::kInitialDelayMs = 3800;
const int FullscreenExitBubble::kIdleTimeMs = 2300;
const int FullscreenExitBubble::kPositionCheckHz = 10;
const int FullscreenExitBubble::kSlideInRegionHeightPx = 4;
const int FullscreenExitBubble::kSlideInDurationMs = 350;
const int FullscreenExitBubble::kSlideOutDurationMs = 700;
const int FullscreenExitBubble::kPopupTopPx = 15;

FullscreenExitBubble::FullscreenExitBubble(Browser* browser,
                                           const GURL& url,
                                           FullscreenExitBubbleType bubble_type)
    : browser_(browser),
      url_(url),
      bubble_type_(bubble_type) {
  DCHECK_NE(FEB_TYPE_NONE, bubble_type_);
}

FullscreenExitBubble::~FullscreenExitBubble() {
}

void FullscreenExitBubble::StartWatchingMouse() {
  // Start the initial delay timer and begin watching the mouse.
  initial_delay_.Start(FROM_HERE,
                       base::TimeDelta::FromMilliseconds(kInitialDelayMs), this,
                       &FullscreenExitBubble::CheckMousePosition);
  gfx::Point cursor_pos = GetCursorScreenPoint();
  last_mouse_pos_ = cursor_pos;
  mouse_position_checker_.Start(FROM_HERE,
      base::TimeDelta::FromMilliseconds(1000 / kPositionCheckHz), this,
      &FullscreenExitBubble::CheckMousePosition);
}

void FullscreenExitBubble::StopWatchingMouse() {
  initial_delay_.Stop();
  idle_timeout_.Stop();
  mouse_position_checker_.Stop();
}

void FullscreenExitBubble::CheckMousePosition() {
  // Desired behavior:
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

  gfx::Point cursor_pos = GetCursorScreenPoint();

  // Check to see whether the mouse is idle.
  if (cursor_pos != last_mouse_pos_) {
    // The mouse moved; reset the idle timer.
    idle_timeout_.Stop();  // If the timer isn't running, this is a no-op.
    idle_timeout_.Start(FROM_HERE,
                        base::TimeDelta::FromMilliseconds(kIdleTimeMs), this,
                        &FullscreenExitBubble::CheckMousePosition);
  }
  last_mouse_pos_ = cursor_pos;

  if (!IsWindowActive() ||
      !WindowContainsPoint(cursor_pos) ||
      (cursor_pos.y() >= GetPopupRect(true).bottom()) ||
      !idle_timeout_.IsRunning()) {
    // The cursor is offscreen, in the slide-out region, or idle.
    if (!initial_delay_.IsRunning()) {
      Hide();
    }
  } else if ((cursor_pos.y() < kSlideInRegionHeightPx) ||
             IsAnimating()) {
    // The cursor is not idle, and either it's in the slide-in region or it's in
    // the neutral region and we're sliding out.
    Show();
  }
}

void FullscreenExitBubble::ToggleFullscreen() {
  browser_->ExecuteCommand(IDC_FULLSCREEN);
}

void FullscreenExitBubble::Accept() {
  browser_->OnAcceptFullscreenPermission(url_, bubble_type_);
}

void FullscreenExitBubble::Cancel() {
  browser_->OnDenyFullscreenPermission(bubble_type_);
}

string16 FullscreenExitBubble::GetCurrentMessageText() const {
  return fullscreen_bubble::GetLabelTextForType(bubble_type_, url_);
}

string16 FullscreenExitBubble::GetCurrentDenyButtonText() const {
  return fullscreen_bubble::GetDenyButtonTextForType(bubble_type_);
}

string16 FullscreenExitBubble::GetAllowButtonText() const {
  return l10n_util::GetStringUTF16(IDS_FULLSCREEN_ALLOW);
}

string16 FullscreenExitBubble::GetInstructionText() const {
  return l10n_util::GetStringFUTF16(IDS_FULLSCREEN_PRESS_ESC_TO_EXIT,
      l10n_util::GetStringUTF16(IDS_APP_ESC_KEY));
}
