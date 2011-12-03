// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_PANEL_H_
#define CHROME_BROWSER_UI_PANELS_PANEL_H_
#pragma once

#include "chrome/browser/ui/browser_window.h"

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/tabs/tab_strip_model_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/gfx/rect.h"

class NativePanel;
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
class Panel : public BrowserWindow,
              public TabStripModelObserver,
              public content::NotificationObserver {
 public:
  enum ExpansionState {
   // The panel is fully expanded with both title-bar and the client-area.
   EXPANDED,
   // The panel is shown with the title-bar only.
   TITLE_ONLY,
   // The panel is shown with 3-pixel line.
   MINIMIZED,
   // The panel is put into the overflow area due to no space available in the
   // normal display area.
   IN_OVERFLOW
  };

  // The panel can be minimized to 4-pixel lines.
  static const int kMinimizedPanelHeight = 4;

  virtual ~Panel();

  // Returns the PanelManager associated with this panel.
  PanelManager* manager() const;

  // Gets the extension that a panel is created from.
  // Returns NULL if it cannot be found.
  const Extension* GetExtension() const;

  void SetExpansionState(ExpansionState new_expansion_state);

  bool ShouldBringUpTitlebar(int mouse_x, int mouse_y) const;

  bool IsDrawingAttention() const;

  // This function will only get called by PanelManager when full screen mode
  // changes i.e it gets called when an app goes into full screen mode or when
  // an app exits full screen mode. Panel should respond by making sure
  // a) it does not go on top when some app enters full screen mode.
  // b) it remains on top when an app exits full screen mode.
  void FullScreenModeChanged(bool is_full_screen);

  void MoveOutOfOverflow();

  // Ensures that the panel is fully visible, that is, not obscured by other
  // top-most windows.
  void EnsureFullyVisible();

  int TitleOnlyHeight() const;

  // Returns the size of the panel when it is iconified, as shown on the
  // overflow area.
  gfx::Size IconOnlySize() const;

  // BrowserWindow overrides.
  virtual void Show() OVERRIDE;
  virtual void ShowInactive() OVERRIDE;
  virtual void SetBounds(const gfx::Rect& bounds) OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual void Activate() OVERRIDE;
  virtual void Deactivate() OVERRIDE;
  virtual bool IsActive() const OVERRIDE;
  virtual void FlashFrame() OVERRIDE;
  virtual gfx::NativeWindow GetNativeHandle() OVERRIDE;
  virtual BrowserWindowTesting* GetBrowserWindowTesting() OVERRIDE;
  virtual StatusBubble* GetStatusBubble() OVERRIDE;
  virtual void ToolbarSizeChanged(bool is_animating) OVERRIDE;
  virtual void UpdateTitleBar() OVERRIDE;
  virtual void BookmarkBarStateChanged(
      BookmarkBar::AnimateChangeType change_type) OVERRIDE;
  virtual void UpdateDevTools() OVERRIDE;
  virtual void UpdateLoadingAnimations(bool should_animate) OVERRIDE;
  virtual void SetStarredState(bool is_starred) OVERRIDE;
  virtual gfx::Rect GetRestoredBounds() const OVERRIDE;
  virtual gfx::Rect GetBounds() const OVERRIDE;
  virtual bool IsMaximized() const OVERRIDE;
  virtual bool IsMinimized() const OVERRIDE;
  virtual void Maximize() OVERRIDE;
  virtual void Minimize() OVERRIDE;
  virtual void Restore() OVERRIDE;
  virtual void EnterFullscreen(
      const GURL& url, FullscreenExitBubbleType type) OVERRIDE;
  virtual void ExitFullscreen() OVERRIDE;
  virtual void UpdateFullscreenExitBubbleContent(
      const GURL& url,
      FullscreenExitBubbleType bubble_type) OVERRIDE;
  virtual bool IsFullscreen() const OVERRIDE;
  virtual bool IsFullscreenBubbleVisible() const OVERRIDE;
  virtual LocationBar* GetLocationBar() const OVERRIDE;
  virtual void SetFocusToLocationBar(bool select_all) OVERRIDE;
  virtual void UpdateReloadStopState(bool is_loading, bool force) OVERRIDE;
  virtual void UpdateToolbar(TabContentsWrapper* contents,
                             bool should_restore_state) OVERRIDE;
  virtual void FocusToolbar() OVERRIDE;
  virtual void FocusAppMenu() OVERRIDE;
  virtual void FocusBookmarksToolbar() OVERRIDE;
  virtual void FocusChromeOSStatus() OVERRIDE;
  virtual void RotatePaneFocus(bool forwards) OVERRIDE;
  virtual bool IsBookmarkBarVisible() const OVERRIDE;
  virtual bool IsBookmarkBarAnimating() const OVERRIDE;
  virtual bool IsTabStripEditable() const OVERRIDE;
  virtual bool IsToolbarVisible() const OVERRIDE;
  virtual void DisableInactiveFrame() OVERRIDE;
  virtual void ConfirmSetDefaultSearchProvider(TabContents* tab_contents,
                                               TemplateURL* template_url,
                                               Profile* profile) OVERRIDE;
  virtual void ConfirmAddSearchProvider(const TemplateURL* template_url,
                                        Profile* profile) OVERRIDE;
  virtual void ToggleBookmarkBar() OVERRIDE;
  virtual void ShowAboutChromeDialog() OVERRIDE;
  virtual void ShowUpdateChromeDialog() OVERRIDE;
  virtual void ShowTaskManager() OVERRIDE;
  virtual void ShowBackgroundPages() OVERRIDE;
  virtual void ShowBookmarkBubble(
      const GURL& url, bool already_bookmarked) OVERRIDE;
  virtual bool IsDownloadShelfVisible() const OVERRIDE;
  virtual DownloadShelf* GetDownloadShelf() OVERRIDE;
  virtual void ShowRepostFormWarningDialog(TabContents* tab_contents) OVERRIDE;
  virtual void ShowCollectedCookiesDialog(TabContentsWrapper* wrapper) OVERRIDE;
  virtual void ConfirmBrowserCloseWithPendingDownloads() OVERRIDE;
  virtual void UserChangedTheme() OVERRIDE;
  virtual int GetExtraRenderViewHeight() const OVERRIDE;
  virtual void TabContentsFocused(TabContents* tab_contents) OVERRIDE;
  virtual void ShowPageInfo(Profile* profile,
                            const GURL& url,
                            const NavigationEntry::SSLStatus& ssl,
                            bool show_history) OVERRIDE;
  virtual void ShowAppMenu() OVERRIDE;
  virtual bool PreHandleKeyboardEvent(
      const NativeWebKeyboardEvent& event,
      bool* is_keyboard_shortcut) OVERRIDE;
  virtual void HandleKeyboardEvent(
      const NativeWebKeyboardEvent& event) OVERRIDE;
  virtual void ShowCreateWebAppShortcutsDialog(
      TabContentsWrapper* tab_contents) OVERRIDE;
  virtual void ShowCreateChromeAppShortcutsDialog(
      Profile* profile, const Extension* app) OVERRIDE;
  virtual void Cut() OVERRIDE;
  virtual void Copy() OVERRIDE;
  virtual void Paste() OVERRIDE;
#if defined(OS_MACOSX)
  virtual void OpenTabpose() OVERRIDE;
  virtual void EnterPresentationMode(
      const GURL& url,
      FullscreenExitBubbleType bubble_type) OVERRIDE;
  virtual void ExitPresentationMode() OVERRIDE;
  virtual bool InPresentationMode() OVERRIDE;
#endif
  virtual void ShowInstant(TabContentsWrapper* preview) OVERRIDE;
  virtual void HideInstant() OVERRIDE;
  virtual gfx::Rect GetInstantBounds() OVERRIDE;
  virtual WindowOpenDisposition GetDispositionForPopupBounds(
      const gfx::Rect& bounds) OVERRIDE;
  virtual FindBar* CreateFindBar() OVERRIDE;
#if defined(OS_CHROMEOS)
  virtual void ShowMobileSetup() OVERRIDE;
  virtual void ShowKeyboardOverlay(gfx::NativeWindow owning_window) OVERRIDE;
#endif
  virtual void UpdatePreferredSize(TabContents* tab_contents,
                                   const gfx::Size& pref_size) OVERRIDE;
  virtual void ShowAvatarBubble(TabContents* tab_contents,
                                const gfx::Rect& rect) OVERRIDE;
  virtual void ShowAvatarBubbleFromAvatarButton() OVERRIDE;

  // TabStripModelObserver overrides.
  virtual void TabInsertedAt(TabContentsWrapper* contents,
                             int index,
                             bool foreground) OVERRIDE;

  // content::NotificationObserver overrides.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Construct a native panel BrowserWindow implementation for the specified
  // |browser|.
  static NativePanel* CreateNativePanel(Browser* browser,
                                        Panel* panel,
                                        const gfx::Rect& bounds);

  // Gets the extension from the browser that a panel is created from.
  // Returns NULL if it cannot be found.
  static const Extension* GetExtensionFromBrowser(Browser* browser);

  // Used on platforms where the panel cannot determine its window size
  // until the window has been created. (e.g. GTK)
  void OnWindowSizeAvailable();

  // Asynchronous completion of panel close request.
  void OnNativePanelClosed();

  NativePanel* native_panel() { return native_panel_; }
  Browser* browser() const { return browser_; }
  ExpansionState expansion_state() const { return expansion_state_; }
  const gfx::Size& min_size() const { return min_size_; }
  const gfx::Size& max_size() const { return max_size_; }
  bool auto_resizable() const { return auto_resizable_; }
  // The restored size is the size of the panel when it is expanded.
  gfx::Size restored_size() const { return restored_size_; }
  void set_restored_size(const gfx::Size& size) { restored_size_ = size; }

  // Panel must be initialized to be "fully created" and ready for use.
  // Only called by PanelManager.
  bool initialized() const { return initialized_; }
  void Initialize(const gfx::Rect& bounds);

  // This is different from BrowserWindow::SetBounds():
  // * SetPanelBounds() is only called by PanelManager to manage its position.
  // * SetBounds() is called by the API to try to change the bounds, which is
  //   not allowed for Panel.
  void SetPanelBounds(const gfx::Rect& bounds);

  // Updates the panel bounds instantly without any animation.
  void SetPanelBoundsInstantly(const gfx::Rect& bounds);

  // Sets whether the panel will auto resize according to its content.
  void SetAutoResizable(bool resizable);

  // Sets minimum and maximum size for the panel.
  void SetSizeRange(const gfx::Size& min_size, const gfx::Size& max_size);

 protected:
  virtual void DestroyBrowser() OVERRIDE;

 private:
  friend class PanelManager;
  friend class PanelBrowserTest;
  FRIEND_TEST_ALL_PREFIXES(PanelBrowserTest, RestoredBounds);

  // Panel can only be created using PanelManager::CreatePanel().
  // |requested_size| is the desired size for the panel, but actual
  // size may differ after panel layout.
  Panel(Browser* browser, const gfx::Size& requested_size);

  // Configures the tab contents for auto resize, including configurations
  // on the renderer and detecting renderer changes.
  void EnableTabContentsAutoResize(TabContents* tab_contents);

  // Configures the renderer for auto resize (if auto resize is enabled).
  void ConfigureAutoResize(TabContents* tab_contents);

  Browser* browser_;  // Weak, owned by native panel.

  bool initialized_;

  // Stores the full size of the panel so we can restore it after it's
  // been minimized.
  gfx::Size restored_size_;

  // This is the minimum size that the panel can shrink to.
  gfx::Size min_size_;

  // This is the size beyond which the panel is not going to grow to accomodate
  // the growing content and WebKit would add the scrollbars in such case.
  gfx::Size max_size_;

  // True if this panel auto resizes based on content.
  bool auto_resizable_;

  // Platform specifc implementation for panels.  It'd be one of
  // PanelBrowserWindowGtk/PanelBrowserView/PanelBrowserWindowCocoa.
  NativePanel* native_panel_;  // Weak, owns us.

  ExpansionState expansion_state_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(Panel);
};

#endif  // CHROME_BROWSER_UI_PANELS_PANEL_H_
