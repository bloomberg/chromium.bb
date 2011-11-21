// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/fullscreen_controller.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "content/browser/user_metrics.h"
#include "content/public/browser/notification_service.h"

FullscreenController::FullscreenController(BrowserWindow* window,
                                           Profile* profile,
                                           Browser* browser)
  : window_(window),
    profile_(profile),
    browser_(browser),
    fullscreened_tab_(NULL),
    tab_caused_fullscreen_(false),
    tab_fullscreen_accepted_(false),
    mouse_lock_state_(MOUSELOCK_NOT_REQUESTED) {
}

FullscreenController::~FullscreenController() {}

bool FullscreenController::IsFullscreenForTab() const {
  return fullscreened_tab_ != NULL;
}

bool FullscreenController::IsFullscreenForTab(const TabContents* tab) const {
  const TabContentsWrapper* wrapper =
      TabContentsWrapper::GetCurrentWrapperForContents(tab);
  if (!wrapper || (wrapper != fullscreened_tab_))
    return false;
  DCHECK(tab == browser_->GetSelectedTabContents());
  DCHECK(window_->IsFullscreen());
  return true;
}

void FullscreenController::RequestToLockMouse(TabContents* tab) {
  // Mouse Lock is only permitted when browser is in tab fullscreen.
  if (!IsFullscreenForTab(tab)) {
    tab->GotResponseToLockMouseRequest(false);
    return;
  }

  if (mouse_lock_state_ == MOUSELOCK_ACCEPTED) {
    tab->GotResponseToLockMouseRequest(true);
    return;
  }

  switch (GetMouseLockSetting(tab->GetURL())) {
    case CONTENT_SETTING_ALLOW:
      mouse_lock_state_ = MOUSELOCK_ACCEPTED;
      tab->GotResponseToLockMouseRequest(true);
      break;
    case CONTENT_SETTING_BLOCK:
      mouse_lock_state_ = MOUSELOCK_NOT_REQUESTED;
      tab->GotResponseToLockMouseRequest(false);
      break;
    case CONTENT_SETTING_ASK:
      mouse_lock_state_ = MOUSELOCK_REQUESTED;
      break;
    default:
      NOTREACHED();
  }
  UpdateFullscreenExitBubbleContent();
}

void FullscreenController::ToggleFullscreenModeForTab(TabContents* tab,
                                                      bool enter_fullscreen) {
  if (tab != browser_->GetSelectedTabContents())
    return;

  bool in_browser_or_tab_fullscreen_mode;
#if defined(OS_MACOSX)
  in_browser_or_tab_fullscreen_mode = window_->InPresentationMode();
#else
  in_browser_or_tab_fullscreen_mode = window_->IsFullscreen();
#endif

  if (enter_fullscreen) {
    fullscreened_tab_ = TabContentsWrapper::GetCurrentWrapperForContents(tab);
    if (!in_browser_or_tab_fullscreen_mode) {
      tab_caused_fullscreen_ = true;
#if defined(OS_MACOSX)
      TogglePresentationMode(true);
#else
      ToggleFullscreenMode(true);
#endif
    } else {
      // We need to update the fullscreen exit bubble, e.g., going from browser
      // fullscreen to tab fullscreen will need to show different content.
      const GURL& url = tab->GetURL();
      if (!tab_fullscreen_accepted_) {
        tab_fullscreen_accepted_ =
            GetFullscreenSetting(url) == CONTENT_SETTING_ALLOW;
      }
      UpdateFullscreenExitBubbleContent();
    }
  } else {
    if (in_browser_or_tab_fullscreen_mode) {
      if (tab_caused_fullscreen_) {
#if defined(OS_MACOSX)
        TogglePresentationMode(true);
#else
        ToggleFullscreenMode(true);
#endif
      } else {
        // If currently there is a tab in "tab fullscreen" mode and fullscreen
        // was not caused by it (i.e., previously it was in "browser fullscreen"
        // mode), we need to switch back to "browser fullscreen" mode. In this
        // case, all we have to do is notifying the tab that it has exited "tab
        // fullscreen" mode.
        NotifyTabOfFullscreenExitIfNecessary();
      }
    }
  }
}

#if defined(OS_MACOSX)
void FullscreenController::TogglePresentationMode(bool for_tab) {
  bool entering_fullscreen = !window_->InPresentationMode();
  GURL url;
  if (for_tab) {
    url = browser_->GetSelectedTabContents()->GetURL();
    tab_fullscreen_accepted_ = entering_fullscreen &&
        GetFullscreenSetting(url) == CONTENT_SETTING_ALLOW;
  }
  if (entering_fullscreen)
    window_->EnterPresentationMode(url, GetFullscreenExitBubbleType());
  else
    window_->ExitPresentationMode();
  WindowFullscreenStateChanged();
}
#endif

