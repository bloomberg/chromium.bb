// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_TEST_BROWSER_WINDOW_H_
#define CHROME_TEST_BASE_TEST_BROWSER_WINDOW_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/test_location_bar.h"

// An implementation of BrowserWindow used for testing. TestBrowserWindow only
// contains a valid LocationBar, all other getters return NULL.
// See BrowserWithTestWindowTest for an example of using this class.
class TestBrowserWindow : public BrowserWindow {
 public:
  explicit TestBrowserWindow(Browser* browser);
  virtual ~TestBrowserWindow();

  // BrowserWindow:
  virtual void Show() OVERRIDE {}
  virtual void ShowInactive() OVERRIDE {}
  virtual void SetBounds(const gfx::Rect& bounds) OVERRIDE {}
  virtual void Close() OVERRIDE {}
  virtual void Activate() OVERRIDE {}
  virtual void Deactivate() OVERRIDE {}
  virtual bool IsActive() const OVERRIDE;
  virtual void FlashFrame() OVERRIDE {}
  virtual gfx::NativeWindow GetNativeHandle() OVERRIDE;
  virtual BrowserWindowTesting* GetBrowserWindowTesting() OVERRIDE;
  virtual StatusBubble* GetStatusBubble() OVERRIDE;
  virtual void ToolbarSizeChanged(bool is_animating) OVERRIDE {}
  virtual void UpdateTitleBar() OVERRIDE {}
  virtual void BookmarkBarStateChanged(
      BookmarkBar::AnimateChangeType change_type) OVERRIDE {}
  virtual void UpdateDevTools() OVERRIDE {}
  virtual void UpdateLoadingAnimations(bool should_animate) OVERRIDE {}
  virtual void SetStarredState(bool is_starred) OVERRIDE {}
  virtual gfx::Rect GetRestoredBounds() const OVERRIDE;
  virtual gfx::Rect GetBounds() const OVERRIDE;
  virtual bool IsMaximized() const OVERRIDE;
  virtual bool IsMinimized() const OVERRIDE;
  virtual void Maximize() OVERRIDE {}
  virtual void Minimize() OVERRIDE {}
  virtual void Restore() OVERRIDE {}
  virtual void EnterFullscreen(
      const GURL& url, FullscreenExitBubbleType type) OVERRIDE {}
  virtual void ExitFullscreen() OVERRIDE {}
  virtual void UpdateFullscreenExitBubbleContent(
      const GURL& url,
      FullscreenExitBubbleType bubble_type) OVERRIDE {}
  virtual bool IsFullscreen() const OVERRIDE;
  virtual bool IsFullscreenBubbleVisible() const OVERRIDE;
  virtual LocationBar* GetLocationBar() const OVERRIDE;
  virtual void SetFocusToLocationBar(bool select_all) OVERRIDE {}
  virtual void UpdateReloadStopState(bool is_loading, bool force) OVERRIDE {}
  virtual void UpdateToolbar(TabContentsWrapper* contents,
                             bool should_restore_state) OVERRIDE {}
  virtual void FocusToolbar() OVERRIDE {}
  virtual void FocusAppMenu() OVERRIDE {}
  virtual void FocusBookmarksToolbar() OVERRIDE {}
  virtual void FocusChromeOSStatus() OVERRIDE {}
  virtual void RotatePaneFocus(bool forwards) OVERRIDE {}
  virtual void ShowAppMenu() OVERRIDE {}
  virtual bool PreHandleKeyboardEvent(const NativeWebKeyboardEvent& event,
                                      bool* is_keyboard_shortcut) OVERRIDE;
  virtual void HandleKeyboardEvent(
      const NativeWebKeyboardEvent& event) OVERRIDE {}
  virtual void ShowCreateWebAppShortcutsDialog(
      TabContentsWrapper* tab_contents) OVERRIDE {}
  virtual void ShowCreateChromeAppShortcutsDialog(
      Profile* profile,
      const Extension* app) OVERRIDE {}

  virtual bool IsBookmarkBarVisible() const OVERRIDE;
  virtual bool IsBookmarkBarAnimating() const OVERRIDE;
  virtual bool IsTabStripEditable() const OVERRIDE;
  virtual bool IsToolbarVisible() const OVERRIDE;
  virtual void ConfirmAddSearchProvider(const TemplateURL* template_url,
                                        Profile* profile) OVERRIDE {}
  virtual void ToggleBookmarkBar() OVERRIDE {}
  virtual void ShowAboutChromeDialog() OVERRIDE;
  virtual void ShowUpdateChromeDialog() OVERRIDE {}
  virtual void ShowTaskManager() OVERRIDE {}
  virtual void ShowBackgroundPages() OVERRIDE {}
  virtual void ShowBookmarkBubble(const GURL& url,
                                  bool already_bookmarked) OVERRIDE {}
  virtual bool IsDownloadShelfVisible() const OVERRIDE;
  virtual DownloadShelf* GetDownloadShelf() OVERRIDE;
  virtual void ShowRepostFormWarningDialog(
      TabContents* tab_contents) OVERRIDE {}
  virtual void ShowCollectedCookiesDialog(
      TabContentsWrapper* wrapper) OVERRIDE {}
  virtual void ConfirmBrowserCloseWithPendingDownloads() OVERRIDE {}
  virtual void UserChangedTheme() OVERRIDE {}
  virtual int GetExtraRenderViewHeight() const OVERRIDE;
  virtual void TabContentsFocused(TabContents* tab_contents) OVERRIDE {}
  virtual void ShowPageInfo(Profile* profile,
                            const GURL& url,
                            const NavigationEntry::SSLStatus& ssl,
                            bool show_history) OVERRIDE {}
  virtual void Cut() OVERRIDE {}
  virtual void Copy() OVERRIDE {}
  virtual void Paste() OVERRIDE {}
#if defined(OS_MACOSX)
  virtual void OpenTabpose() OVERRIDE {}
  virtual void EnterPresentationMode(
      const GURL& url,
      FullscreenExitBubbleType bubble_type) OVERRIDE {}
  virtual void ExitPresentationMode() OVERRIDE {}
  virtual bool InPresentationMode() OVERRIDE;
#endif

  virtual void ShowInstant(TabContentsWrapper* preview_contents) OVERRIDE {}
  virtual void HideInstant() OVERRIDE {}
  virtual gfx::Rect GetInstantBounds() OVERRIDE;
  virtual WindowOpenDisposition GetDispositionForPopupBounds(
      const gfx::Rect& bounds) OVERRIDE;
  virtual FindBar* CreateFindBar() OVERRIDE;
  virtual void ShowAvatarBubble(TabContents* tab_contents,
                                const gfx::Rect& rect) OVERRIDE {}
  virtual void ShowAvatarBubbleFromAvatarButton() OVERRIDE {}

#if defined(OS_CHROMEOS)
  virtual void ShowMobileSetup() OVERRIDE {}
  virtual void ShowKeyboardOverlay(gfx::NativeWindow owning_window) OVERRIDE {}
#endif

 protected:
  virtual void DestroyBrowser() OVERRIDE {}

 private:
  TestLocationBar location_bar_;

  DISALLOW_COPY_AND_ASSIGN(TestBrowserWindow);
};

#endif  // CHROME_TEST_BASE_TEST_BROWSER_WINDOW_H_
