// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_PANEL_H_
#define CHROME_BROWSER_UI_PANELS_PANEL_H_
#pragma once

#include "chrome/browser/ui/browser_window.h"

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/panels/panel_constants.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/gfx/rect.h"

class NativePanel;
class PanelManager;
class PanelStrip;

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
   MINIMIZED
  };

  // Controls how the attention should be drawn.
  enum AttentionMode {
    // Uses the panel attention. The panel's titlebar would be painted
    // differently to attract the user's attention. This is the default mode.
    USE_PANEL_ATTENTION = 0x01,
    // Uses the system attention. On Windows or Linux (depending on Window
    // Manager), the app icon on taskbar will be flashed. On MacOS, the dock
    // icon will jump once.
    USE_SYSTEM_ATTENTION = 0x02
  };

  // The panel can be minimized to 4-pixel lines.
  static const int kMinimizedPanelHeight = 4;

  virtual ~Panel();

  // Returns the PanelManager associated with this panel.
  PanelManager* manager() const;

  void SetExpansionState(ExpansionState new_expansion_state);

  bool IsDrawingAttention() const;

  // This function will only get called by PanelManager when full screen mode
  // changes i.e it gets called when an app goes into full screen mode or when
  // an app exits full screen mode. Panel should respond by making sure
  // a) it does not go on top when some app enters full screen mode.
  // b) it remains on top when an app exits full screen mode.
  void FullScreenModeChanged(bool is_full_screen);

  // Ensures that the panel is fully visible, that is, not obscured by other
  // top-most windows.
  void EnsureFullyVisible();

  int TitleOnlyHeight() const;

  // Returns true if the panel can be minimized or restored, depending on the
  // strip the panel is in.
  bool CanMinimize() const;
  bool CanRestore() const;

  // BrowserWindow overrides.
  virtual void Show() OVERRIDE;
  virtual void ShowInactive() OVERRIDE;
  virtual void SetBounds(const gfx::Rect& bounds) OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual void Activate() OVERRIDE;
  virtual void Deactivate() OVERRIDE;
  virtual bool IsActive() const OVERRIDE;
  virtual void FlashFrame(bool flash) OVERRIDE;
  virtual bool IsAlwaysOnTop() const OVERRIDE;
  virtual gfx::NativeWindow GetNativeHandle() OVERRIDE;
  virtual BrowserWindowTesting* GetBrowserWindowTesting() OVERRIDE;
  virtual StatusBubble* GetStatusBubble() OVERRIDE;
  virtual void ToolbarSizeChanged(bool is_animating) OVERRIDE;
  virtual void UpdateTitleBar() OVERRIDE;
  virtual void BookmarkBarStateChanged(
      BookmarkBar::AnimateChangeType change_type) OVERRIDE;
  virtual void UpdateDevTools() OVERRIDE;
  virtual void SetDevToolsDockSide(DevToolsDockSide side) OVERRIDE;
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
  virtual void RotatePaneFocus(bool forwards) OVERRIDE;
  virtual bool IsBookmarkBarVisible() const OVERRIDE;
  virtual bool IsBookmarkBarAnimating() const OVERRIDE;
  virtual bool IsTabStripEditable() const OVERRIDE;
  virtual bool IsToolbarVisible() const OVERRIDE;
  virtual gfx::Rect GetRootWindowResizerRect() const OVERRIDE;
  virtual bool IsPanel() const OVERRIDE;
  virtual void DisableInactiveFrame() OVERRIDE;
  virtual void ConfirmAddSearchProvider(TemplateURL* template_url,
                                        Profile* profile) OVERRIDE;
  virtual void ToggleBookmarkBar() OVERRIDE;
  virtual void ShowAboutChromeDialog() OVERRIDE;
  virtual void ShowUpdateChromeDialog() OVERRIDE;
  virtual void ShowTaskManager() OVERRIDE;
  virtual void ShowBackgroundPages() OVERRIDE;
  virtual void ShowBookmarkBubble(
      const GURL& url, bool already_bookmarked) OVERRIDE;
  virtual void ShowChromeToMobileBubble() OVERRIDE;
