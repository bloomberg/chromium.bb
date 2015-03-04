// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/exclusive_access/mouse_lock_controller.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"

using content::RenderViewHost;
using content::WebContents;

MouseLockController::MouseLockController(ExclusiveAccessManager* manager)
    : ExclusiveAccessControllerBase(manager),
      mouse_lock_state_(MOUSELOCK_NOT_REQUESTED) {
}

MouseLockController::~MouseLockController() {
}

bool MouseLockController::IsMouseLocked() const {
  return mouse_lock_state_ == MOUSELOCK_ACCEPTED ||
         mouse_lock_state_ == MOUSELOCK_ACCEPTED_SILENTLY;
}

bool MouseLockController::IsMouseLockSilentlyAccepted() const {
  return mouse_lock_state_ == MOUSELOCK_ACCEPTED_SILENTLY;
}

void MouseLockController::RequestToLockMouse(WebContents* web_contents,
                                             bool user_gesture,
                                             bool last_unlocked_by_target) {
  DCHECK(!IsMouseLocked());
  NotifyMouseLockChange();

  // Must have a user gesture to prevent misbehaving sites from constantly
  // re-locking the mouse. Exceptions are when the page has unlocked
  // (i.e. not the user), or if we're in tab fullscreen (user gesture required
  // for that)
  if (!last_unlocked_by_target && !user_gesture &&
      !exclusive_access_manager()
           ->fullscreen_controller()
           ->IsFullscreenForTabOrPending(web_contents)) {
    web_contents->GotResponseToLockMouseRequest(false);
    return;
  }
  SetTabWithExclusiveAccess(web_contents);
  ExclusiveAccessBubbleType bubble_type =
      exclusive_access_manager()->GetExclusiveAccessExitBubbleType();

  switch (GetMouseLockSetting(web_contents->GetURL())) {
    case CONTENT_SETTING_ALLOW:
      // If bubble already displaying buttons we must not lock the mouse yet,
      // or it would prevent pressing those buttons. Instead, merge the request.
      if (!exclusive_access_manager()
               ->fullscreen_controller()
               ->IsPrivilegedFullscreenForTab() &&
          exclusive_access_bubble::ShowButtonsForType(bubble_type)) {
        mouse_lock_state_ = MOUSELOCK_REQUESTED;
      } else {
        // Lock mouse.
        if (web_contents->GotResponseToLockMouseRequest(true)) {
          if (last_unlocked_by_target) {
            mouse_lock_state_ = MOUSELOCK_ACCEPTED_SILENTLY;
          } else {
            mouse_lock_state_ = MOUSELOCK_ACCEPTED;
          }
        } else {
          SetTabWithExclusiveAccess(nullptr);
          mouse_lock_state_ = MOUSELOCK_NOT_REQUESTED;
        }
      }
      break;
    case CONTENT_SETTING_BLOCK:
      web_contents->GotResponseToLockMouseRequest(false);
      SetTabWithExclusiveAccess(nullptr);
      mouse_lock_state_ = MOUSELOCK_NOT_REQUESTED;
      break;
    case CONTENT_SETTING_ASK:
      mouse_lock_state_ = MOUSELOCK_REQUESTED;
      break;
    default:
      NOTREACHED();
  }
  exclusive_access_manager()->UpdateExclusiveAccessExitBubbleContent();
}

void MouseLockController::ExitExclusiveAccessIfNecessary() {
  NotifyTabExclusiveAccessLost();
}

void MouseLockController::NotifyTabExclusiveAccessLost() {
  WebContents* tab = exclusive_access_tab();
  if (tab) {
    if (IsMouseLockRequested()) {
      tab->GotResponseToLockMouseRequest(false);
      NotifyMouseLockChange();
    } else {
      UnlockMouse();
    }
    SetTabWithExclusiveAccess(nullptr);
    mouse_lock_state_ = MOUSELOCK_NOT_REQUESTED;
    exclusive_access_manager()->UpdateExclusiveAccessExitBubbleContent();
  }
}

bool MouseLockController::HandleUserPressedEscape() {
  if (IsMouseLocked() || IsMouseLockRequested()) {
    ExitExclusiveAccessIfNecessary();
    return true;
  }

  return false;
}

void MouseLockController::ExitExclusiveAccessToPreviousState() {
  // Nothing to do for mouse lock.
}

