// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_FULLSCREEN_CONTROLLER_H_
#define CHROME_BROWSER_UI_FULLSCREEN_CONTROLLER_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/task.h"
#include "chrome/browser/ui/fullscreen_exit_bubble_type.h"
#include "chrome/common/content_settings.h"

class Browser;
class BrowserWindow;
class GURL;
class Profile;
class TabContents;
class TabContentsWrapper;

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
class FullscreenController : public base::RefCounted<FullscreenController> {
 public:
  FullscreenController(BrowserWindow* window,
                       Profile* profile,
                       Browser* browser);
  virtual ~FullscreenController();

  // Querying.
  bool IsFullscreenForTab() const;
  bool IsFullscreenForTab(const TabContents* tab) const;

  // Requests.
  void RequestToLockMouse(TabContents* tab);
  void ToggleFullscreenModeForTab(TabContents* tab, bool enter_fullscreen);
#if defined(OS_MACOSX)
  void TogglePresentationMode(bool for_tab);
#endif
  // TODO(koz): Change |for_tab| to an enum.
  void ToggleFullscreenMode(bool for_tab);

  // Notifications.
  void LostMouseLock();
  void OnTabClosing(TabContents* tab_contents);
  void OnTabDeactivated(TabContentsWrapper* contents);
  void OnAcceptFullscreenPermission(const GURL& url,
                                    FullscreenExitBubbleType bubble_type);
  void OnDenyFullscreenPermission(FullscreenExitBubbleType bubble_type);
  void WindowFullscreenStateChanged();
  bool HandleUserPressedEscape();

  FullscreenExitBubbleType GetFullscreenExitBubbleType() const;

 private:
  enum MouseLockState {
    MOUSELOCK_NOT_REQUESTED,
    // The page requests to lock the mouse and the user hasn't responded to the
    // request.
    MOUSELOCK_REQUESTED,
    // Mouse lock has been allowed by the user.
    MOUSELOCK_ACCEPTED
  };

  // Notifies the tab that it has been forced out of fullscreen mode if
  // necessary.
  void NotifyTabOfFullscreenExitIfNecessary();
  // Make the current tab exit fullscreen mode if it is in it.
  void ExitTabbedFullscreenModeIfNecessary();
  void UpdateFullscreenExitBubbleContent();
  void NotifyFullscreenChange();
  ContentSetting GetFullscreenSetting(const GURL& url) const;
  ContentSetting GetMouseLockSetting(const GURL& url) const;

  BrowserWindow* window_;
  Profile* profile_;
  Browser* browser_;

  // If there is currently a tab in fullscreen mode (entered via
  // webkitRequestFullScreen), this is its wrapper.
  TabContentsWrapper* fullscreened_tab_;

  // True if the current tab entered fullscreen mode via webkitRequestFullScreen
  bool tab_caused_fullscreen_;
  // True if tab fullscreen has been allowed, either by settings or by user
  // clicking the allow button on the fullscreen infobar.
  bool tab_fullscreen_accepted_;

  MouseLockState mouse_lock_state_;

  DISALLOW_COPY_AND_ASSIGN(FullscreenController);
};

#endif  // CHROME_BROWSER_UI_FULLSCREEN_CONTROLLER_H_