#if defined(ENABLE_ONE_CLICK_SIGNIN)
  virtual void ShowOneClickSigninBubble(
      const base::Closure& learn_more_callback,
      const base::Closure& advanced_callback) OVERRIDE;
#endif
  virtual bool IsDownloadShelfVisible() const OVERRIDE;
  virtual DownloadShelf* GetDownloadShelf() OVERRIDE;
  virtual void ConfirmBrowserCloseWithPendingDownloads() OVERRIDE;
  virtual void UserChangedTheme() OVERRIDE;
  virtual int GetExtraRenderViewHeight() const OVERRIDE;
  virtual void WebContentsFocused(content::WebContents* contents) OVERRIDE;
  virtual void ShowPageInfo(Profile* profile,
                            const GURL& url,
                            const content::SSLStatus& ssl,
                            bool show_history) OVERRIDE;
  virtual void ShowWebsiteSettings(Profile* profile,
                                   TabContentsWrapper* tab_contents_wrapper,
                                   const GURL& url,
                                   const content::SSLStatus& ssl,
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
  virtual void ShowKeyboardOverlay(gfx::NativeWindow owning_window) OVERRIDE;
#endif
  virtual void ResizeDueToAutoResize(content::WebContents* web_contents,
                                     const gfx::Size& new_size) OVERRIDE;
  virtual void ShowAvatarBubble(content::WebContents* web_contents,
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

  // Invoked when the native panel has detected a mouse click on the
  // panel's titlebar, minimize or restore buttons. Behavior of the
  // click may be modified as indicated by |modifier|.
  void OnTitlebarClicked(panel::ClickModifier modifier);
  void OnMinimizeButtonClicked(panel::ClickModifier modifier);
  void OnRestoreButtonClicked(panel::ClickModifier modifier);

  // Used on platforms where the panel cannot determine its window size
  // until the window has been created. (e.g. GTK)
  void OnWindowSizeAvailable();

  // Asynchronous completion of panel close request.
  void OnNativePanelClosed();

  NativePanel* native_panel() { return native_panel_; }
  Browser* browser() const { return browser_; }

  // May be NULL if:
  // * panel is newly created and has not been positioned yet.
  // * panel is being closed asynchronously.
  // Please use it with caution.
  PanelStrip* panel_strip() const { return panel_strip_; }

  // Sets the current panel strip that contains this panel.
  void set_panel_strip(PanelStrip* new_strip) { panel_strip_ = new_strip; }

  ExpansionState expansion_state() const { return expansion_state_; }
  const gfx::Size& min_size() const { return min_size_; }
  const gfx::Size& max_size() const { return max_size_; }
  bool auto_resizable() const { return auto_resizable_; }

  bool in_preview_mode() const { return in_preview_mode_; }

  panel::Resizability CanResizeByMouse() const;

  AttentionMode attention_mode() const { return attention_mode_; }
  void set_attention_mode(AttentionMode attention_mode) {
    attention_mode_ = attention_mode;
  }

  // The full size is the size of the panel when it is detached or expanded
  // in the docked strip and squeezing mode is not on.
  gfx::Size full_size() const { return full_size_; }
  void set_full_size(const gfx::Size& size) { full_size_ = size; }

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

  // Ensures that the panel's size does not exceed the display area by
  // updating maximum and full size of the panel. This is called each time
  // when display settings are changed. Note that bounds are not updated here
  // and the call of setting bounds or refreshing layout should be called after
  // this.
  void LimitSizeToDisplayArea(const gfx::Rect& display_area);

  // Sets whether the panel will auto resize according to its content.
  void SetAutoResizable(bool resizable);

  // Sets minimum and maximum size for the panel.
  void SetSizeRange(const gfx::Size& min_size, const gfx::Size& max_size);

  // Updates the maximum size of the panel so that it's never smaller than the
  // panel's desired size. Note that even if the user resizes the panel smaller
  // later, the increased maximum size will still be in effect. Since it's not
  // possible currently to switch the panel back to autosizing from
  // user-resizable, it should not be a problem.
  void IncreaseMaxSize(const gfx::Size& desired_panel_size);

  // Whether the panel window is always on top.
  void SetAlwaysOnTop(bool on_top);
  bool always_on_top() const { return always_on_top_; }

  // Sets whether the panel is shown in preview mode. When the panel is
  // being dragged, it is in preview mode.
  void SetPreviewMode(bool in_preview_mode);

  // Sets up the panel for being resizable by the user - for example,
  // enables the resize mouse cursors when mouse is hovering over the edges.
  void EnableResizeByMouse(bool enable);

  // Sets whether the minimize or restore button, if any, are visible.
  void UpdateMinimizeRestoreButtonVisibility();

  // Changes the preferred size to acceptable based on min_size() and max_size()
  gfx::Size ClampSize(const gfx::Size& size) const;

  // Called when the panel's active state changes.
  // |active| is true if panel became active.
  void OnActiveStateChanged(bool active);

  // Called when the panel starts/ends the user resizing.
  void OnPanelStartUserResizing();
  void OnPanelEndUserResizing();

 protected:
  virtual void DestroyBrowser() OVERRIDE;

 private:
  friend class PanelManager;
  friend class PanelBrowserTest;
  FRIEND_TEST_ALL_PREFIXES(PanelBrowserTest, RestoredBounds);

  enum MaxSizePolicy {
    // Default maximum size is proportional to the work area.
    DEFAULT_MAX_SIZE,
    // Custom maximum size is used when the panel is resized by the user.
    CUSTOM_MAX_SIZE
  };

  // Panel can only be created using PanelManager::CreatePanel().
  // |requested_size| is the desired size for the panel, but actual
  // size may differ after panel layout.
  Panel(Browser* browser, const gfx::Size& requested_size);

  // Configures the web contents for auto resize, including configurations
  // on the renderer and detecting renderer changes.
  void EnableWebContentsAutoResize(content::WebContents* web_contents);

  // Configures the renderer for auto resize (if auto resize is enabled).
  void ConfigureAutoResize(content::WebContents* web_contents);

  Browser* browser_;  // Weak, owned by native panel.

  // Current collection of panels to which this panel belongs. This determines
  // the panel's screen layout.
  PanelStrip* panel_strip_;  // Owned by PanelManager.

  bool initialized_;

  // Stores the full size of the panel so we can restore it after it's
  // been minimized or squeezed due to lack of space in the strip.
  gfx::Size full_size_;

  // This is the minimum size that the panel can shrink to.
  gfx::Size min_size_;

  // This is the size beyond which the panel is not going to grow to accomodate
  // the growing content and WebKit would add the scrollbars in such case.
  gfx::Size max_size_;

  MaxSizePolicy max_size_policy_;

  // True if this panel auto resizes based on content.
  bool auto_resizable_;

  // True if this panel should always stay on top of other windows.
  bool always_on_top_;

  // True if this panel is in preview mode. When in preview mode, panel bounds
  // should not be affected by layout refresh. This is currently used by drag
  // controller to add a panel to the strip without causing its bounds to
  // change.
  bool in_preview_mode_;

  // Platform specifc implementation for panels.  It'd be one of
  // PanelBrowserWindowGtk/PanelBrowserView/PanelBrowserWindowCocoa.
  NativePanel* native_panel_;  // Weak, owns us.

  AttentionMode attention_mode_;

  ExpansionState expansion_state_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(Panel);
};

#endif  // CHROME_BROWSER_UI_PANELS_PANEL_H_
