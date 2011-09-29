// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_BROWSER_WINDOW_COCOA_H_
#define CHROME_BROWSER_UI_COCOA_BROWSER_WINDOW_COCOA_H_
#pragma once

#include "base/memory/scoped_nsobject.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/ui/browser_window.h"
#include "content/common/notification_registrar.h"
#include "ui/base/ui_base_types.h"

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
                     BrowserWindowController* controller);
  virtual ~BrowserWindowCocoa();

  // Overridden from BrowserWindow
  virtual void Show();
  virtual void ShowInactive();
  virtual void SetBounds(const gfx::Rect& bounds);
  virtual void Close();
  virtual void Activate();
  virtual void Deactivate();
  virtual bool IsActive() const;
  virtual void FlashFrame();
  virtual gfx::NativeWindow GetNativeHandle();
  virtual BrowserWindowTesting* GetBrowserWindowTesting();
  virtual StatusBubble* GetStatusBubble();
  virtual void ToolbarSizeChanged(bool is_animating);
  virtual void UpdateTitleBar();
  virtual void BookmarkBarStateChanged(
      BookmarkBar::AnimateChangeType change_type);
  virtual void UpdateDevTools();
  virtual void UpdateLoadingAnimations(bool should_animate);
  virtual void SetStarredState(bool is_starred);
  virtual gfx::Rect GetRestoredBounds() const;
  virtual gfx::Rect GetBounds() const;
  virtual bool IsMaximized() const;
  virtual bool IsMinimized() const;
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
  virtual void ShowAboutChromeDialog();
  virtual void ShowUpdateChromeDialog();
  virtual void ShowTaskManager();
  virtual void ShowBackgroundPages();
  virtual void ShowBookmarkBubble(const GURL& url, bool already_bookmarked);
  virtual bool IsDownloadShelfVisible() const;
  virtual DownloadShelf* GetDownloadShelf();
  virtual void ShowRepostFormWarningDialog(TabContents* tab_contents);
  virtual void ShowCollectedCookiesDialog(TabContentsWrapper* wrapper);
  virtual void ShowThemeInstallBubble();
  virtual void ConfirmBrowserCloseWithPendingDownloads();
  virtual gfx::NativeWindow ShowHTMLDialog(HtmlDialogUIDelegate* delegate,
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
  virtual void ShowCreateWebAppShortcutsDialog(
      TabContentsWrapper* tab_contents);
  virtual void ShowCreateChromeAppShortcutsDialog(Profile* profile,
                                                  const Extension* app);
  virtual void Cut();
  virtual void Copy();
  virtual void Paste();
  virtual void ToggleTabStripMode();
  virtual void OpenTabpose();
  virtual void SetPresentationMode(bool presentation_mode);
  virtual bool InPresentationMode();
  virtual void PrepareForInstant();
  virtual void ShowInstant(TabContentsWrapper* preview);
  virtual void HideInstant(bool instant_is_active);
  virtual gfx::Rect GetInstantBounds();
  virtual WindowOpenDisposition GetDispositionForPopupBounds(
      const gfx::Rect& bounds);
  virtual FindBar* CreateFindBar() OVERRIDE;

  // Overridden from NotificationObserver
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Adds the given FindBar cocoa controller to this browser window.
  void AddFindBar(FindBarCocoaController* find_bar_cocoa_controller);

  // Returns the cocoa-world BrowserWindowController
  BrowserWindowController* cocoa_controller() { return controller_; }

 protected:
  virtual void DestroyBrowser();

 private:
  NSWindow* window() const;  // Accessor for the (current) |NSWindow|.
  void UpdateSidebarForContents(TabContents* tab_contents);

  NotificationRegistrar registrar_;
  Browser* browser_;  // weak, owned by controller
  BrowserWindowController* controller_;  // weak, owns us
  base::WeakPtrFactory<Browser> confirm_close_factory_;
  scoped_nsobject<NSString> pending_window_title_;
  ui::WindowShowState initial_show_state_;
};

#endif  // CHROME_BROWSER_UI_COCOA_BROWSER_WINDOW_COCOA_H_
