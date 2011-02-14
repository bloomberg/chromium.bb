// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_TEST_BROWSER_WINDOW_H_
#define CHROME_TEST_TEST_BROWSER_WINDOW_H_
#pragma once

#include "chrome/browser/browser_window.h"
#include "chrome/test/test_location_bar.h"

// An implementation of BrowserWindow used for testing. TestBrowserWindow only
// contains a valid LocationBar, all other getters return NULL.
// See BrowserWithTestWindowTest for an example of using this class.
class TestBrowserWindow : public BrowserWindow {
 public:
  explicit TestBrowserWindow(Browser* browser);
  virtual ~TestBrowserWindow();

  virtual void Init() {}
  virtual void Show() {}
  virtual void SetBounds(const gfx::Rect& bounds) {}
  virtual void Close() {}
  virtual void Activate() {}
  virtual void Deactivate() {}
  virtual bool IsActive() const;
  virtual void FlashFrame() {}
  virtual gfx::NativeWindow GetNativeHandle();
  virtual BrowserWindowTesting* GetBrowserWindowTesting();
  virtual StatusBubble* GetStatusBubble();
  virtual void SelectedTabToolbarSizeChanged(bool is_animating) {}
  virtual void UpdateTitleBar() {}
  virtual void ShelfVisibilityChanged() {}
  virtual void UpdateDevTools() {}
  virtual void UpdateLoadingAnimations(bool should_animate) {}
  virtual void SetStarredState(bool is_starred) {}
  virtual gfx::Rect GetRestoredBounds() const;
  virtual gfx::Rect GetBounds() const;
  virtual bool IsMaximized() const;
  virtual void SetFullscreen(bool fullscreen) {}
  virtual bool IsFullscreen() const;
  virtual bool IsFullscreenBubbleVisible() const;
  virtual LocationBar* GetLocationBar() const;
  virtual void SetFocusToLocationBar(bool select_all) {}
  virtual void UpdateReloadStopState(bool is_loading, bool force) {}
  virtual void UpdateToolbar(TabContentsWrapper* contents,
                             bool should_restore_state) {}
  virtual void FocusToolbar() {}
  virtual void FocusAppMenu() {}
  virtual void FocusBookmarksToolbar() {}
  virtual void FocusChromeOSStatus() {}
  virtual void RotatePaneFocus(bool forwards) {}
  virtual void ShowAppMenu() {}
  virtual bool PreHandleKeyboardEvent(const NativeWebKeyboardEvent& event,
                                      bool* is_keyboard_shortcut);
  virtual void HandleKeyboardEvent(const NativeWebKeyboardEvent& event) {}
  virtual void ShowCreateWebAppShortcutsDialog(TabContents* tab_contents) {}
  virtual void ShowCreateChromeAppShortcutsDialog(Profile* profile,
                                                  const Extension* app) {}
#if defined(TOOLKIT_VIEWS)
  virtual void ToggleCompactNavigationBar() {}
#endif  // defined(TOOLKIT_VIEWS)

  virtual bool IsBookmarkBarVisible() const;
  virtual bool IsBookmarkBarAnimating() const;
  virtual bool IsTabStripEditable() const;
  virtual bool IsToolbarVisible() const;
  virtual void ConfirmAddSearchProvider(const TemplateURL* template_url,
                                        Profile* profile) {}
  virtual void ToggleBookmarkBar() {}
  virtual views::Window* ShowAboutChromeDialog();
  virtual void ShowUpdateChromeDialog() {}
  virtual void ShowTaskManager() {}
  virtual void ShowBackgroundPages() {}
  virtual void ShowBookmarkManager() {}
  virtual void ShowBookmarkBubble(const GURL& url, bool already_bookmarked) {}
  virtual bool IsDownloadShelfVisible() const;
  virtual DownloadShelf* GetDownloadShelf();
  virtual void ShowReportBugDialog() {}
  virtual void ShowClearBrowsingDataDialog() {}
  virtual void ShowImportDialog() {}
  virtual void ShowSearchEnginesDialog() {}
  virtual void ShowPasswordManager() {}
  virtual void ShowRepostFormWarningDialog(TabContents* tab_contents) {}
  virtual void ShowContentSettingsWindow(ContentSettingsType content_type,
                                         Profile* profile) {}
  virtual void ShowCollectedCookiesDialog(TabContents* tab_contents) {}
  virtual void ShowProfileErrorDialog(int message_id) {}
  virtual void ShowThemeInstallBubble() {}
  virtual void ConfirmBrowserCloseWithPendingDownloads() {}
  virtual void ShowHTMLDialog(HtmlDialogUIDelegate* delegate,
                              gfx::NativeWindow parent_window) {}
  virtual void UserChangedTheme() {}
  virtual int GetExtraRenderViewHeight() const;
  virtual void TabContentsFocused(TabContents* tab_contents) {}
  virtual void ShowPageInfo(Profile* profile,
                            const GURL& url,
                            const NavigationEntry::SSLStatus& ssl,
                            bool show_history) {}
  virtual void Cut() {}
  virtual void Copy() {}
  virtual void Paste() {}
  virtual void ToggleTabStripMode() {}
  virtual void OpenTabpose() {}
  virtual void PrepareForInstant() {}
  virtual void ShowInstant(TabContents* preview_contents) {}
  virtual void HideInstant(bool instant_is_active) {}
  virtual gfx::Rect GetInstantBounds();

#if defined(OS_CHROMEOS)
  virtual void ShowKeyboardOverlay(gfx::NativeWindow owning_window) {}
#endif

 protected:
  virtual void DestroyBrowser() {}

 private:
  TestLocationBar location_bar_;

  DISALLOW_COPY_AND_ASSIGN(TestBrowserWindow);
};

#endif  // CHROME_TEST_TEST_BROWSER_WINDOW_H_
