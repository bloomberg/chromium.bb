// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_BROWSER_WINDOW_GTK_H_
#define CHROME_BROWSER_GTK_BROWSER_WINDOW_GTK_H_

#include <gtk/gtk.h>

#include <map>

#include "app/active_window_watcher_x.h"
#include "base/gfx/rect.h"
#include "base/scoped_ptr.h"
#include "base/timer.h"
#include "build/build_config.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/pref_member.h"
#include "chrome/common/x11_util.h"

#ifdef OS_CHROMEOS
namespace chromeos {
class CompactNavigationBar;
class PanelController;
class StatusAreaView;
}
#endif

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
                         public ActiveWindowWatcherX::Observer {
 public:
  explicit BrowserWindowGtk(Browser* browser);
  virtual ~BrowserWindowGtk();

  // Process a keyboard event which was not handled by webkit.
  bool HandleKeyboardEvent(GdkEventKey* event);

  // Overridden from BrowserWindow
  virtual void Show();
  virtual void SetBounds(const gfx::Rect& bounds);
  virtual void Close();
  virtual void Activate();
  virtual bool IsActive() const;
  virtual void FlashFrame();
  virtual gfx::NativeWindow GetNativeHandle();
  virtual BrowserWindowTesting* GetBrowserWindowTesting();
  virtual StatusBubble* GetStatusBubble();
  virtual void SelectedTabToolbarSizeChanged(bool is_animating);
  virtual void SelectedTabExtensionShelfSizeChanged();
  virtual void UpdateTitleBar();
  virtual void ShelfVisibilityChanged();
  virtual void UpdateDevTools();
  virtual void FocusDevTools();
  virtual void UpdateLoadingAnimations(bool should_animate);
  virtual void SetStarredState(bool is_starred);
  virtual gfx::Rect GetRestoredBounds() const;
  virtual bool IsMaximized() const;
  virtual void SetFullscreen(bool fullscreen);
  virtual bool IsFullscreen() const;
  virtual bool IsFullscreenBubbleVisible() const;
  virtual LocationBar* GetLocationBar() const;
  virtual void SetFocusToLocationBar();
  virtual void UpdateStopGoState(bool is_loading, bool force);
  virtual void UpdateToolbar(TabContents* contents,
                             bool should_restore_state);
  virtual void FocusToolbar();
  virtual bool IsBookmarkBarVisible() const;
  virtual bool IsToolbarVisible() const;
  virtual gfx::Rect GetRootWindowResizerRect() const;
  virtual void ConfirmAddSearchProvider(const TemplateURL* template_url,
                                        Profile* profile);
  virtual void ToggleBookmarkBar();
  virtual void ToggleExtensionShelf();
  virtual void ShowAboutChromeDialog();
  virtual void ShowTaskManager();
  virtual void ShowBookmarkManager();
  virtual void ShowBookmarkBubble(const GURL& url, bool already_bookmarked);
  virtual bool IsDownloadShelfVisible() const;
  virtual DownloadShelf* GetDownloadShelf();
  virtual void ShowReportBugDialog();
  virtual void ShowClearBrowsingDataDialog();
  virtual void ShowImportDialog();
  virtual void ShowSearchEnginesDialog();
  virtual void ShowPasswordManager();
  virtual void ShowSelectProfileDialog();
  virtual void ShowNewProfileDialog();
  virtual void ShowRepostFormWarningDialog(TabContents* tab_contents);
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
  virtual void ShowPageMenu();
  virtual void ShowAppMenu();
  virtual int GetCommandId(const NativeWebKeyboardEvent& event);
  virtual void ShowCreateShortcutsDialog(TabContents* tab_contents);

  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Overridden from TabStripModelObserver:
  virtual void TabDetachedAt(TabContents* contents, int index);
  virtual void TabSelectedAt(TabContents* old_contents,
                             TabContents* new_contents,
                             int index,
                             bool user_gesture);
  virtual void TabStripEmpty();

  // Overriden from ActiveWindowWatcher::Observer.
  virtual void ActiveWindowChanged(GdkWindow* active_window);

  // Accessor for the tab strip.
  TabStripGtk* tabstrip() const { return tabstrip_.get(); }

  void UpdateDevToolsForContents(TabContents* contents);

  void UpdateUIForContents(TabContents* contents);

  void OnBoundsChanged(const gfx::Rect& bounds);
  void OnDebouncedBoundsChanged();
  void OnStateChanged(GdkWindowState state, GdkWindowState changed_mask);

  // Request the underlying window to unmaximize.  Also tries to work around
  // a window manager "feature" that can prevent this in some edge cases.
  void UnMaximize();

  // Returns false if we're not ready to close yet.  E.g., a tab may have an
  // onbeforeunload handler that prevents us from closing.
  bool CanClose() const;

  bool ShouldShowWindowIcon() const;

  // Add the find bar widget to the window hierarchy.
  void AddFindBar(FindBarGtk* findbar);

#if defined(OS_CHROMEOS)
  // Sets whether a drag is active. If a drag is active the window will not
  // close.
  void set_drag_active(bool drag_active) { drag_active_ = drag_active; }

  // Sets the flag that the next toplevel browser window being created will
  // use the compact nav bar. This is used to implement the "new compact nav
  // window" menu option. This flag will be cleared after the next window is
  // opened, which will revert to the old behavior.
  //
  // TODO(brettw) remove this when we figure out how this is actually going
  // to work long-term. This is a hack so the feature can be tested.
  static void set_next_window_should_use_compact_nav() {
    next_window_should_use_compact_nav_ = true;
  }
#endif

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

  gfx::Rect bounds() const { return bounds_; }

  // Make changes necessary when the floating state of the bookmark bar changes.
  // This should only be called by the bookmark bar itself.
  void BookmarkBarIsFloating(bool is_floating);

  static void RegisterUserPrefs(PrefService* prefs);

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
  // Border between toolbar and render area. This is hidden when the find bar
  // is added because thereafter the findbar will draw the border for us.
  GtkWidget* toolbar_border_;

  scoped_ptr<Browser> browser_;

  // The download shelf view (view at the bottom of the page).
  scoped_ptr<DownloadShelfGtk> download_shelf_;

 private:
  // Show or hide the bookmark bar.
  void MaybeShowBookmarkBar(TabContents* contents, bool animate);

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
  // of the window itself, otherwise set the bounds of the web contents. In
  // either case, set the position of the window.
  void SetBoundsImpl(const gfx::Rect& bounds, bool exterior);

  // Callback for when the custom frame alignment needs to be redrawn.
  // The content area includes the toolbar and web page but not the tab strip.
  static gboolean OnCustomFrameExpose(GtkWidget* widget, GdkEventExpose* event,
                                      BrowserWindowGtk* window);
  // A helper method that draws the shadow above the toolbar and in the frame
  // border during an expose.
  static void DrawContentShadow(cairo_t* cr, BrowserWindowGtk* window);

  static gboolean OnGtkAccelerator(GtkAccelGroup* accel_group,
                                   GObject* acceleratable,
                                   guint keyval,
                                   GdkModifierType modifier,
                                   BrowserWindowGtk* browser_window);

  // Mouse move and mouse button press callbacks.
  static gboolean OnMouseMoveEvent(GtkWidget* widget,
                                   GdkEventMotion* event,
                                   BrowserWindowGtk* browser);
  static gboolean OnButtonPressEvent(GtkWidget* widget,
                                     GdkEventButton* event,
                                     BrowserWindowGtk* browser);

  // Maps and Unmaps the xid of |widget| to |window|.
  static void MainWindowMapped(GtkWidget* widget, BrowserWindowGtk* window);
  static void MainWindowUnMapped(GtkWidget* widget, BrowserWindowGtk* window);

  // Tracks focus state of browser.
  static gboolean OnFocusIn(GtkWidget* widget,
                            GdkEventFocus* event,
                            BrowserWindowGtk* browser);
  static gboolean OnFocusOut(GtkWidget* widget,
                             GdkEventFocus* event,
                             BrowserWindowGtk* browser);

  // A small shim for browser_->ExecuteCommand.
  void ExecuteBrowserCommand(int id);

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

  // Checks to see if the mouse pointer at |x|, |y| is over the border of the
  // custom frame (a spot that should trigger a window resize). Returns true if
  // it should and sets |edge|.
  bool GetWindowEdge(int x, int y, GdkWindowEdge* edge);

  // Returns |true| if we should use the custom frame.
  bool UseCustomFrame();

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

#if defined(OS_CHROMEOS)
  // True if a drag is active. See description above setter for details.
  bool drag_active_;
  // Controls interactions with the window manager for popup panels.
  chromeos::PanelController* panel_controller_;

  chromeos::CompactNavigationBar* compact_navigation_bar_;
  chromeos::StatusAreaView* status_area_;

  // The MainMenu button.
  CustomDrawButton* main_menu_button_;

  // A hbox container for the compact navigation bar.
  GtkWidget* compact_navbar_hbox_;

  static bool next_window_should_use_compact_nav_;
#endif

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

  // The accelerator group used to handle accelerators, owned by this object.
  GtkAccelGroup* accel_group_;

  scoped_ptr<FullscreenExitBubbleGtk> fullscreen_exit_bubble_;

  DISALLOW_COPY_AND_ASSIGN(BrowserWindowGtk);
};

#endif  // CHROME_BROWSER_GTK_BROWSER_WINDOW_GTK_H_
