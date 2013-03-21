// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_BROWSER_WINDOW_GTK_H_
#define CHROME_BROWSER_UI_GTK_BROWSER_WINDOW_GTK_H_

#include <gtk/gtk.h>

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_member.h"
#include "base/timer.h"
#include "build/build_config.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/extension_keybinding_registry.h"
#include "chrome/browser/infobars/infobar_container.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/base/ui_base_types.h"
#include "ui/base/x/active_window_watcher_x_observer.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/rect.h"

class BookmarkBarGtk;
class Browser;
class BrowserTitlebar;
class BrowserToolbarGtk;
class DevToolsWindow;
class DownloadShelfGtk;
class ExtensionKeybindingRegistryGtk;
class FindBarGtk;
class FullscreenExitBubbleGtk;
class GlobalMenuBar;
class InfoBarContainerGtk;
class InstantOverlayControllerGtk;
class LocationBar;
class PrefRegistrySyncable;
class StatusBubbleGtk;
class TabContentsContainerGtk;
class TabStripGtk;

namespace autofill {
class PasswordGenerator;
}

namespace extensions {
class ActiveTabPermissionGranter;
class Extension;
}

// An implementation of BrowserWindow for GTK.
// Cross-platform code will interact with this object when
// it needs to manipulate the window.

class BrowserWindowGtk
    : public BrowserWindow,
      public content::NotificationObserver,
      public TabStripModelObserver,
      public ui::ActiveWindowWatcherXObserver,
      public InfoBarContainer::Delegate,
      public extensions::ExtensionKeybindingRegistry::Delegate {
 public:
  explicit BrowserWindowGtk(Browser* browser);
  virtual ~BrowserWindowGtk();

  // Separating initialization from constructor.
  void Init();

  // Overridden from BrowserWindow:
  virtual void Show() OVERRIDE;
  virtual void ShowInactive() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual void SetBounds(const gfx::Rect& bounds) OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual void Activate() OVERRIDE;
  virtual void Deactivate() OVERRIDE;
  virtual bool IsActive() const OVERRIDE;
  virtual void FlashFrame(bool flash) OVERRIDE;
  virtual bool IsAlwaysOnTop() const OVERRIDE;
  virtual gfx::NativeWindow GetNativeWindow() OVERRIDE;
  virtual BrowserWindowTesting* GetBrowserWindowTesting() OVERRIDE;
  virtual StatusBubble* GetStatusBubble() OVERRIDE;
  virtual void UpdateTitleBar() OVERRIDE;
  virtual void BookmarkBarStateChanged(
      BookmarkBar::AnimateChangeType change_type) OVERRIDE;
  virtual void UpdateDevTools() OVERRIDE;
  virtual void UpdateLoadingAnimations(bool should_animate) OVERRIDE;
  virtual void SetStarredState(bool is_starred) OVERRIDE;
  virtual void ZoomChangedForActiveTab(bool can_show_bubble) OVERRIDE;
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
  virtual bool ShouldHideUIForFullscreen() const OVERRIDE;
  virtual bool IsFullscreen() const OVERRIDE;
  virtual bool IsFullscreenBubbleVisible() const OVERRIDE;
  virtual LocationBar* GetLocationBar() const OVERRIDE;
  virtual void SetFocusToLocationBar(bool select_all) OVERRIDE;
  virtual void UpdateReloadStopState(bool is_loading, bool force) OVERRIDE;
  virtual void UpdateToolbar(content::WebContents* contents,
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
  virtual void ConfirmAddSearchProvider(TemplateURL* template_url,
                                        Profile* profile) OVERRIDE;
  virtual void ToggleBookmarkBar() OVERRIDE;
  virtual void ShowUpdateChromeDialog() OVERRIDE;
  virtual void ShowBookmarkBubble(const GURL& url,
                                  bool already_bookmarked) OVERRIDE;
  virtual void ShowChromeToMobileBubble() OVERRIDE;
#if defined(ENABLE_ONE_CLICK_SIGNIN)
  virtual void ShowOneClickSigninBubble(
      OneClickSigninBubbleType type,
      const StartSyncCallback& start_sync_callback) OVERRIDE;
#endif
  virtual bool IsDownloadShelfVisible() const OVERRIDE;
  virtual DownloadShelf* GetDownloadShelf() OVERRIDE;
  virtual void ConfirmBrowserCloseWithPendingDownloads() OVERRIDE;
  virtual void UserChangedTheme() OVERRIDE;
  virtual int GetExtraRenderViewHeight() const OVERRIDE;
  virtual void WebContentsFocused(content::WebContents* contents) OVERRIDE;
  virtual void ShowWebsiteSettings(Profile* profile,
                                   content::WebContents* web_contents,
                                   const GURL& url,
                                   const content::SSLStatus& ssl,
                                   bool show_history) OVERRIDE;
  virtual void ShowAppMenu() OVERRIDE;
  virtual bool PreHandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event,
      bool* is_keyboard_shortcut) OVERRIDE;
  virtual void HandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) OVERRIDE;
  virtual void ShowCreateChromeAppShortcutsDialog(
      Profile* profile,
      const extensions::Extension* app) OVERRIDE;
  virtual void Cut() OVERRIDE;
  virtual void Copy() OVERRIDE;
  virtual void Paste() OVERRIDE;
  virtual gfx::Rect GetInstantBounds() OVERRIDE;
  virtual WindowOpenDisposition GetDispositionForPopupBounds(
      const gfx::Rect& bounds) OVERRIDE;
  virtual FindBar* CreateFindBar() OVERRIDE;
  virtual bool GetConstrainedWindowTopY(int* top_y) OVERRIDE;
  virtual void ShowAvatarBubble(content::WebContents* web_contents,
                                const gfx::Rect& rect) OVERRIDE;
  virtual void ShowAvatarBubbleFromAvatarButton() OVERRIDE;
  virtual void ShowPasswordGenerationBubble(
      const gfx::Rect& rect,
      const content::PasswordForm& form,
      autofill::PasswordGenerator* password_generator) OVERRIDE;

  // Overridden from NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Overridden from TabStripModelObserver:
  virtual void TabDetachedAt(content::WebContents* contents,
                             int index) OVERRIDE;
  virtual void ActiveTabChanged(content::WebContents* old_contents,
                                content::WebContents* new_contents,
                                int index,
                                bool user_gesture) OVERRIDE;

  // Overridden from ActiveWindowWatcherXObserver.
  virtual void ActiveWindowChanged(GdkWindow* active_window) OVERRIDE;

  // Overridden from InfoBarContainer::Delegate:
  virtual SkColor GetInfoBarSeparatorColor() const OVERRIDE;
  virtual void InfoBarContainerStateChanged(bool is_animating) OVERRIDE;
  virtual bool DrawInfoBarArrows(int* x) const OVERRIDE;

  // Overridden from ExtensionKeybindingRegistry::Delegate:
  virtual extensions::ActiveTabPermissionGranter*
      GetActiveTabPermissionGranter() OVERRIDE;

  // Accessor for the tab strip.
  TabStripGtk* tabstrip() const { return tabstrip_.get(); }

  void OnDebouncedBoundsChanged();

  // Request the underlying window to unmaximize.
  void UnMaximize();

  // Returns false if we're not ready to close yet.  E.g., a tab may have an
  // onbeforeunload handler that prevents us from closing.
  bool CanClose() const;

  // Returns whether to draw the content drop shadow on the sides and bottom
  // of the browser window. When false, we still draw a shadow on the top of
  // the toolbar (under the tab strip), but do not round the top corners.
  bool ShouldDrawContentDropShadow() const;

  bool ShouldShowWindowIcon() const;

  // Add the find bar widget to the window hierarchy.
  void AddFindBar(FindBarGtk* findbar);

  // Reset the mouse cursor to the default cursor if it was set to something
  // else for the custom frame.
  void ResetCustomFrameCursor();

  // Returns the BrowserWindowGtk registered with |window|.
  static BrowserWindowGtk* GetBrowserWindowForNativeWindow(
      gfx::NativeWindow window);

  // Retrieves the GtkWindow associated with |xid|, which is the X Window
  // ID of the top-level X window of this object.
  static GtkWindow* GetBrowserWindowForXID(XID xid);

  Browser* browser() const { return browser_.get(); }

  GtkWindow* window() const { return window_; }

  BrowserTitlebar* titlebar() const { return titlebar_.get(); }

  GtkWidget* titlebar_widget() const;

  BrowserToolbarGtk* GetToolbar() { return toolbar_.get(); }

  gfx::Rect bounds() const { return bounds_; }

  // Returns the tab we're currently displaying in the tab contents container.
  content::WebContents* GetDisplayedTab();

  static void RegisterUserPrefs(PrefRegistrySyncable* registry);

  // Tells GTK that the toolbar area is invalidated and needs redrawing. We
  // have this method as a hack because GTK doesn't queue the toolbar area for
  // redraw when it should.
  void QueueToolbarRedraw();

  // Get the position where the infobar arrow should be anchored in
  // |relative_to| coordinates. This is the middle of the omnibox location icon.
  int GetXPositionOfLocationIcon(GtkWidget* relative_to);

  // Show or hide the bookmark bar.
  void MaybeShowBookmarkBar(bool animate);

 protected:
  virtual void DestroyBrowser() OVERRIDE;

  // Checks to see if the mouse pointer at |x|, |y| is over the border of the
  // custom frame (a spot that should trigger a window resize). Returns true if
  // it should and sets |edge|.
  bool GetWindowEdge(int x, int y, GdkWindowEdge* edge);

  // Returns the window shape for the window with |width| and |height|.
  // The caller is responsible for destroying the region if non-null region is
  // returned.
  GdkRegion* GetWindowShape(int width, int height) const;

  // Save the window position in the prefs.
  void SaveWindowPosition();

  // Sets the default size for the window and the way the user is allowed to
  // resize it.
  void SetGeometryHints();

  // Returns |true| if we should use the custom frame.
  bool UseCustomFrame() const;

  // Invalidate window to force repaint.
  void InvalidateWindow();

  // Top level window. NULL after the window starts closing.
  GtkWindow* window_;
  // Determines whether window was shown.
  bool window_has_shown_;
  // GtkAlignment that holds the interior components of the chromium window.
  // This is used to draw the custom frame border and content shadow. Owned by
  // window_.
  GtkWidget* window_container_;
  // VBox that holds everything (tabs, toolbar, bookmarks bar, tab contents).
  // Owned by window_container_.
  GtkWidget* window_vbox_;
  // VBox that holds everything below the toolbar. Owned by
  // render_area_floating_container_.
  GtkWidget* render_area_vbox_;
  // Floating container that holds the render area. It is needed to position
  // the findbar. Owned by render_area_event_box_.
  GtkWidget* render_area_floating_container_;
  // EventBox that holds render_area_floating_container_. Owned by window_vbox_.
  GtkWidget* render_area_event_box_;
  // Border between toolbar and render area. Owned by render_area_vbox_.
  GtkWidget* toolbar_border_;

  scoped_ptr<Browser> browser_;

 private:
  // Connect to signals on |window_|.
  void ConnectHandlersToSignals();

  // Create the various UI components.
  void InitWidgets();

  // Set up background color of the window (depends on if we're incognito or
  // not).
  void SetBackgroundColor();

  // Applies the window shape to if we're in custom drawing mode.
  void UpdateWindowShape(int width, int height);

  // Connect accelerators that aren't connected to menu items (like ctrl-o,
  // ctrl-l, etc.).
  void ConnectAccelerators();

  // Whether we should draw the tab background instead of the theme_frame
  // background because this window is a popup.
  bool UsingCustomPopupFrame() const;

  // Draws the normal custom frame using theme_frame.
  void DrawCustomFrame(cairo_t* cr, GtkWidget* widget, GdkEventExpose* event);

  // Draws the tab image as the frame so we can write legible text.
  void DrawPopupFrame(cairo_t* cr, GtkWidget* widget, GdkEventExpose* event);

  // Draws the border, including resizable corners, for the custom frame.
  void DrawCustomFrameBorder(GtkWidget* widget);

  // Change whether we're showing the custom blue frame.
  // Must be called once at startup.
  // Triggers relayout of the content.
  void UpdateCustomFrame();

  // Set the bounds of the current window. If |exterior| is true, set the size
  // of the window itself, otherwise set the bounds of the web contents.
  // If |move| is true, set the position of the window, otherwise leave the
  // position to the WM.
  void SetBoundsImpl(const gfx::Rect& bounds, bool exterior, bool move);

  CHROMEGTK_CALLBACK_1(BrowserWindowGtk, gboolean, OnConfigure,
                       GdkEventConfigure*);
  CHROMEGTK_CALLBACK_1(BrowserWindowGtk, gboolean, OnWindowState,
                       GdkEventWindowState*);
  CHROMEGTK_CALLBACK_1(BrowserWindowGtk, gboolean, OnMainWindowDeleteEvent,
                       GdkEvent*);
  CHROMEGTK_CALLBACK_0(BrowserWindowGtk, void, OnMainWindowDestroy);
  // Callback for when the custom frame alignment needs to be redrawn.
  // The content area includes the toolbar and web page but not the tab strip.
  CHROMEGTK_CALLBACK_1(BrowserWindowGtk, gboolean, OnCustomFrameExpose,
                       GdkEventExpose*);

  // A helper method that draws the shadow above the toolbar and in the frame
  // border during an expose.
  void DrawContentShadow(cairo_t* cr);

  // The background frame image needs to be offset by the size of the top of
  // the window to the top of the tabs when the full skyline isn't displayed
  // for some reason.
  int GetVerticalOffset();

  // Returns which frame image we should use based on the window's current
  // activation state / incognito state.
  int GetThemeFrameResource();

  // Invalidate all the widgets that need to redraw when the infobar draw state
  // has changed.
  void InvalidateInfoBarBits();

  // When the location icon moves, we have to redraw the arrow.
  CHROMEGTK_CALLBACK_1(BrowserWindowGtk, void, OnLocationIconSizeAllocate,
                       GtkAllocation*);

  // Used to draw the infobar arrow and drop shadow. This is connected to
  // multiple widgets' expose events because it overlaps several widgets.
  CHROMEGTK_CALLBACK_1(BrowserWindowGtk, gboolean, OnExposeDrawInfobarBits,
                       GdkEventExpose*);

  // Used to draw the infobar bits for the bookmark bar. When the bookmark
  // bar is in floating mode, it has to draw a drop shadow only; otherwise
  // it is responsible for its portion of the arrow as well as some shadowing.
  CHROMEGTK_CALLBACK_1(BrowserWindowGtk, gboolean, OnBookmarkBarExpose,
                       GdkEventExpose*);

  // Callback for "size-allocate" signal on bookmark bar; this is relevant
  // because when the bookmark bar changes dimensions, the infobar arrow has to
  // change its shape, and we need to queue appropriate redraws.
  CHROMEGTK_CALLBACK_1(BrowserWindowGtk, void, OnBookmarkBarSizeAllocate,
                       GtkAllocation*);

  // Callback for accelerator activation. |user_data| stores the command id
  // of the matched accelerator.
  static gboolean OnGtkAccelerator(GtkAccelGroup* accel_group,
                                   GObject* acceleratable,
                                   guint keyval,
                                   GdkModifierType modifier,
                                   void* user_data);

  // Key press event callback.
  CHROMEGTK_CALLBACK_1(BrowserWindowGtk, gboolean, OnKeyPress, GdkEventKey*);

  // Mouse move and mouse button press callbacks.
  CHROMEGTK_CALLBACK_1(BrowserWindowGtk, gboolean, OnMouseMoveEvent,
                       GdkEventMotion*);
  CHROMEGTK_CALLBACK_1(BrowserWindowGtk, gboolean, OnButtonPressEvent,
                       GdkEventButton*);

  // Tracks focus state of browser.
  CHROMEGTK_CALLBACK_1(BrowserWindowGtk, gboolean, OnFocusIn,
                       GdkEventFocus*);
  CHROMEGTK_CALLBACK_1(BrowserWindowGtk, gboolean, OnFocusOut,
                       GdkEventFocus*);

  // Callback for the loading animation(s) associated with this window.
  void LoadingAnimationCallback();

  // Shows UI elements for supported window features.
  void ShowSupportedWindowFeatures();

  // Hides UI elements for unsupported window features.
  void HideUnsupportedWindowFeatures();

  // Helper functions that query |browser_| concerning support for UI features
  // in this window. (For example, a popup window might not support a tabstrip).
  bool IsTabStripSupported() const;
  bool IsToolbarSupported() const;
  bool IsBookmarkBarSupported() const;

  // Put the bookmark bar where it belongs.
  void PlaceBookmarkBar(bool is_floating);

  // Decides if we should draw the frame as if the window is active.
  bool DrawFrameAsActive() const;

  // Updates devtools window for given contents. This method will show docked
  // devtools window for inspected |contents| that has docked devtools
  // and hide it for NULL or not inspected |contents|. It will also make
  // sure devtools window size and position are restored for given tab.
  void UpdateDevToolsForContents(content::WebContents* contents);

  // Shows docked devtools.
  void ShowDevToolsContainer();

  // Hides docked devtools.
  void HideDevToolsContainer();

  // Reads split position from the current tab's devtools window and applies
  // it to the devtools split.
  void UpdateDevToolsSplitPosition();

  // Called when the preference changes.
  void OnUseCustomChromeFrameChanged();

  // Determine whether we use should default to native decorations or the custom
  // frame based on the currently-running window manager.
  static bool GetCustomFramePrefDefault();

  // The position and size of the current window.
  gfx::Rect bounds_;

  // The configure bounds of the current window, used to figure out whether to
  // ignore later configure events. See OnConfigure() for more information.
  gfx::Rect configure_bounds_;

  // The position and size of the non-maximized, non-fullscreen window.
  gfx::Rect restored_bounds_;

  GdkWindowState state_;

  // Controls a hidden GtkMenuBar that we keep updated so GNOME can take a look
  // inside "our menu bar" and present it in the top panel, akin to Mac OS.
  scoped_ptr<GlobalMenuBar> global_menu_bar_;

  // The container for the titlebar + tab strip.
  scoped_ptr<BrowserTitlebar> titlebar_;

  // The object that manages all of the widgets in the toolbar.
  scoped_ptr<BrowserToolbarGtk> toolbar_;

  // The object that manages the bookmark bar. This will be NULL if the
  // bookmark bar is not supported.
  scoped_ptr<BookmarkBarGtk> bookmark_bar_;

  // The download shelf view (view at the bottom of the page).
  scoped_ptr<DownloadShelfGtk> download_shelf_;

  // The status bubble manager.  Always non-NULL.
  scoped_ptr<StatusBubbleGtk> status_bubble_;

  // A container that manages the GtkWidget*s that are the webpage display
  // (along with associated infobars, shelves, and other things that are part
  // of the content area).
  scoped_ptr<TabContentsContainerGtk> contents_container_;

  // A container that manages the GtkWidget*s of developer tools for the
  // selected tab contents.
  scoped_ptr<TabContentsContainerGtk> devtools_container_;

  // A sub-controller that manages the Instant overlay visual state.
  scoped_ptr<InstantOverlayControllerGtk> instant_overlay_controller_;

  // The Extension Keybinding Registry responsible for registering listeners for
  // accelerators that are sent to the window, that are destined to be turned
  // into events and sent to the extension.
  scoped_ptr<ExtensionKeybindingRegistryGtk> extension_keybinding_registry_;

  DevToolsDockSide devtools_dock_side_;

  // Docked devtools window instance. NULL when current tab is not inspected
  // or is inspected with undocked version of DevToolsWindow.
  DevToolsWindow* devtools_window_;

  // Split pane containing the contents_container_ and the devtools_container_.
  // Owned by contents_vsplit_.
  GtkWidget* contents_hsplit_;

  // Split pane containing the contents_hsplit_ and the devtools_container_.
  // Owned by render_area_vbox_.
  GtkWidget* contents_vsplit_;

  // The tab strip.  Always non-NULL.
  scoped_ptr<TabStripGtk> tabstrip_;

  // The container for info bars. Always non-NULL.
  scoped_ptr<InfoBarContainerGtk> infobar_container_;

  // The timer used to update frames for the Loading Animation.
  base::RepeatingTimer<BrowserWindowGtk> loading_animation_timer_;

  // The timer used to save the window position for session restore.
  base::OneShotTimer<BrowserWindowGtk> window_configure_debounce_timer_;

  // Whether the custom chrome frame pref is set.  Normally you want to use
  // UseCustomFrame() above to determine whether to use the custom frame or
  // not.
  BooleanPrefMember use_custom_frame_pref_;

  // The current window cursor.  We set it to a resize cursor when over the
  // custom frame border.  We set it to NULL if we want the default cursor.
  GdkCursor* frame_cursor_;

  // True if the window manager thinks the window is active.  Not all window
  // managers keep track of this state (_NET_ACTIVE_WINDOW), in which case
  // this will always be true.
  bool is_active_;

  // Optionally maximize or minimize the window after we call
  // BrowserWindow::Show for the first time.  This is to work around a compiz
  // bug.
  ui::WindowShowState show_state_after_show_;

  // If true, don't call gdk_window_raise() when we get a click in the title
  // bar or window border.  This is to work around a compiz bug.
  bool suppress_window_raise_;

  // The accelerator group used to handle accelerators, owned by this object.
  GtkAccelGroup* accel_group_;

  scoped_ptr<FullscreenExitBubbleGtk> fullscreen_exit_bubble_;

  FullscreenExitBubbleType fullscreen_exit_bubble_type_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(BrowserWindowGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_BROWSER_WINDOW_GTK_H_
