// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_PANEL_H_
#define CHROME_BROWSER_UI_PANELS_PANEL_H_
#pragma once

#include "chrome/browser/ui/browser_window.h"

#include "base/memory/scoped_ptr.h"
#include "ui/gfx/rect.h"

class PanelManager;

// A platform independent implementation of BrowserWindow for Panels.  This
// class would get the first crack at all the BrowserWindow calls for Panels and
// do one or more of the following:
// - Do nothing.  The function is not relevant to Panels.
// - Throw an exceptions.  The function shouldn't be called for Panels.
// - Do Panel specific platform independent processing and then invoke the
//   function on the platform specific BrowserWindow member.  For example,
//   Panel size is restricted to certain limits.
// - Invoke an appropriate PanelManager function to do stuff that might affect
//   other Panels.  For example deleting a panel would rearrange other panels.
class Panel : public BrowserWindow {
 public:
  virtual ~Panel();

  // Returns the PanelManager associated with this panel.
  PanelManager* manager() const;

  void Minimize();

  void Restore();

  const gfx::Rect& bounds() const { return bounds_; }

  bool minimized() const { return minimized_; }

  // BrowserWindow overrides.
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
  virtual void ShelfVisibilityChanged();
  virtual void UpdateDevTools();
  virtual void UpdateLoadingAnimations(bool should_animate);
  virtual void SetStarredState(bool is_starred);
  virtual gfx::Rect GetRestoredBounds() const;
  virtual gfx::Rect GetBounds() const;
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
  virtual void DisableInactiveFrame();
  virtual void ConfirmSetDefaultSearchProvider(
      TabContents* tab_contents,
      TemplateURL* template_url,
      TemplateURLModel* template_url_model);
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
  virtual void ShowCollectedCookiesDialog(TabContents* tab_contents);
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
  virtual void ShowCreateWebAppShortcutsDialog(
      TabContentsWrapper* tab_contents);
  virtual void ShowCreateChromeAppShortcutsDialog(Profile* profile,
                                                  const Extension* app);
  virtual void Cut();
  virtual void Copy();
  virtual void Paste();
  virtual void ToggleTabStripMode();
#if defined(OS_MACOSX)
  virtual void OpenTabpose();
#endif
  virtual void PrepareForInstant();
  virtual void ShowInstant(TabContentsWrapper* preview);
  virtual void HideInstant(bool instant_is_active);
  virtual gfx::Rect GetInstantBounds();
#if defined(OS_CHROMEOS)
  virtual void ShowKeyboardOverlay(gfx::NativeWindow owning_window);
#endif

  // Construct a native panel BrowserWindow implementation for the specified
  // |browser|.
  static BrowserWindow* CreateNativePanel(Browser* browser, Panel* panel);

 protected:
  virtual void DestroyBrowser();

 private:
  friend class PanelManager;

  // Panel can only be created using PanelManager::CreatePanel().
  Panel(Browser* browser, const gfx::Rect& bounds);

  // Platform specifc BrowserWindow implementation for panels.  It'd be one of
  // PanelBrowserWindowGtk/PanelBrowserView/PanelBrowserWindowCocoa.
  scoped_ptr<BrowserWindow> browser_window_;

  gfx::Rect bounds_;

  // Is the panel minimized?
  bool minimized_;

  DISALLOW_COPY_AND_ASSIGN(Panel);
};

#endif  // CHROME_BROWSER_UI_PANELS_PANEL_H_
