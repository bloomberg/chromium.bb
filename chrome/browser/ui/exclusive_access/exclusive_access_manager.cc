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
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "ui/events/keycodes/keyboard_codes.h"

using content::WebContents;

const base::Feature ExclusiveAccessManager::kSimplifiedUIFeature = {
    "ViewsSimplifiedFullscreenUI",
#if defined(USE_AURA)
    base::FEATURE_ENABLED_BY_DEFAULT,
#else
    base::FEATURE_DISABLED_BY_DEFAULT,
#endif
};

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

  if (mouse_lock_controller_.IsMouseLockSilentlyAccepted() &&
      (!fullscreen_controller_.IsWindowFullscreenForTabOrPending() ||
       fullscreen_controller_.IsUserAcceptedFullscreen()))
    return EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE;

  if (!fullscreen_controller_.IsWindowFullscreenForTabOrPending()) {
    if (mouse_lock_controller_.IsMouseLocked())
      return EXCLUSIVE_ACCESS_BUBBLE_TYPE_MOUSELOCK_EXIT_INSTRUCTION;
    if (mouse_lock_controller_.IsMouseLockRequested())
      return EXCLUSIVE_ACCESS_BUBBLE_TYPE_MOUSELOCK_BUTTONS;
    if (fullscreen_controller_.IsExtensionFullscreenOrPending())
      return EXCLUSIVE_ACCESS_BUBBLE_TYPE_EXTENSION_FULLSCREEN_EXIT_INSTRUCTION;
    if (fullscreen_controller_.IsControllerInitiatedFullscreen() && !app_mode)
      return EXCLUSIVE_ACCESS_BUBBLE_TYPE_BROWSER_FULLSCREEN_EXIT_INSTRUCTION;
    return EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE;
  }

  if (fullscreen_controller_.IsUserAcceptedFullscreen()) {
    if (fullscreen_controller_.IsPrivilegedFullscreenForTab())
      return EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE;
    if (mouse_lock_controller_.IsMouseLocked())
      return EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_MOUSELOCK_EXIT_INSTRUCTION;
    if (mouse_lock_controller_.IsMouseLockRequested())
      return EXCLUSIVE_ACCESS_BUBBLE_TYPE_MOUSELOCK_BUTTONS;
    return EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_EXIT_INSTRUCTION;
  }

  if (mouse_lock_controller_.IsMouseLockRequested())
    return EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_MOUSELOCK_BUTTONS;
  return EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_BUTTONS;
}

void ExclusiveAccessManager::UpdateExclusiveAccessExitBubbleContent() {
  GURL url = GetExclusiveAccessBubbleURL();
  ExclusiveAccessBubbleType bubble_type = GetExclusiveAccessExitBubbleType();

  // If bubble displays buttons, unlock mouse to allow pressing them.
  if (exclusive_access_bubble::ShowButtonsForType(bubble_type) &&
      mouse_lock_controller_.IsMouseLocked())
    mouse_lock_controller_.UnlockMouse();

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
bool ExclusiveAccessManager::IsSimplifiedFullscreenUIEnabled() {
  return base::FeatureList::IsEnabled(kSimplifiedUIFeature);
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
    exclusive_access_context_->OnExclusiveAccessUserInput();
    return false;
  }

  bool handled = false;
  handled = fullscreen_controller_.HandleUserPressedEscape();
  handled |= mouse_lock_controller_.HandleUserPressedEscape();
  return handled;
}

void ExclusiveAccessManager::OnAcceptExclusiveAccessPermission() {
  bool updateBubble =
      mouse_lock_controller_.OnAcceptExclusiveAccessPermission();
  updateBubble |= fullscreen_controller_.OnAcceptExclusiveAccessPermission();
  if (updateBubble)
    UpdateExclusiveAccessExitBubbleContent();
}

void ExclusiveAccessManager::OnDenyExclusiveAccessPermission() {
  bool updateBubble = mouse_lock_controller_.OnDenyExclusiveAccessPermission();
  updateBubble |= fullscreen_controller_.OnDenyExclusiveAccessPermission();
  if (updateBubble)
    UpdateExclusiveAccessExitBubbleContent();
}

void ExclusiveAccessManager::ExitExclusiveAccess() {
  fullscreen_controller_.ExitExclusiveAccessToPreviousState();
  mouse_lock_controller_.LostMouseLock();
}