bool MouseLockController::OnAcceptExclusiveAccessPermission() {
  ExclusiveAccessBubbleType bubble_type =
      exclusive_access_manager()->GetExclusiveAccessExitBubbleType();
  bool mouse_lock = false;
  exclusive_access_bubble::PermissionRequestedByType(bubble_type, nullptr,
                                                     &mouse_lock);
  DCHECK(!(mouse_lock && IsMouseLocked()));

  if (mouse_lock && !IsMouseLocked()) {
    DCHECK(IsMouseLockRequested());

    HostContentSettingsMap* settings_map = exclusive_access_manager()
                                               ->context()
                                               ->GetProfile()
                                               ->GetHostContentSettingsMap();

    GURL url = GetExclusiveAccessBubbleURL();
    ContentSettingsPattern pattern = ContentSettingsPattern::FromURL(url);

    // TODO(markusheintz): We should allow patterns for all possible URLs here.
    //
    // Do not store preference on file:// URLs, they don't have a clean
    // origin policy.
    // TODO(estark): Revisit this when crbug.com/455882 is fixed.
    if (!url.SchemeIsFile() && pattern.IsValid()) {
      settings_map->SetContentSetting(pattern,
                                      ContentSettingsPattern::Wildcard(),
                                      CONTENT_SETTINGS_TYPE_MOUSELOCK,
                                      std::string(), CONTENT_SETTING_ALLOW);
    }

    WebContents* tab = exclusive_access_tab();
    if (tab && tab->GotResponseToLockMouseRequest(true)) {
      mouse_lock_state_ = MOUSELOCK_ACCEPTED;
    } else {
      mouse_lock_state_ = MOUSELOCK_NOT_REQUESTED;
      SetTabWithExclusiveAccess(nullptr);
    }
    NotifyMouseLockChange();
    return true;
  }

  return false;
}

bool MouseLockController::OnDenyExclusiveAccessPermission() {
  WebContents* tab = exclusive_access_tab();

  if (tab && IsMouseLockRequested()) {
    mouse_lock_state_ = MOUSELOCK_NOT_REQUESTED;
    tab->GotResponseToLockMouseRequest(false);
    SetTabWithExclusiveAccess(nullptr);
    NotifyMouseLockChange();
    return true;
  }

  return false;
}

void MouseLockController::LostMouseLock() {
  mouse_lock_state_ = MOUSELOCK_NOT_REQUESTED;
  SetTabWithExclusiveAccess(nullptr);
  NotifyMouseLockChange();
  exclusive_access_manager()->UpdateExclusiveAccessExitBubbleContent();
}

bool MouseLockController::IsMouseLockRequested() const {
  return mouse_lock_state_ == MOUSELOCK_REQUESTED;
}

void MouseLockController::NotifyMouseLockChange() {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_MOUSE_LOCK_CHANGED,
      content::Source<MouseLockController>(this),
      content::NotificationService::NoDetails());
}

void MouseLockController::UnlockMouse() {
  WebContents* tab = exclusive_access_tab();

  if (!tab)
    return;

  content::RenderWidgetHostView* mouse_lock_view = nullptr;
  FullscreenController* fullscreen_controller =
      exclusive_access_manager()->fullscreen_controller();
  if ((fullscreen_controller->exclusive_access_tab() == tab) &&
      fullscreen_controller->IsPrivilegedFullscreenForTab()) {
    mouse_lock_view =
        exclusive_access_tab()->GetFullscreenRenderWidgetHostView();
  }

  if (!mouse_lock_view) {
    RenderViewHost* const rvh = exclusive_access_tab()->GetRenderViewHost();
    if (rvh)
      mouse_lock_view = rvh->GetView();
  }

  if (mouse_lock_view)
    mouse_lock_view->UnlockMouse();
}

ContentSetting MouseLockController::GetMouseLockSetting(const GURL& url) const {
  // Always ask on file:// URLs, since we can't meaningfully make the
  // decision stick for a particular origin.
  // TODO(estark): Revisit this when crbug.com/455882 is fixed.
  if (url.SchemeIsFile())
    return CONTENT_SETTING_ASK;

  if (exclusive_access_manager()
          ->fullscreen_controller()
          ->IsPrivilegedFullscreenForTab())
    return CONTENT_SETTING_ALLOW;

  HostContentSettingsMap* settings_map = exclusive_access_manager()
                                             ->context()
                                             ->GetProfile()
                                             ->GetHostContentSettingsMap();
  return settings_map->GetContentSetting(
      url, url, CONTENT_SETTINGS_TYPE_MOUSELOCK, std::string());
}
