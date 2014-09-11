// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_CONTROLLER_H_
#define CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_CONTROLLER_H_

#include <set>

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/fullscreen/fullscreen_exit_bubble_type.h"
#include "components/content_settings/core/common/content_settings.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Browser;
class BrowserWindow;
class GURL;
class Profile;

namespace content {
class WebContents;
}

// There are two different kinds of fullscreen mode - "tab fullscreen" and
// "browser fullscreen". "Tab fullscreen" refers to a renderer-initiated
// fullscreen mode (eg: from a Flash plugin or via the JS fullscreen API),
// whereas "browser fullscreen" refers to the user putting the browser itself
// into fullscreen mode from the UI. The difference is that tab fullscreen has
// implications for how the contents of the tab render (eg: a video element may
// grow to consume the whole tab), whereas browser fullscreen mode doesn't.
// Therefore if a user forces an exit from tab fullscreen, we need to notify the
// tab so it can stop rendering in its fullscreen mode.
//
// For Flash, FullscreenController will auto-accept all permission requests for
// fullscreen and/or mouse lock, since the assumption is that the plugin handles
// this for us.
//
// FullscreenWithinTab Note:
// All fullscreen widgets are displayed within the tab contents area, and
// FullscreenController will expand the browser window so that the tab contents
// area fills the entire screen. However, special behavior applies when a tab is
// being screen-captured. First, the browser window will not be
// fullscreened. This allows the user to retain control of their desktop to work
// in other browser tabs or applications while the fullscreen view is displayed
// on a remote screen. Second, FullscreenController will auto-resize fullscreen
// widgets to that of the capture video resolution when they are hidden (e.g.,
// when a user has switched to another tab). This is both a performance and
// quality improvement since scaling and letterboxing steps can be skipped in
// the capture pipeline.

// This class implements fullscreen and mouselock behaviour.
class FullscreenController : public content::NotificationObserver {
 public:
  explicit FullscreenController(Browser* browser);
  virtual ~FullscreenController();

  // Browser/User Fullscreen ///////////////////////////////////////////////////

  // Returns true if the window is currently fullscreen and was initially
  // transitioned to fullscreen by a browser (i.e., not tab-initiated) mode
  // transition.
  bool IsFullscreenForBrowser() const;

  void ToggleBrowserFullscreenMode();

  // Extension API implementation uses this method to toggle fullscreen mode.
  // The extension's name is displayed in the full screen bubble UI to attribute
  // the cause of the full screen state change.
  void ToggleBrowserFullscreenModeWithExtension(const GURL& extension_url);

  // Tab/HTML/Flash Fullscreen /////////////////////////////////////////////////

  // Returns true if the browser window has/will fullscreen because of
  // tab-initiated fullscreen. The window may still be transitioning, and
  // BrowserWindow::IsFullscreen() may still return false.
  bool IsWindowFullscreenForTabOrPending() const;

  // Returns true if the tab is/will be in fullscreen mode. Note: This does NOT
  // indicate whether the browser window is/will be fullscreened as well. See
  // 'FullscreenWithinTab Note'.
  bool IsFullscreenForTabOrPending(
      const content::WebContents* web_contents) const;

  // True if fullscreen was entered because of tab fullscreen (was not
  // previously in user-initiated fullscreen).
  bool IsFullscreenCausedByTab() const;

  // Enter or leave tab-initiated fullscreen mode. FullscreenController will
  // decide whether to also fullscreen the browser window. See
  // 'FullscreenWithinTab Note'.
  void ToggleFullscreenModeForTab(content::WebContents* web_contents,
                                  bool enter_fullscreen);

  // Platform Fullscreen ///////////////////////////////////////////////////////

  // Returns whether we are currently in a Metro snap view.
  bool IsInMetroSnapMode();

#if defined(OS_WIN)
  // API that puts the window into a mode suitable for rendering when Chrome
  // is rendered in a 20% screen-width Metro snap view on Windows 8.
  void SetMetroSnapMode(bool enable);
#endif

#if defined(OS_MACOSX)
  void ToggleBrowserFullscreenWithChrome();
#endif

  // Mouse Lock ////////////////////////////////////////////////////////////////

  bool IsMouseLockRequested() const;
  bool IsMouseLocked() const;

  void RequestToLockMouse(content::WebContents* web_contents,
                          bool user_gesture,
                          bool last_unlocked_by_target);

  // Callbacks /////////////////////////////////////////////////////////////////

  // Called by Browser::TabDeactivated.
  void OnTabDeactivated(content::WebContents* web_contents);

  // Called by Browser::ActiveTabChanged.
  void OnTabDetachedFromView(content::WebContents* web_contents);

