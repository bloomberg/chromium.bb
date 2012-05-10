// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/fullscreen_controller.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/download/download_shelf.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"

using content::RenderViewHost;
using content::UserMetricsAction;
using content::WebContents;

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

bool FullscreenController::IsFullscreenForBrowser() const {
  return window_->IsFullscreen() && !tab_caused_fullscreen_;
}

bool FullscreenController::IsFullscreenForTabOrPending() const {
  return fullscreened_tab_ != NULL;
}

bool FullscreenController::IsFullscreenForTabOrPending(
    const WebContents* tab) const {
  const TabContentsWrapper* wrapper =
      TabContentsWrapper::GetCurrentWrapperForContents(tab);
  if (!wrapper || (wrapper != fullscreened_tab_))
    return false;
  DCHECK(tab == browser_->GetSelectedWebContents());
  return true;
}

bool FullscreenController::IsMouseLockRequested() const {
  return mouse_lock_state_ == MOUSELOCK_REQUESTED;
}

bool FullscreenController::IsMouseLocked() const {
  return mouse_lock_state_ == MOUSELOCK_ACCEPTED;
}

void FullscreenController::RequestToLockMouse(WebContents* tab,
                                              bool /* user_gesture */) {
  // TODO(scheib) user_gesture required for Mouse Lock in Windowed Mode.
  // See http://crbug.com/107013, which will land in multiple patches.

  DCHECK(!IsMouseLocked());

  // Mouse Lock is only permitted when browser is in tab fullscreen.
  if (!IsFullscreenForTabOrPending(tab)) {
    tab->GotResponseToLockMouseRequest(false);
    return;
  }

  switch (GetMouseLockSetting(tab->GetURL())) {
    case CONTENT_SETTING_ALLOW:
      if (tab_fullscreen_accepted_) {
        if (tab->GotResponseToLockMouseRequest(true))
          mouse_lock_state_ = MOUSELOCK_ACCEPTED;
      } else {
        mouse_lock_state_ = MOUSELOCK_REQUESTED;
      }
      break;
    case CONTENT_SETTING_BLOCK:
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

void FullscreenController::ToggleFullscreenModeForTab(WebContents* tab,
                                                      bool enter_fullscreen) {
  if (tab != browser_->GetSelectedWebContents())
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
      TogglePresentationModeInternal(true);
#else
      ToggleFullscreenModeInternal(true);
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
        TogglePresentationModeInternal(true);
#else
        ToggleFullscreenModeInternal(true);
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
void FullscreenController::TogglePresentationMode() {
  TogglePresentationModeInternal(false);
}
#endif

void FullscreenController::ToggleFullscreenMode() {
  extension_caused_fullscreen_ = GURL();
  ToggleFullscreenModeInternal(false);
}

void FullscreenController::ToggleFullscreenModeWithExtension(
    const GURL& extension_url) {
  // |extension_caused_fullscreen_| will be reset if this causes fullscreen to
  // exit.
  extension_caused_fullscreen_ = extension_url;
  ToggleFullscreenModeInternal(false);
}

void FullscreenController::LostMouseLock() {
  mouse_lock_state_ = MOUSELOCK_NOT_REQUESTED;
  UpdateFullscreenExitBubbleContent();
}

void FullscreenController::OnTabClosing(WebContents* web_contents) {
  if (IsFullscreenForTabOrPending(web_contents)) {
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
  ContentSettingsPattern pattern = ContentSettingsPattern::FromURL(url);
  if (mouse_lock) {
    DCHECK(IsMouseLockRequested());
    // TODO(markusheintz): We should allow patterns for all possible URLs here.
    if (pattern.IsValid()) {
      settings_map->SetContentSetting(
          pattern, ContentSettingsPattern::Wildcard(),
          CONTENT_SETTINGS_TYPE_MOUSELOCK, std::string(),
          CONTENT_SETTING_ALLOW);
    }
    mouse_lock_state_ =
        fullscreened_tab_->web_contents()->GotResponseToLockMouseRequest(true) ?
        MOUSELOCK_ACCEPTED : MOUSELOCK_NOT_REQUESTED;
  }
  if (!tab_fullscreen_accepted_) {
    if (pattern.IsValid()) {
      settings_map->SetContentSetting(
          pattern, ContentSettingsPattern::Wildcard(),
          CONTENT_SETTINGS_TYPE_FULLSCREEN, std::string(),
          CONTENT_SETTING_ALLOW);
    }
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
    DCHECK(IsMouseLockRequested());
    mouse_lock_state_ = MOUSELOCK_NOT_REQUESTED;
    fullscreened_tab_->web_contents()->GotResponseToLockMouseRequest(false);
    if (!fullscreen)
      UpdateFullscreenExitBubbleContent();
  }

  if (fullscreen)
    ExitTabbedFullscreenModeIfNecessary();
}

void FullscreenController::WindowFullscreenStateChanged() {
  MessageLoop::current()->PostTask(FROM_HERE,
      base::Bind(&FullscreenController::NotifyFullscreenChange, this));
  bool exiting_fullscreen;
#if defined(OS_MACOSX)
  exiting_fullscreen = !window_->InPresentationMode();
#else
  exiting_fullscreen = !window_->IsFullscreen();
#endif
  if (exiting_fullscreen)
    NotifyTabOfFullscreenExitIfNecessary();
#if !defined(OS_CHROMEOS)
  if (exiting_fullscreen)
    window_->GetDownloadShelf()->Unhide();
  else
    window_->GetDownloadShelf()->Hide();
#endif
}

bool FullscreenController::HandleUserPressedEscape() {
  if (!IsFullscreenForTabOrPending())
    return false;
  ExitTabbedFullscreenModeIfNecessary();
  return true;
}

FullscreenController::~FullscreenController() {}

void FullscreenController::NotifyTabOfFullscreenExitIfNecessary() {
  if (fullscreened_tab_) {
    RenderViewHost* rvh =
        fullscreened_tab_->web_contents()->GetRenderViewHost();
    fullscreened_tab_ = NULL;
    tab_caused_fullscreen_ = false;
    tab_fullscreen_accepted_ = false;
    mouse_lock_state_ = MOUSELOCK_NOT_REQUESTED;
    if (rvh)
      rvh->ExitFullscreen();
  } else {
    DCHECK_EQ(mouse_lock_state_, MOUSELOCK_NOT_REQUESTED);
  }

  UpdateFullscreenExitBubbleContent();
}

void FullscreenController::ExitTabbedFullscreenModeIfNecessary() {
  if (tab_caused_fullscreen_)
    ToggleFullscreenMode();
  else
    NotifyTabOfFullscreenExitIfNecessary();
}

void FullscreenController::UpdateFullscreenExitBubbleContent() {
  GURL url;
  if (fullscreened_tab_)
    url = fullscreened_tab_->web_contents()->GetURL();
  else if (!extension_caused_fullscreen_.is_empty())
    url = extension_caused_fullscreen_;

  window_->UpdateFullscreenExitBubbleContent(url,
                                             GetFullscreenExitBubbleType());
}

void FullscreenController::NotifyFullscreenChange() {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_FULLSCREEN_CHANGED,
      content::Source<FullscreenController>(this),
      content::NotificationService::NoDetails());
}

FullscreenExitBubbleType FullscreenController::GetFullscreenExitBubbleType()
    const {
  if (!fullscreened_tab_) {
    DCHECK_EQ(MOUSELOCK_NOT_REQUESTED, mouse_lock_state_);
    return !extension_caused_fullscreen_.is_empty() ?
        FEB_TYPE_BROWSER_EXTENSION_FULLSCREEN_EXIT_INSTRUCTION :
        FEB_TYPE_BROWSER_FULLSCREEN_EXIT_INSTRUCTION;
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

#if defined(OS_MACOSX)
void FullscreenController::TogglePresentationModeInternal(bool for_tab) {
  bool entering_fullscreen = !window_->InPresentationMode();
  GURL url;
  if (for_tab) {
    url = browser_->GetSelectedWebContents()->GetURL();
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
void FullscreenController::ToggleFullscreenModeInternal(bool for_tab) {
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
    url = browser_->GetSelectedWebContents()->GetURL();
    tab_fullscreen_accepted_ = entering_fullscreen &&
        GetFullscreenSetting(url) == CONTENT_SETTING_ALLOW;
  } else {
    if (!extension_caused_fullscreen_.is_empty())
      url = extension_caused_fullscreen_;
    content::RecordAction(UserMetricsAction("ToggleFullscreen"));
  }
  if (entering_fullscreen) {
    window_->EnterFullscreen(url, GetFullscreenExitBubbleType());
  } else {
    window_->ExitFullscreen();
    extension_caused_fullscreen_ = GURL();
  }

  // Once the window has become fullscreen it'll call back to
  // WindowFullscreenStateChanged(). We don't do this immediately as
  // BrowserWindow::EnterFullscreen() asks for bookmark_bar_state_, so we let
  // the BrowserWindow invoke WindowFullscreenStateChanged when appropriate.
}
