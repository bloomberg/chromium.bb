// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_BROWSER_WINDOW_GTK_H_
#define CHROME_BROWSER_UI_GTK_BROWSER_WINDOW_GTK_H_
#pragma once

#include <gtk/gtk.h>

#include <map>

#include "base/scoped_ptr.h"
#include "base/timer.h"
#include "build/build_config.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/gtk/infobars/infobar_arrow_model.h"
#include "content/common/notification_registrar.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/base/x/active_window_watcher_x.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/rect.h"

class BookmarkBarGtk;
class Browser;
class BrowserTitlebar;
class BrowserToolbarGtk;
class CustomDrawButton;
class DownloadShelfGtk;
class FindBarGtk;
class FullscreenExitBubbleGtk;
class InfoBarContainerGtk;
class LocationBar;
class StatusBubbleGtk;
class TabContentsContainerGtk;
class TabStripGtk;

// An implementation of BrowserWindow for GTK.
// Cross-platform code will interact with this object when
// it needs to manipulate the window.

class BrowserWindowGtk : public BrowserWindow,
                         public NotificationObserver,
                         public TabStripModelObserver,
                         public ui::ActiveWindowWatcherX::Observer,
                         public InfoBarArrowModel::Observer {
 public:
  explicit BrowserWindowGtk(Browser* browser);
  virtual ~BrowserWindowGtk();

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
  virtual void ShowCreateWebAppShortcutsDialog(TabContents*  tab_contents);
  virtual void ShowCreateChromeAppShortcutsDialog(Profile* profile,
                                                  const Extension* app);
  virtual void Cut();
  virtual void Copy();
  virtual void Paste();
  virtual void ToggleTabStripMode() {}
  virtual void PrepareForInstant();
  virtual void ShowInstant(TabContents* preview_contents);
  virtual void HideInstant(bool instant_is_active);
  virtual gfx::Rect GetInstantBounds();

  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Overridden from TabStripModelObserver:
  virtual void TabDetachedAt(TabContentsWrapper* contents, int index);
  virtual void TabSelectedAt(TabContentsWrapper* old_contents,
                             TabContentsWrapper* new_contents,
                             int index,
                             bool user_gesture);

  // Overridden from ActiveWindowWatcher::Observer.
  virtual void ActiveWindowChanged(GdkWindow* active_window);

  // Overridden from InfoBarArrowModel::Observer.
  virtual void PaintStateChanged();

  // Accessor for the tab strip.
  TabStripGtk* tabstrip() const { return tabstrip_.get(); }

  void UpdateDevToolsForContents(TabContents* contents);

  void OnDebouncedBoundsChanged();

  // Request the underlying window to unmaximize.  Also tries to work around
  // a window manager "feature" that can prevent this in some edge cases.
  void UnMaximize();

  // Returns false if we're not ready to close yet.  E.g., a tab may have an
  // onbeforeunload handler that prevents us from closing.
  bool CanClose() const;

  bool ShouldShowWindowIcon() const;

  // Add the find bar widget to the window hierarchy.
  void AddFindBar(FindBarGtk* findbar);

  // Reset the mouse cursor to the default cursor if it was set to something
  // else for the custom frame.
  void ResetCustomFrameCursor();

  // Toggles whether an infobar is showing.
  // |animate| controls whether we animate to the new state set by |bar|.
  void SetInfoBarShowing(InfoBar* bar, bool animate);

  // Returns the BrowserWindowGtk registered with |window|.
  static BrowserWindowGtk* GetBrowserWindowForNativeWindow(
      gfx::NativeWindow window);

  // Retrieves the GtkWindow associated with |xid|, which is the X Window
  // ID of the top-level X window of this object.
  static GtkWindow* GetBrowserWindowForXID(XID xid);

  Browser* browser() const { return browser_.get(); }

  GtkWindow* window() const { return window_; }

  BrowserToolbarGtk* GetToolbar() { return toolbar_.get(); }

  gfx::Rect bounds() const { return bounds_; }

  // Make changes necessary when the floating state of the bookmark bar changes.
  // This should only be called by the bookmark bar itself.
  void BookmarkBarIsFloating(bool is_floating);

  // Returns the tab contents we're currently displaying in the tab contents
  // container.
  TabContents* GetDisplayedTabContents();

  static void RegisterUserPrefs(PrefService* prefs);

  // Returns whether to draw the content drop shadow on the sides and bottom
  // of the browser window. When false, we still draw a shadow on the top of
  // the toolbar (under the tab strip), but do not round the top corners.
  bool ShouldDrawContentDropShadow();

  // Tells GTK that the toolbar area is invalidated and needs redrawing. We
  // have this method as a hack because GTK doesn't queue the toolbar area for
  // redraw when it should.
  void QueueToolbarRedraw();

  // Get the position where the infobar arrow should be anchored in
  // |relative_to| coordinates. This is the middle of the omnibox location icon.
  int GetXPositionOfLocationIcon(GtkWidget* relative_to);

 protected:
  virtual void DestroyBrowser();
  // Top level window.
  GtkWindow* window_;
  // GtkAlignment that holds the interior components of the chromium window.
  // This is used to draw the custom frame border and content shadow.
  GtkWidget* window_container_;
  // VBox that holds everything (tabs, toolbar, bookmarks bar, tab contents).
  GtkWidget* window_vbox_;
  // VBox that holds everything below the toolbar.
  GtkWidget* render_area_vbox_;
  // Floating container that holds the render area. It is needed to position
  // the findbar.
  GtkWidget* render_area_floating_container_;
  // EventBox that holds render_area_floating_container_.
  GtkWidget* render_area_event_box_;
  // Border between toolbar and render area.
  GtkWidget* toolbar_border_;

  scoped_ptr<Browser> browser_;

  // The download shelf view (view at the bottom of the page).
  scoped_ptr<DownloadShelfGtk> download_shelf_;

 private:
  // Show or hide the bookmark bar.
  void MaybeShowBookmarkBar(bool animate);

  // Sets the default size for the window and the the way the user is allowed to
  // resize it.
  void SetGeometryHints();

  // Connect to signals on |window_|.
  void ConnectHandlersToSignals();

  // Create the various UI components.
  void InitWidgets();

  // Set up background color of the window (depends on if we're incognito or
  // not).
  void SetBackgroundColor();

  // Called when the window size changed.
  void OnSizeChanged(int width, int height);

  // Applies the window shape to if we're in custom drawing mode.
  void UpdateWindowShape(int width, int height);

  // Connect accelerators that aren't connected to menu items (like ctrl-o,
  // ctrl-l, etc.).
  void ConnectAccelerators();

  // Change whether we're showing the custom blue frame.
  // Must be called once at startup.
  // Triggers relayout of the content.
  void UpdateCustomFrame();

  // Save the window position in the prefs.
  void SaveWindowPosition();

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

  // Draws the tab image as the frame so we can write legible text.
  void DrawPopupFrame(cairo_t* cr, GtkWidget* widget, GdkEventExpose* event);

  // Draws the normal custom frame using theme_frame.
  void DrawCustomFrame(cairo_t* cr, GtkWidget* widget, GdkEventExpose* event);

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

  // Maps and Unmaps the xid of |widget| to |window|.
  static void MainWindowMapped(GtkWidget* widget);
  static void MainWindowUnMapped(GtkWidget* widget);

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

  // Whether we should draw the tab background instead of the theme_frame
  // background because this window is a popup.
  bool UsingCustomPopupFrame() const;

  // Checks to see if the mouse pointer at |x|, |y| is over the border of the
  // custom frame (a spot that should trigger a window resize). Returns true if
  // it should and sets |edge|.
  bool GetWindowEdge(int x, int y, GdkWindowEdge* edge);

  // Returns |true| if we should use the custom frame.
  bool UseCustomFrame();

  // Returns |true| if the window bounds match the monitor size.
  bool BoundsMatchMonitorSize();

  // Put the bookmark bar where it belongs.
  void PlaceBookmarkBar(bool is_floating);

  // Determine whether we use should default to native decorations or the custom
  // frame based on the currently-running window manager.
  static bool GetCustomFramePrefDefault();

  NotificationRegistrar registrar_;

  // The position and size of the current window.
  gfx::Rect bounds_;

  // The position and size of the non-maximized, non-fullscreen window.
  gfx::Rect restored_bounds_;

  GdkWindowState state_;

  // The container for the titlebar + tab strip.
  scoped_ptr<BrowserTitlebar> titlebar_;

  // The object that manages all of the widgets in the toolbar.
  scoped_ptr<BrowserToolbarGtk> toolbar_;

  // The object that manages the bookmark bar. This will be NULL if the
  // bookmark bar is not supported.
  scoped_ptr<BookmarkBarGtk> bookmark_bar_;

  // Caches the hover state of the bookmark bar.
  bool bookmark_bar_is_floating_;

  // The status bubble manager.  Always non-NULL.
  scoped_ptr<StatusBubbleGtk> status_bubble_;

  // A container that manages the GtkWidget*s that are the webpage display
  // (along with associated infobars, shelves, and other things that are part
  // of the content area).
  scoped_ptr<TabContentsContainerGtk> contents_container_;

  // A container that manages the GtkWidget*s of developer tools for the
  // selected tab contents.
  scoped_ptr<TabContentsContainerGtk> devtools_container_;

  // Split pane containing the contents_container_ and the devtools_container_.
  GtkWidget* contents_split_;

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

  // A map which translates an X Window ID into its respective GtkWindow.
  static std::map<XID, GtkWindow*> xid_map_;

  // The current window cursor.  We set it to a resize cursor when over the
  // custom frame border.  We set it to NULL if we want the default cursor.
  GdkCursor* frame_cursor_;

  // True if the window manager thinks the window is active.  Not all window
  // managers keep track of this state (_NET_ACTIVE_WINDOW), in which case
  // this will always be true.
  bool is_active_;

  // Keep track of the last click time and the last click position so we can
  // filter out extra GDK_BUTTON_PRESS events when a double click happens.
  guint32 last_click_time_;
  gfx::Point last_click_position_;

  // If true, maximize the window after we call BrowserWindow::Show for the
  // first time.  This is to work around a compiz bug.
  bool maximize_after_show_;

  // If true, don't call gdk_window_raise() when we get a click in the title
  // bar or window border.  This is to work around a compiz bug.
  bool suppress_window_raise_;

  // The accelerator group used to handle accelerators, owned by this object.
  GtkAccelGroup* accel_group_;

  scoped_ptr<FullscreenExitBubbleGtk> fullscreen_exit_bubble_;

  // The model that tracks the paint state of the arrow for the infobar
  // directly below the toolbar.
  InfoBarArrowModel infobar_arrow_model_;

  DISALLOW_COPY_AND_ASSIGN(BrowserWindowGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_BROWSER_WINDOW_GTK_H_
