// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_BROWSER_WINDOW_COCOA_H_
#define CHROME_BROWSER_UI_COCOA_BROWSER_WINDOW_COCOA_H_
#pragma once

#include "base/scoped_nsobject.h"
#include "base/task.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/common/notification_registrar.h"

class Browser;
@class BrowserWindowController;
@class FindBarCocoaController;
@class NSEvent;
@class NSMenu;
@class NSWindow;

// An implementation of BrowserWindow for Cocoa. Bridges between C++ and
// the Cocoa NSWindow. Cross-platform code will interact with this object when
// it needs to manipulate the window.

class BrowserWindowCocoa : public BrowserWindow,
                           public NotificationObserver {
 public:
  BrowserWindowCocoa(Browser* browser,
                     BrowserWindowController* controller,
                     NSWindow* window);
  virtual ~BrowserWindowCocoa();

  // Overridden from BrowserWindow
  virtual void Show();
  virtual void SetBounds(const gfx::Rect& bounds);
  virtual void Close();
  virtual void Activate();
  virtual void Deactivate();
  virtual bool IsActive() const;
  virtual void FlashFrame();
  virtual gfx::NativeWindow GetNativeHandle();
  virtual BrowserWindowTesting* GetBrowserWindowTesting();
  virtual StatusBubble* GetStatusBubble();
  virtual void SelectedTabToolbarSizeChanged(bool is_animating);
  virtual void UpdateTitleBar();
  virtual void ShelfVisibilityChanged();
  virtual void UpdateDevTools();
  virtual void UpdateLoadingAnimations(bool should_animate);
  virtual void SetStarredState(bool is_starred);
  virtual gfx::Rect GetRestoredBounds() const;
  virtual bool IsMaximized() const;
  virtual void SetFullscreen(bool fullscreen);
  virtual bool IsFullscreen() const;
  virtual bool IsFullscreenBubbleVisible() const;
  virtual LocationBar* GetLocationBar() const;
  virtual void SetFocusToLocationBar(bool select_all);
  virtual void UpdateReloadStopState(bool is_loading, bool force);
  virtual void UpdateToolbar(TabContentsWrapper* contents,
                             bool should_restore_state);
  virtual void FocusToolbar();
  virtual void FocusAppMenu();
  virtual void FocusBookmarksToolbar();
  virtual void FocusChromeOSStatus();
  virtual void RotatePaneFocus(bool forwards);
  virtual bool IsBookmarkBarVisible() const;
  virtual bool IsBookmarkBarAnimating() const;
  virtual bool IsTabStripEditable() const;
  virtual bool IsToolbarVisible() const;
  virtual void ConfirmAddSearchProvider(const TemplateURL* template_url,
                                        Profile* profile);
  virtual void ToggleBookmarkBar();
  virtual views::Window* ShowAboutChromeDialog();
  virtual void ShowUpdateChromeDialog();
  virtual void ShowTaskManager();
  virtual void ShowBookmarkBubble(const GURL& url, bool already_bookmarked);
  virtual bool IsDownloadShelfVisible() const;
  virtual DownloadShelf* GetDownloadShelf();
  virtual void ShowClearBrowsingDataDialog();
  virtual void ShowImportDialog();
  virtual void ShowSearchEnginesDialog();
  virtual void ShowPasswordManager();
  virtual void ShowRepostFormWarningDialog(TabContents* tab_contents);
  virtual void ShowContentSettingsWindow(ContentSettingsType content_type,
                                         Profile* profile);
  virtual void ShowCollectedCookiesDialog(TabContents* tab_contents);
  virtual void ShowProfileErrorDialog(int message_id);
  virtual void ShowThemeInstallBubble();
  virtual void ConfirmBrowserCloseWithPendingDownloads();
  virtual void ShowHTMLDialog(HtmlDialogUIDelegate* delegate,
                              gfx::NativeWindow parent_window);
  virtual void UserChangedTheme();
  virtual int GetExtraRenderViewHeight() const;
  virtual void TabContentsFocused(TabContents* tab_contents);
  virtual void ShowPageInfo(Profile* profile,
                            const GURL& url,
                            const NavigationEntry::SSLStatus& ssl,
                            bool show_history);
  virtual void ShowAppMenu();
  virtual bool PreHandleKeyboardEvent(const NativeWebKeyboardEvent& event,
                                      bool* is_keyboard_shortcut);
  virtual void HandleKeyboardEvent(const NativeWebKeyboardEvent& event);
  virtual void ShowCreateWebAppShortcutsDialog(TabContents* tab_contents);
  virtual void ShowCreateChromeAppShortcutsDialog(Profile* profile,
                                                  const Extension* app);
  virtual void Cut();
  virtual void Copy();
  virtual void Paste();
  virtual void ToggleTabStripMode();
  virtual void OpenTabpose();
  virtual void PrepareForInstant();
  virtual void ShowInstant(TabContents* preview_contents);
  virtual void HideInstant(bool instant_is_active);
  virtual gfx::Rect GetInstantBounds();

  // Overridden from NotificationObserver
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Adds the given FindBar cocoa controller to this browser window.
  void AddFindBar(FindBarCocoaController* find_bar_cocoa_controller);

  // Returns the cocoa-world BrowserWindowController
  BrowserWindowController* cocoa_controller() { return controller_; }

 protected:
  virtual void DestroyBrowser();

 private:
  int GetCommandId(const NativeWebKeyboardEvent& event);
  bool HandleKeyboardEventInternal(NSEvent* event);
  NSWindow* window() const;  // Accessor for the (current) |NSWindow|.
  void UpdateSidebarForContents(TabContents* tab_contents);

  NotificationRegistrar registrar_;
  Browser* browser_;  // weak, owned by controller
  BrowserWindowController* controller_;  // weak, owns us
  ScopedRunnableMethodFactory<Browser> confirm_close_factory_;
  scoped_nsobject<NSString> pending_window_title_;
};

#endif  // CHROME_BROWSER_UI_COCOA_BROWSER_WINDOW_COCOA_H_
