// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"

#include "base/command_line.h"
#include "build/build_config.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/exclusive_access/mouse_lock_controller.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "ui/events/keycodes/keyboard_codes.h"

using content::WebContents;

namespace {

// Time in milliseconds to hold the Esc key in order to exit full screen.
// TODO(dominickn) refactor the way timings/input handling works so this
// constant doesn't have to be in this file.
const int kHoldEscapeTimeMs = 1500;

}

ExclusiveAccessManager::ExclusiveAccessManager(
    ExclusiveAccessContext* exclusive_access_context)
    : exclusive_access_context_(exclusive_access_context),
      fullscreen_controller_(this),
      mouse_lock_controller_(this) {
}

ExclusiveAccessManager::~ExclusiveAccessManager() {
}

ExclusiveAccessBubbleType
ExclusiveAccessManager::GetExclusiveAccessExitBubbleType() const {
  // In kiosk and exclusive app mode we always want to be fullscreen and do not
  // want to show exit instructions for browser mode fullscreen.
  bool app_mode = false;
#if !defined(OS_MACOSX)  // App mode (kiosk) is not available on Mac yet.
  app_mode = chrome::IsRunningInAppMode();
#endif

  if (fullscreen_controller_.IsWindowFullscreenForTabOrPending()) {
    if (!fullscreen_controller_.IsTabFullscreen())
      return EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_EXIT_INSTRUCTION;

    if (mouse_lock_controller_.IsMouseLockedSilently() ||
        fullscreen_controller_.IsPrivilegedFullscreenForTab()) {
      return EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE;
    }

    if (IsExperimentalKeyboardLockUIEnabled())
      return EXCLUSIVE_ACCESS_BUBBLE_TYPE_KEYBOARD_LOCK_EXIT_INSTRUCTION;

    if (mouse_lock_controller_.IsMouseLocked())
      return EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_MOUSELOCK_EXIT_INSTRUCTION;

    return EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_EXIT_INSTRUCTION;
  }

  if (mouse_lock_controller_.IsMouseLockedSilently())
    return EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE;

  if (mouse_lock_controller_.IsMouseLocked())
    return EXCLUSIVE_ACCESS_BUBBLE_TYPE_MOUSELOCK_EXIT_INSTRUCTION;

  if (fullscreen_controller_.IsExtensionFullscreenOrPending())
    return EXCLUSIVE_ACCESS_BUBBLE_TYPE_EXTENSION_FULLSCREEN_EXIT_INSTRUCTION;

  if (fullscreen_controller_.IsControllerInitiatedFullscreen() && !app_mode)
    return EXCLUSIVE_ACCESS_BUBBLE_TYPE_BROWSER_FULLSCREEN_EXIT_INSTRUCTION;

  return EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE;
}

void ExclusiveAccessManager::UpdateExclusiveAccessExitBubbleContent() {
  GURL url = GetExclusiveAccessBubbleURL();
  ExclusiveAccessBubbleType bubble_type = GetExclusiveAccessExitBubbleType();

  exclusive_access_context_->UpdateExclusiveAccessExitBubbleContent(
      url, bubble_type);
}

GURL ExclusiveAccessManager::GetExclusiveAccessBubbleURL() const {
  GURL result = fullscreen_controller_.GetURLForExclusiveAccessBubble();
  if (!result.is_valid())
    result = mouse_lock_controller_.GetURLForExclusiveAccessBubble();
  return result;
}

// static
bool ExclusiveAccessManager::IsExperimentalKeyboardLockUIEnabled() {
  return base::FeatureList::IsEnabled(features::kExperimentalKeyboardLockUI);
}

// static
bool ExclusiveAccessManager::IsSimplifiedFullscreenUIEnabled() {
#if defined(OS_MACOSX)
  // Always enabled on Mac (the mouse cursor tracking required to implement the
  // non-simplified version is not implemented).
  return true;
#else
  return base::FeatureList::IsEnabled(features::kSimplifiedFullscreenUI);
#endif
}