  // Called by Browser::TabClosingAt.
  void OnTabClosing(content::WebContents* web_contents);

  // Called by Browser::WindowFullscreenStateChanged.
  void WindowFullscreenStateChanged();

  // Called by Browser::PreHandleKeyboardEvent.
  bool HandleUserPressedEscape();

  // Called by platform FullscreenExitBubble.
  void ExitTabOrBrowserFullscreenToPreviousState();
  void OnAcceptFullscreenPermission();
  void OnDenyFullscreenPermission();

  // Called by Browser::LostMouseLock.
  void LostMouseLock();

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Bubble Content ////////////////////////////////////////////////////////////

  GURL GetFullscreenExitBubbleURL() const;
  FullscreenExitBubbleType GetFullscreenExitBubbleType() const;

 private:
  friend class FullscreenControllerTest;

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

  enum FullscreenInternalOption {
    BROWSER,
#if defined(OS_MACOSX)
    BROWSER_WITH_CHROME,
#endif
    TAB
  };

  void UpdateNotificationRegistrations();

  // Posts a task to call NotifyFullscreenChange.
  void PostFullscreenChangeNotification(bool is_fullscreen);
  // Sends a NOTIFICATION_FULLSCREEN_CHANGED notification.
  void NotifyFullscreenChange(bool is_fullscreen);
  // Notifies the tab that it has been forced out of fullscreen and mouse lock
  // mode if necessary.
  void NotifyTabOfExitIfNecessary();
  void NotifyMouseLockChange();

  void ToggleFullscreenModeInternal(FullscreenInternalOption option);
  void EnterFullscreenModeInternal(FullscreenInternalOption option);
  void ExitFullscreenModeInternal();
  void SetFullscreenedTab(content::WebContents* tab);
  void SetMouseLockTab(content::WebContents* tab);

  // Make the current tab exit fullscreen mode or mouse lock if it is in it.
  void ExitTabFullscreenOrMouseLockIfNecessary();
  void UpdateFullscreenExitBubbleContent();

  ContentSetting GetFullscreenSetting(const GURL& url) const;
  ContentSetting GetMouseLockSetting(const GURL& url) const;

  bool IsPrivilegedFullscreenForTab() const;
  void SetPrivilegedFullscreenForTesting(bool is_privileged);
  // Returns true if |web_contents| was toggled into/out of fullscreen mode as a
  // screen-captured tab. See 'FullscreenWithinTab Note'.
  bool MaybeToggleFullscreenForCapturedTab(content::WebContents* web_contents,
                                           bool enter_fullscreen);
  // Returns true if |web_contents| is in fullscreen mode as a screen-captured
  // tab. See 'FullscreenWithinTab Note'.
  bool IsFullscreenForCapturedTab(const content::WebContents* web_contents)
      const;
  void UnlockMouse();

  Browser* const browser_;
  BrowserWindow* const window_;
  Profile* const profile_;

  // If there is currently a tab in fullscreen mode (entered via
  // webkitRequestFullScreen), this is its WebContents.
  // Assign using SetFullscreenedTab().
  content::WebContents* fullscreened_tab_;

  // The URL of the extension which trigerred "browser fullscreen" mode.
  GURL extension_caused_fullscreen_;

  enum PriorFullscreenState {
    STATE_INVALID,
    STATE_NORMAL,
    STATE_BROWSER_FULLSCREEN_NO_CHROME,
#if defined(OS_MACOSX)
    STATE_BROWSER_FULLSCREEN_WITH_CHROME,
#endif
  };
  // The state before entering tab fullscreen mode via webkitRequestFullScreen.
  // When not in tab fullscreen, it is STATE_INVALID.
  PriorFullscreenState state_prior_to_tab_fullscreen_;
  // True if tab fullscreen has been allowed, either by settings or by user
  // clicking the allow button on the fullscreen infobar.
  bool tab_fullscreen_accepted_;

  // True if this controller has toggled into tab OR browser fullscreen.
  bool toggled_into_fullscreen_;

  // WebContents for current tab requesting or currently in mouse lock.
  // Assign using SetMouseLockTab().
  content::WebContents* mouse_lock_tab_;

  MouseLockState mouse_lock_state_;

  content::NotificationRegistrar registrar_;

  // Used to verify that calls we expect to reenter by calling
  // WindowFullscreenStateChanged do so.
  bool reentrant_window_state_change_call_check_;

  // Used in testing to confirm proper behavior for specific, privileged
  // fullscreen cases.
  bool is_privileged_fullscreen_for_testing_;

  base::WeakPtrFactory<FullscreenController> ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FullscreenController);
};

#endif  // CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_CONTROLLER_H_
