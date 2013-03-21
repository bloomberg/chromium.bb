// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_TEST_BROWSER_WINDOW_H_
#define CHROME_TEST_BASE_TEST_BROWSER_WINDOW_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "build/build_config.h"
#include "chrome/browser/download/test_download_shelf.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/test_location_bar.h"

namespace extensions {
class Extension;
}

// An implementation of BrowserWindow used for testing. TestBrowserWindow only
// contains a valid LocationBar, all other getters return NULL.
// See BrowserWithTestWindowTest for an example of using this class.
class TestBrowserWindow : public BrowserWindow {
 public:
  TestBrowserWindow();
  virtual ~TestBrowserWindow();

  // BrowserWindow:
  virtual void Show() OVERRIDE {}
  virtual void ShowInactive() OVERRIDE {}
  virtual void Hide() OVERRIDE {}
  virtual void SetBounds(const gfx::Rect& bounds) OVERRIDE {}
  virtual void Close() OVERRIDE {}
  virtual void Activate() OVERRIDE {}
  virtual void Deactivate() OVERRIDE {}
  virtual bool IsActive() const OVERRIDE;
  virtual void FlashFrame(bool flash) OVERRIDE {}
  virtual bool IsAlwaysOnTop() const OVERRIDE;
  virtual gfx::NativeWindow GetNativeWindow() OVERRIDE;
  virtual BrowserWindowTesting* GetBrowserWindowTesting() OVERRIDE;
  virtual StatusBubble* GetStatusBubble() OVERRIDE;
  virtual void UpdateTitleBar() OVERRIDE {}
  virtual void BookmarkBarStateChanged(
      BookmarkBar::AnimateChangeType change_type) OVERRIDE {}
  virtual void UpdateDevTools() OVERRIDE {}
  virtual void UpdateLoadingAnimations(bool should_animate) OVERRIDE {}
  virtual void SetStarredState(bool is_starred) OVERRIDE {}
  virtual void ZoomChangedForActiveTab(bool can_show_bubble) OVERRIDE {}
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
  virtual bool ShouldHideUIForFullscreen() const OVERRIDE;
  virtual bool IsFullscreen() const OVERRIDE;
#if defined(OS_WIN)
  virtual void SetMetroSnapMode(bool enable) OVERRIDE {}
  virtual bool IsInMetroSnapMode() const;
#endif
  virtual bool IsFullscreenBubbleVisible() const OVERRIDE;
  virtual LocationBar* GetLocationBar() const OVERRIDE;
  virtual void SetFocusToLocationBar(bool select_all) OVERRIDE {}
  virtual void UpdateReloadStopState(bool is_loading, bool force) OVERRIDE {}
  virtual void UpdateToolbar(content::WebContents* contents,
                             bool should_restore_state) OVERRIDE {}
  virtual void FocusToolbar() OVERRIDE {}
  virtual void FocusAppMenu() OVERRIDE {}
  virtual void FocusBookmarksToolbar() OVERRIDE {}
  virtual void RotatePaneFocus(bool forwards) OVERRIDE {}
  virtual void ShowAppMenu() OVERRIDE {}
  virtual bool PreHandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event,
      bool* is_keyboard_shortcut) OVERRIDE;
  virtual void HandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) OVERRIDE {}
  virtual void ShowCreateChromeAppShortcutsDialog(
      Profile* profile,
      const extensions::Extension* app) OVERRIDE {}

  virtual bool IsBookmarkBarVisible() const OVERRIDE;
  virtual bool IsBookmarkBarAnimating() const OVERRIDE;
  virtual bool IsTabStripEditable() const OVERRIDE;
  virtual bool IsToolbarVisible() const OVERRIDE;
  virtual gfx::Rect GetRootWindowResizerRect() const OVERRIDE;
  virtual bool IsPanel() const OVERRIDE;
  virtual void ConfirmAddSearchProvider(TemplateURL* template_url,
                                        Profile* profile) OVERRIDE {}
  virtual void ToggleBookmarkBar() OVERRIDE {}
  virtual void ShowUpdateChromeDialog() OVERRIDE {}
  virtual void ShowBookmarkBubble(const GURL& url,
                                  bool already_bookmarked) OVERRIDE {}
  virtual void ShowChromeToMobileBubble() OVERRIDE {}
#if defined(ENABLE_ONE_CLICK_SIGNIN)
  virtual void ShowOneClickSigninBubble(
      OneClickSigninBubbleType type,
      const StartSyncCallback& start_sync_callback) OVERRIDE {}
#endif
  virtual bool IsDownloadShelfVisible() const OVERRIDE;
  virtual DownloadShelf* GetDownloadShelf() OVERRIDE;
  virtual void ConfirmBrowserCloseWithPendingDownloads() OVERRIDE {}
  virtual void UserChangedTheme() OVERRIDE {}
  virtual int GetExtraRenderViewHeight() const OVERRIDE;
  virtual void WebContentsFocused(content::WebContents* contents) OVERRIDE {}
  virtual void ShowWebsiteSettings(Profile* profile,
                                   content::WebContents* web_contents,
                                   const GURL& url,
                                   const content::SSLStatus& ssl,
                                   bool show_history) OVERRIDE {}
  virtual void Cut() OVERRIDE {}
  virtual void Copy() OVERRIDE {}
  virtual void Paste() OVERRIDE {}
#if defined(OS_MACOSX)
  virtual void OpenTabpose() OVERRIDE {}
  virtual void EnterFullscreenWithChrome() OVERRIDE {}
  virtual bool IsFullscreenWithChrome() OVERRIDE;
  virtual bool IsFullscreenWithoutChrome() OVERRIDE;
#endif

  virtual gfx::Rect GetInstantBounds() OVERRIDE;
  virtual WindowOpenDisposition GetDispositionForPopupBounds(
      const gfx::Rect& bounds) OVERRIDE;
  virtual FindBar* CreateFindBar() OVERRIDE;
  virtual bool GetConstrainedWindowTopY(int* top_y) OVERRIDE;
  virtual void ShowAvatarBubble(content::WebContents* web_contents,
                                const gfx::Rect& rect) OVERRIDE {}
  virtual void ShowAvatarBubbleFromAvatarButton() OVERRIDE {}
  virtual void ShowPasswordGenerationBubble(
      const gfx::Rect& rect,
      const content::PasswordForm& form,
      autofill::PasswordGenerator* generator) OVERRIDE {}

 protected:
  virtual void DestroyBrowser() OVERRIDE {}

 private:
  TestDownloadShelf download_shelf_;
  TestLocationBar location_bar_;

  DISALLOW_COPY_AND_ASSIGN(TestBrowserWindow);
};

namespace chrome {

// Helper that handle the lifetime of TestBrowserWindow instances.
Browser* CreateBrowserWithTestWindowForParams(Browser::CreateParams* params);

}  // namespace chrome

#endif  // CHROME_TEST_BASE_TEST_BROWSER_WINDOW_H_