// TODO(koz): Change |for_tab| to an enum.
void FullscreenController::ToggleFullscreenMode(bool for_tab) {
  bool entering_fullscreen = !window_->IsFullscreen();

#if !defined(OS_MACOSX)
  // In kiosk mode, we always want to be fullscreen. When the browser first
  // starts we're not yet fullscreen, so let the initial toggle go through.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kKioskMode) &&
      window_->IsFullscreen())
    return;
#endif

  GURL url;
  if (for_tab) {
    url = browser_->GetSelectedTabContents()->GetURL();
    tab_fullscreen_accepted_ = entering_fullscreen &&
        GetFullscreenSetting(url) == CONTENT_SETTING_ALLOW;
  } else {
    UserMetrics::RecordAction(UserMetricsAction("ToggleFullscreen"));
  }
  if (entering_fullscreen)
    window_->EnterFullscreen(url, GetFullscreenExitBubbleType());
  else
    window_->ExitFullscreen();

  // Once the window has become fullscreen it'll call back to
  // WindowFullscreenStateChanged(). We don't do this immediately as
  // BrowserWindow::EnterFullscreen() asks for bookmark_bar_state_, so we let
  // the BrowserWindow invoke WindowFullscreenStateChanged when appropriate.

  // TODO: convert mac to invoke WindowFullscreenStateChanged once it updates
  // the necessary state of the frame.
#if defined(OS_MACOSX)
  WindowFullscreenStateChanged();
#endif
}

void FullscreenController::LostMouseLock() {
  mouse_lock_state_ = MOUSELOCK_NOT_REQUESTED;
  UpdateFullscreenExitBubbleContent();
}

void FullscreenController::OnTabClosing(TabContents* tab_contents) {
  if (IsFullscreenForTab(tab_contents)) {
    ExitTabbedFullscreenModeIfNecessary();
    // The call to exit fullscreen may result in asynchronous notification of
    // fullscreen state change (e.g., on Linux). We don't want to rely on it
    // to call NotifyTabOfFullscreenExitIfNecessary(), because at that point
    // |fullscreen_tab_| may not be valid. Instead, we call it here to clean up
    // tab fullscreen related state.
    NotifyTabOfFullscreenExitIfNecessary();
  }
}

void FullscreenController::OnTabDeactivated(TabContentsWrapper* contents) {
  if (contents == fullscreened_tab_)
    ExitTabbedFullscreenModeIfNecessary();
}

void FullscreenController::OnAcceptFullscreenPermission(
    const GURL& url,
    FullscreenExitBubbleType bubble_type) {
  bool mouse_lock = false;
  bool fullscreen = false;
  fullscreen_bubble::PermissionRequestedByType(bubble_type, &fullscreen,
                                               &mouse_lock);
  DCHECK(fullscreened_tab_);
  DCHECK_NE(tab_fullscreen_accepted_, fullscreen);

  HostContentSettingsMap* settings_map = profile_->GetHostContentSettingsMap();
  if (mouse_lock) {
    DCHECK_EQ(mouse_lock_state_, MOUSELOCK_REQUESTED);
    settings_map->SetContentSetting(ContentSettingsPattern::FromURL(url),
        ContentSettingsPattern::Wildcard(), CONTENT_SETTINGS_TYPE_MOUSELOCK,
        std::string(), CONTENT_SETTING_ALLOW);
    mouse_lock_state_ =
        fullscreened_tab_->tab_contents()->GotResponseToLockMouseRequest(true) ?
        MOUSELOCK_ACCEPTED : MOUSELOCK_NOT_REQUESTED;
  }
  if (!tab_fullscreen_accepted_) {
    settings_map->SetContentSetting(ContentSettingsPattern::FromURL(url),
        ContentSettingsPattern::Wildcard(), CONTENT_SETTINGS_TYPE_FULLSCREEN,
        std::string(), CONTENT_SETTING_ALLOW);
    tab_fullscreen_accepted_ = true;
  }
  UpdateFullscreenExitBubbleContent();
}