void ExclusiveAccessManager::OnTabDeactivated(WebContents* web_contents) {
  fullscreen_controller_.OnTabDeactivated(web_contents);
  mouse_lock_controller_.OnTabDeactivated(web_contents);
}

void ExclusiveAccessManager::OnTabDetachedFromView(WebContents* web_contents) {
  fullscreen_controller_.OnTabDetachedFromView(web_contents);
  mouse_lock_controller_.OnTabDetachedFromView(web_contents);
}

void ExclusiveAccessManager::OnTabClosing(WebContents* web_contents) {
  fullscreen_controller_.OnTabClosing(web_contents);
  mouse_lock_controller_.OnTabClosing(web_contents);
}

bool ExclusiveAccessManager::HandleUserKeyPress(
    const content::NativeWebKeyboardEvent& event) {
  if (event.windowsKeyCode != ui::VKEY_ESCAPE) {
    OnUserInput();
    return false;
  }

  if (IsExperimentalKeyboardLockUIEnabled()) {
    if (event.type() == content::NativeWebKeyboardEvent::KeyUp &&
        hold_timer_.IsRunning()) {
      // Seeing a key up event on Esc with the hold timer running cancels the
      // timer and doesn't exit. This means the user pressed Esc, but not long
      // enough to trigger an exit
      hold_timer_.Stop();
    } else if (event.type() == content::NativeWebKeyboardEvent::RawKeyDown &&
               !hold_timer_.IsRunning()) {
      // Seeing a key down event on Esc when the hold timer is stopped starts
      // the timer. When the timer reaches 0, the callback will trigger an exit
      // from fullscreen/mouselock.
      hold_timer_.Start(
          FROM_HERE, base::TimeDelta::FromMilliseconds(kHoldEscapeTimeMs),
          base::Bind(&ExclusiveAccessManager::HandleUserHeldEscape,
                     base::Unretained(this)));
    }
    // We never handle the keyboard event.
    return false;
  }

  bool handled = false;
  handled = fullscreen_controller_.HandleUserPressedEscape();
  handled |= mouse_lock_controller_.HandleUserPressedEscape();
  return handled;
}

void ExclusiveAccessManager::OnUserInput() {
  exclusive_access_context_->OnExclusiveAccessUserInput();
}

void ExclusiveAccessManager::ExitExclusiveAccess() {
  fullscreen_controller_.ExitExclusiveAccessToPreviousState();
  mouse_lock_controller_.LostMouseLock();
}

void ExclusiveAccessManager::RecordBubbleReshownUMA(
    ExclusiveAccessBubbleType type) {
  // Figure out whether each of fullscreen, mouselock is in effect.
  bool fullscreen = false;
  bool mouselock = false;
  switch (type) {
    case EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE:
      // None in effect.
      break;
    case EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_EXIT_INSTRUCTION:
    case EXCLUSIVE_ACCESS_BUBBLE_TYPE_KEYBOARD_LOCK_EXIT_INSTRUCTION:
    case EXCLUSIVE_ACCESS_BUBBLE_TYPE_BROWSER_FULLSCREEN_EXIT_INSTRUCTION:
    case EXCLUSIVE_ACCESS_BUBBLE_TYPE_EXTENSION_FULLSCREEN_EXIT_INSTRUCTION:
      // Only fullscreen in effect.
      fullscreen = true;
      break;
    case EXCLUSIVE_ACCESS_BUBBLE_TYPE_MOUSELOCK_EXIT_INSTRUCTION:
      // Only mouselock in effect.
      mouselock = true;
      break;
    case EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_MOUSELOCK_EXIT_INSTRUCTION:
      // Both in effect.
      fullscreen = true;
      mouselock = true;
      break;
  }

  if (fullscreen)
    fullscreen_controller_.RecordBubbleReshownUMA();
  if (mouselock)
    mouse_lock_controller_.RecordBubbleReshownUMA();
}

void ExclusiveAccessManager::HandleUserHeldEscape() {
  fullscreen_controller_.HandleUserPressedEscape();
  mouse_lock_controller_.HandleUserPressedEscape();
}
