// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_FULLSCREEN_CONTROLLER_H_
#define CHROME_BROWSER_UI_FULLSCREEN_CONTROLLER_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/ui/fullscreen_exit_bubble_type.h"
#include "chrome/common/content_settings.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Browser;
class BrowserWindow;
class GURL;
class Profile;
class TabContents;

namespace content {
class WebContents;
}

// There are two different kinds of fullscreen mode - "tab fullscreen" and
// "browser fullscreen". "Tab fullscreen" refers to when a tab enters
// fullscreen mode via the JS fullscreen API, and "browser fullscreen" refers
// to the user putting the browser itself into fullscreen mode from the UI. The
// difference is that tab fullscreen has implications for how the contents of
// the tab render (eg: a video element may grow to consume the whole tab),
// whereas browser fullscreen mode doesn't. Therefore if a user forces an exit
// from tab fullscreen, we need to notify the tab so it can stop rendering in
// its fullscreen mode.

// This class implements fullscreen and mouselock behaviour.
class FullscreenController : public base::RefCounted<FullscreenController>,
                             public content::NotificationObserver {
 public:
  FullscreenController(BrowserWindow* window,
                       Profile* profile,
                       Browser* browser);

  // Querying.

  // Returns true if the window is currently fullscreen and was initially
  // transitioned to fullscreen by a browser (vs tab) mode transition.
  bool IsFullscreenForBrowser() const;

  // Returns true if fullscreen has been caused by a tab.
  // The window may still be transitioning, and window_->IsFullscreen()
  // may still return false.
  bool IsFullscreenForTabOrPending() const;
  bool IsFullscreenForTabOrPending(
      const content::WebContents* web_contents) const;

#if defined(OS_WIN)
  // Returns whether we are currently in a Metro snap view.
  bool IsInMetroSnapMode();
#endif

  bool IsMouseLockRequested() const;
  bool IsMouseLocked() const;

  // Requests.
  void RequestToLockMouse(content::WebContents* web_contents,
                          bool user_gesture,
                          bool last_unlocked_by_target);
  void ToggleFullscreenModeForTab(content::WebContents* web_contents,
                                  bool enter_fullscreen);
#if defined(OS_WIN)
  // API that puts the window into a mode suitable for rendering when Chrome
  // is rendered in a 20% screen-width Metro snap view on Windows 8.
  void SetMetroSnapMode(bool enable);
#endif
#if defined(OS_MACOSX)
  void TogglePresentationMode();
#endif
  void ToggleFullscreenMode();
  // Extension API implementation uses this method to toggle fullscreen mode.
  // The extension's name is displayed in the full screen bubble UI to attribute
  // the cause of the full screen state change.
  void ToggleFullscreenModeWithExtension(const GURL& extension_url);

  // Notifications.
  void LostMouseLock();
  void OnTabClosing(content::WebContents* web_contents);
  void OnTabDeactivated(TabContents* contents);
  void OnAcceptFullscreenPermission(const GURL& url,
                                    FullscreenExitBubbleType bubble_type);
  void OnDenyFullscreenPermission(FullscreenExitBubbleType bubble_type);
  void WindowFullscreenStateChanged();
  bool HandleUserPressedEscape();

  FullscreenExitBubbleType GetFullscreenExitBubbleType() const;

  // content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  friend class base::RefCounted<FullscreenController>;

  enum MouseLockState {
    MOUSELOCK_NOT_REQUESTED,
    // The page requests to lock the mouse and the user hasn't responded to the
    // request.
    MOUSELOCK_REQUESTED,
    // Mouse lock has been allowed by the user.
    MOUSELOCK_ACCEPTED,
    // Mouse lock has been silently accepted, no notification to user.
    MOUSELOCK_ACCEPTED_SILENTLY
  };

  virtual ~FullscreenController();

  // Notifies the tab that it has been forced out of fullscreen and mouse lock
  // mode if necessary.
  void NotifyTabOfExitIfNecessary();

  // Makes the browser exit fullscreen mode when a navigation occurs.
  void EnterCancelFullscreenOnNavigateMode();

  // Makes the browser no longer exit fullscreen mode when a navigation occurs.
  void ExitCancelFullscreenOnNavigateMode();

  // Make the current tab exit fullscreen mode or mouse lock if it is in it.
  void ExitTabFullscreenOrMouseLockIfNecessary();
  void UpdateFullscreenExitBubbleContent();
  void NotifyFullscreenChange(bool is_fullscreen);
  void NotifyMouseLockChange();
  ContentSetting GetFullscreenSetting(const GURL& url) const;
  ContentSetting GetMouseLockSetting(const GURL& url) const;

#if defined(OS_MACOSX)
  void TogglePresentationModeInternal(bool for_tab);
#endif
  // TODO(koz): Change |for_tab| to an enum.
  void ToggleFullscreenModeInternal(bool for_tab);

  BrowserWindow* window_;
  Profile* profile_;
  Browser* browser_;

  // If there is currently a tab in fullscreen mode (entered via
  // webkitRequestFullScreen), this is its TabContents.
  TabContents* fullscreened_tab_;

  // The URL of the extension which trigerred "browser fullscreen" mode.
  GURL extension_caused_fullscreen_;

  // True if the current tab entered fullscreen mode via webkitRequestFullScreen
  bool tab_caused_fullscreen_;
  // True if tab fullscreen has been allowed, either by settings or by user
  // clicking the allow button on the fullscreen infobar.
  bool tab_fullscreen_accepted_;

  // True if this controller has toggled into tab OR browser fullscreen.
  bool toggled_into_fullscreen_;

  // TabContents for current tab requesting or currently in mouse lock.
  TabContents* mouse_lock_tab_;

  MouseLockState mouse_lock_state_;

  content::NotificationRegistrar registrar_;

  // If this is true then we are listening for navigation events and will
  // cancel fullscreen when one occurs.
  bool cancel_fullscreen_on_navigate_mode_;

  DISALLOW_COPY_AND_ASSIGN(FullscreenController);
};

#endif  // CHROME_BROWSER_UI_FULLSCREEN_CONTROLLER_H_