void FullscreenController::OnDenyFullscreenPermission(
    FullscreenExitBubbleType bubble_type) {
  bool mouse_lock = false;
  bool fullscreen = false;
  fullscreen_bubble::PermissionRequestedByType(bubble_type, &fullscreen,
                                               &mouse_lock);
  DCHECK(fullscreened_tab_);
  DCHECK_NE(tab_fullscreen_accepted_, fullscreen);

  if (mouse_lock) {
    DCHECK_EQ(mouse_lock_state_, MOUSELOCK_REQUESTED);
    mouse_lock_state_ = MOUSELOCK_NOT_REQUESTED;
    fullscreened_tab_->tab_contents()->GotResponseToLockMouseRequest(false);
    if (!fullscreen)
      UpdateFullscreenExitBubbleContent();
  }

  if (fullscreen)
    ExitTabbedFullscreenModeIfNecessary();
}

void FullscreenController::WindowFullscreenStateChanged() {
  MessageLoop::current()->PostTask(FROM_HERE,
      base::Bind(&FullscreenController::NotifyFullscreenChange, this));
  bool notify_tab_of_exit;
#if defined(OS_MACOSX)
  notify_tab_of_exit = !window_->InPresentationMode();
#else
  notify_tab_of_exit = !window_->IsFullscreen();
#endif
  if (notify_tab_of_exit)
    NotifyTabOfFullscreenExitIfNecessary();
}

bool FullscreenController::HandleUserPressedEscape() {
  if (!IsFullscreenForTab())
    return false;
  ExitTabbedFullscreenModeIfNecessary();
  return true;
}

void FullscreenController::NotifyTabOfFullscreenExitIfNecessary() {
  if (fullscreened_tab_)
    fullscreened_tab_->ExitFullscreenMode();
  else
    DCHECK_EQ(mouse_lock_state_, MOUSELOCK_NOT_REQUESTED);

  fullscreened_tab_ = NULL;
  tab_caused_fullscreen_ = false;
  tab_fullscreen_accepted_ = false;
  mouse_lock_state_ = MOUSELOCK_NOT_REQUESTED;

  UpdateFullscreenExitBubbleContent();
}

void FullscreenController::ExitTabbedFullscreenModeIfNecessary() {
  if (tab_caused_fullscreen_)
    ToggleFullscreenMode(false);
  else
    NotifyTabOfFullscreenExitIfNecessary();
}

void FullscreenController::UpdateFullscreenExitBubbleContent() {
  GURL url;
  if (fullscreened_tab_)
    url = fullscreened_tab_->tab_contents()->GetURL();

  window_->UpdateFullscreenExitBubbleContent(url,
                                             GetFullscreenExitBubbleType());
}

void FullscreenController::NotifyFullscreenChange() {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_FULLSCREEN_CHANGED,
      content::Source<FullscreenController>(this),
      content::NotificationService::NoDetails());
}

FullscreenExitBubbleType
    FullscreenController::GetFullscreenExitBubbleType() const {
  if (!fullscreened_tab_) {
    DCHECK_EQ(MOUSELOCK_NOT_REQUESTED, mouse_lock_state_);
    return FEB_TYPE_BROWSER_FULLSCREEN_EXIT_INSTRUCTION;
  }
  if (fullscreened_tab_ && !tab_fullscreen_accepted_) {
    DCHECK_NE(MOUSELOCK_ACCEPTED, mouse_lock_state_);
    return mouse_lock_state_ == MOUSELOCK_REQUESTED ?
        FEB_TYPE_FULLSCREEN_MOUSELOCK_BUTTONS : FEB_TYPE_FULLSCREEN_BUTTONS;
  }
  if (mouse_lock_state_ == MOUSELOCK_REQUESTED)
    return FEB_TYPE_MOUSELOCK_BUTTONS;
  return mouse_lock_state_ == MOUSELOCK_ACCEPTED ?
      FEB_TYPE_FULLSCREEN_MOUSELOCK_EXIT_INSTRUCTION :
      FEB_TYPE_FULLSCREEN_EXIT_INSTRUCTION;
}

ContentSetting
    FullscreenController::GetFullscreenSetting(const GURL& url) const {
  if (url.SchemeIsFile())
    return CONTENT_SETTING_ALLOW;

  return profile_->GetHostContentSettingsMap()->GetContentSetting(url, url,
      CONTENT_SETTINGS_TYPE_FULLSCREEN, std::string());

}

ContentSetting
    FullscreenController::GetMouseLockSetting(const GURL& url) const {
  if (url.SchemeIsFile())
    return CONTENT_SETTING_ALLOW;

  HostContentSettingsMap* settings_map = profile_->GetHostContentSettingsMap();
  return settings_map->GetContentSetting(url, url,
      CONTENT_SETTINGS_TYPE_MOUSELOCK, std::string());
}
