// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_H_
#define CHROME_BROWSER_UI_BROWSER_H_
#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/string16.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/debugger/devtools_toggle_action.h"
#include "chrome/browser/event_disposition.h"
#include "chrome/browser/extensions/extension_tab_helper_delegate.h"
#include "chrome/browser/instant/instant_delegate.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/sessions/session_id.h"
#include "chrome/browser/sessions/tab_restore_service_observer.h"
#include "chrome/browser/sync/profile_sync_service_observer.h"
#include "chrome/browser/ui/blocked_content/blocked_content_tab_helper_delegate.h"
#include "chrome/browser/ui/bookmarks/bookmark_bar.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper_delegate.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/constrained_window_tab_helper_delegate.h"
#include "chrome/browser/ui/fullscreen_exit_bubble_type.h"
#include "chrome/browser/ui/search_engines/search_engine_tab_helper_delegate.h"
#include "chrome/browser/ui/select_file_dialog.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/toolbar/toolbar_model.h"
#include "chrome/browser/ui/zoom/zoom_observer.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/content_settings_types.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/common/page_zoom.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/rect.h"

class BrowserContentSettingBubbleModelDelegate;
class BrowserSyncedWindowDelegate;
class BrowserToolbarModelDelegate;
class BrowserTabRestoreServiceDelegate;
class BrowserWindow;
class ExtensionWindowController;
class FindBarController;
class FullscreenController;
class InstantController;
class InstantUnloadHandler;
class PrefService;
class Profile;
class SkBitmap;
class StatusBubble;
class TabNavigation;
class TabStripModel;
struct WebApplicationInfo;

namespace chrome {
namespace search {
class SearchDelegate;
class SearchModel;
}
}

namespace content {
class NavigationController;
class SessionStorageNamespace;
}

namespace extensions {
class Extension;
}

namespace gfx {
class Point;
}

namespace ui {
class WebDialogDelegate;
}

namespace webkit_glue {
struct WebIntentServiceData;
}

class Browser : public TabStripModelDelegate,
                public TabStripModelObserver,
                public content::WebContentsDelegate,
                public CoreTabHelperDelegate,
                public SearchEngineTabHelperDelegate,
                public ConstrainedWindowTabHelperDelegate,
                public BlockedContentTabHelperDelegate,
                public BookmarkTabHelperDelegate,
                public ZoomObserver,
                public ExtensionTabHelperDelegate,
                public content::PageNavigator,
                public CommandUpdater::CommandUpdaterDelegate,
                public content::NotificationObserver,
                public SelectFileDialog::Listener,
                public TabRestoreServiceObserver,
                public ProfileSyncServiceObserver,
                public InstantDelegate {
 public:
  // SessionService::WindowType mirrors these values.  If you add to this
  // enum, look at SessionService::WindowType to see if it needs to be
  // updated.
  enum Type {
    // If you add a new type, consider updating the test
    // BrowserTest.StartMaximized.
    TYPE_TABBED = 1,
    TYPE_POPUP = 2,
    TYPE_PANEL = 3
  };

  // Distinguishes between browsers that host an app (opened from
  // ApplicationLauncher::OpenApplication), and child browsers created by an app
  // from Browser::CreateForApp (e.g. by windows.open or the extension API).
  // TODO(stevenjb): This is currently only needed by the ash Launcher for
  // identifying child panels. Remove this once panels are no longer
  // implemented as Browsers, crbug.com/112198.
  enum AppType {
    APP_TYPE_HOST = 1,
    APP_TYPE_CHILD = 2
  };

  // Possible elements of the Browser window.
  enum WindowFeature {
    FEATURE_NONE = 0,
    FEATURE_TITLEBAR = 1,
    FEATURE_TABSTRIP = 2,
    FEATURE_TOOLBAR = 4,
    FEATURE_LOCATIONBAR = 8,
    FEATURE_BOOKMARKBAR = 16,
    FEATURE_INFOBAR = 32,
    FEATURE_DOWNLOADSHELF = 64
  };

  // The context for a download blocked notification from
  // OkToCloseWithInProgressDownloads.
  enum DownloadClosePreventionType {
    // Browser close is not blocked by download state.
    DOWNLOAD_CLOSE_OK,

    // The browser is shutting down and there are active downloads
    // that would be cancelled.
    DOWNLOAD_CLOSE_BROWSER_SHUTDOWN,

    // There are active downloads associated with this incognito profile
    // that would be canceled.
    DOWNLOAD_CLOSE_LAST_WINDOW_IN_INCOGNITO_PROFILE,
  };

  // Different types of action when web app info is available.
  // OnDidGetApplicationInfo uses this to dispatch calls.
  enum WebAppAction {
    NONE,             // No action at all.
    CREATE_SHORTCUT,  // Bring up create application shortcut dialog.
    UPDATE_SHORTCUT   // Update icon for app shortcut.
  };

  struct CreateParams {
    CreateParams();
    CreateParams(Type type, Profile* profile);

    static CreateParams CreateForApp(Type type,
                                     const std::string& app_name,
                                     const gfx::Rect& window_bounds,
                                     Profile* profile);

    static CreateParams CreateForDevTools(Profile* profile);

    // The browser type.
    Type type;

    // The associated profile.
    Profile* profile;

    // The application name that is also the name of the window to the shell.
    // This name should be set when:
    // 1) we launch an application via an application shortcut or extension API.
    // 2) we launch an undocked devtool window.
    std::string app_name;

    // Type of app (host or child). See description of AppType.
    AppType app_type;

    // The bounds of the window to open.
    gfx::Rect initial_bounds;

    ui::WindowShowState initial_show_state;

    bool is_session_restore;
  };

  // Constructors, Creation, Showing //////////////////////////////////////////

  // Creates a new browser of the given |type| and for the given |profile|. The
  // Browser has a NULL window after its construction, InitBrowserWindow must
  // be called after configuration for window() to be valid.
  // Avoid using this constructor directly if you can use one of the Create*()
  // methods below. This applies to almost all non-testing code.
  Browser(Type type, Profile* profile);
  virtual ~Browser();

  // Creates a normal tabbed browser with the specified profile. The Browser's
  // window is created by this function call.
  static Browser* Create(Profile* profile);

  // Like Create, but creates a browser of the specified parameters.
  static Browser* CreateWithParams(const CreateParams& params);

  // Set overrides for the initial window bounds and maximized state.
  void set_override_bounds(const gfx::Rect& bounds) {
    override_bounds_ = bounds;
  }
  ui::WindowShowState initial_show_state() const { return initial_show_state_; }
  void set_initial_show_state(ui::WindowShowState initial_show_state) {
    initial_show_state_ = initial_show_state;
  }
  // Return true if the initial window bounds have been overridden.
  bool bounds_overridden() const {
    return !override_bounds_.IsEmpty();
  }
  // Set indicator that this browser is being created via session restore.
  // This is used on the Mac (only) to determine animation style when the
  // browser window is shown.
  void set_is_session_restore(bool is_session_restore) {
    is_session_restore_ = is_session_restore;
  }
  bool is_session_restore() const {
    return is_session_restore_;
  }

  // Creates the Browser Window. Prefer to use the static helpers above where
  // possible. This does not show the window. You need to call window()->Show()
  // to show it.
  void InitBrowserWindow();

  // Sets the BrowserWindow. This is intended for tests only.
  // Use CreateBrowserWindow outside of testing, or the static convenience
  // methods that create a BrowserWindow for you.
  void SetWindowForTesting(BrowserWindow* window);

  // Accessors ////////////////////////////////////////////////////////////////

  Type type() const { return type_; }
  const std::string& app_name() const { return app_name_; }
  AppType app_type() const { return app_type_; }
  Profile* profile() const { return profile_; }
  gfx::Rect override_bounds() const { return override_bounds_; }

  // Returns the InstantController or NULL if there is no InstantController for
  // this Browser.
  InstantController* instant() const { return instant_.get(); }

  // |window()| will return NULL if called before |CreateBrowserWindow()|
  // is done.
  BrowserWindow* window() const { return window_; }
  ToolbarModel* toolbar_model() { return toolbar_model_.get(); }
  chrome::search::SearchModel* search_model() { return search_model_.get(); }
  const SessionID& session_id() const { return session_id_; }
  CommandUpdater* command_updater() { return &command_updater_; }
  bool block_command_execution() const { return block_command_execution_; }
  BrowserContentSettingBubbleModelDelegate*
      content_setting_bubble_model_delegate() {
    return content_setting_bubble_model_delegate_.get();
  }
  BrowserTabRestoreServiceDelegate* tab_restore_service_delegate() {
    return tab_restore_service_delegate_.get();
  }
  BrowserSyncedWindowDelegate* synced_window_delegate() {
    return synced_window_delegate_.get();
  }

  // Get the FindBarController for this browser, creating it if it does not
  // yet exist.
  FindBarController* GetFindBarController();

  // Returns true if a FindBarController exists for this browser.
  bool HasFindBarController() const;

  // Returns the state of the bookmark bar.
  BookmarkBar::State bookmark_bar_state() const { return bookmark_bar_state_; }

  // State Storage and Retrieval for UI ///////////////////////////////////////

  // Gets the Favicon of the page in the selected tab.
  SkBitmap GetCurrentPageIcon() const;

  // Gets the title of the window based on the selected tab's title.
  string16 GetWindowTitleForCurrentTab() const;

  // Prepares a title string for display (removes embedded newlines, etc).
  static void FormatTitleForDisplay(string16* title);

  // OnBeforeUnload handling //////////////////////////////////////////////////

  // Gives beforeunload handlers the chance to cancel the close.
  bool ShouldCloseWindow();

  bool IsAttemptingToCloseBrowser() const {
    return is_attempting_to_close_browser_;
  }

  // Invoked when the window containing us is closing. Performs the necessary
  // cleanup.
  void OnWindowClosing();

  // OnWindowActivationChanged handling ///////////////////////////////////////

  // Invoked when the window containing us is activated.
  void OnWindowActivated();

  // In-progress download termination handling /////////////////////////////////

  // Called when the user has decided whether to proceed or not with the browser
  // closure.  |cancel_downloads| is true if the downloads should be canceled
  // and the browser closed, false if the browser should stay open and the
  // downloads running.
  void InProgressDownloadResponse(bool cancel_downloads);

  // Indicates whether or not this browser window can be closed, or
  // would be blocked by in-progress downloads.
  // If executing downloads would be cancelled by this window close,
  // then |*num_downloads_blocking| is updated with how many downloads
  // would be canceled if the close continued.
  DownloadClosePreventionType OkToCloseWithInProgressDownloads(
      int* num_downloads_blocking) const;

  // TabStripModel pass-thrus /////////////////////////////////////////////////

  TabStripModel* tab_strip_model() const { return tab_strip_model_.get(); }

  int tab_count() const;
  int active_index() const;
  int GetIndexOfController(
      const content::NavigationController* controller) const;

  TabContents* GetActiveTabContents() const;
  // A convenient version of the above which returns the TabContents's
  // WebContents.
  content::WebContents* GetActiveWebContents() const;
  TabContents* GetTabContentsAt(int index) const;
  // A convenient version of the above which returns the TabContents's
  // WebContents.
  content::WebContents* GetWebContentsAt(int index) const;
  void ActivateTabAt(int index, bool user_gesture);
  bool IsTabPinned(int index) const;
  bool IsTabDiscarded(int index) const;
  void CloseAllTabs();

  // Tab adding/showing functions /////////////////////////////////////////////

  // Returns true if the tab strip is editable (for extensions).
  bool IsTabStripEditable() const;

  // Returns the index to insert a tab at during session restore and startup.
  // |relative_index| gives the index of the url into the number of tabs that
  // are going to be opened. For example, if three urls are passed in on the
  // command line this is invoked three times with the values 0, 1 and 2.
  int GetIndexForInsertionDuringRestore(int relative_index);

  // Adds a selected tab with the specified URL and transition, returns the
  // created TabContents.
  TabContents* AddSelectedTabWithURL(const GURL& url,
                                     content::PageTransition transition);

  // Add a new tab, given a TabContents. A WebContents appropriate to
  // display the last committed entry is created and returned.
  content::WebContents* AddTab(TabContents* tab_contents,
                               content::PageTransition type);

  // Add a tab with its session history restored from the SessionRestore
  // system. If select is true, the tab is selected. |tab_index| gives the index
  // to insert the tab at. |selected_navigation| is the index of the
  // TabNavigation in |navigations| to select. If |extension_app_id| is
  // non-empty the tab is an app tab and |extension_app_id| is the id of the
  // extension. If |pin| is true and |tab_index|/ is the last pinned tab, then
  // the newly created tab is pinned. If |from_last_session| is true,
  // |navigations| are from the previous session.
  content::WebContents* AddRestoredTab(
      const std::vector<TabNavigation>& navigations,
      int tab_index,
      int selected_navigation,
      const std::string& extension_app_id,
      bool select,
      bool pin,
      bool from_last_session,
      content::SessionStorageNamespace* storage_namespace);

  // Creates a new tab with the already-created WebContents 'new_contents'.
  // The window for the added contents will be reparented correctly when this
  // method returns.  If |disposition| is NEW_POPUP, |pos| should hold the
  // initial position.
  void AddWebContents(content::WebContents* new_contents,
                      WindowOpenDisposition disposition,
                      const gfx::Rect& initial_pos,
                      bool user_gesture);
  void CloseTabContents(content::WebContents* contents);

  // Replaces the state of the currently selected tab with the session
  // history restored from the SessionRestore system.
  void ReplaceRestoredTab(
      const std::vector<TabNavigation>& navigations,
      int selected_navigation,
      bool from_last_session,
      const std::string& extension_app_id,
      content::SessionStorageNamespace* session_storage_namespace);

  // Invoked when the fullscreen state of the window changes.
  // BrowserWindow::EnterFullscreen invokes this after the window has become
  // fullscreen.
  void WindowFullscreenStateChanged();

  // Assorted browser commands ////////////////////////////////////////////////

  // NOTE: Within each of the following sections, the IDs are ordered roughly by
  // how they appear in the GUI/menus (left to right, top to bottom, etc.).

  // In kiosk mode, the first toggle is valid, the rest is discarded.
  void ToggleFullscreenMode();
  // See the description of
  // FullscreenController::ToggleFullscreenModeWithExtension.
  void ToggleFullscreenModeWithExtension(const GURL& extension_url);
#if defined(OS_WIN)
  // See the description of FullscreenController::ToggleMetroSnapMode.
  void SetMetroSnapMode(bool enable);
#endif
#if defined(OS_MACOSX)
  void TogglePresentationMode();
#endif

  // Returns true if the Browser supports the specified feature. The value of
  // this varies during the lifetime of the browser. For example, if the window
  // is fullscreen this may return a different value. If you only care about
  // whether or not it's possible for the browser to support a particular
  // feature use |CanSupportWindowFeature|.
  bool SupportsWindowFeature(WindowFeature feature) const;

  // Returns true if the Browser can support the specified feature. See comment
  // in |SupportsWindowFeature| for details on this.
  bool CanSupportWindowFeature(WindowFeature feature) const;

  // TODO(port): port these, and re-merge the two function declaration lists.
  // Page-related commands.
  void ToggleEncodingAutoDetect();
  void OverrideEncoding(int encoding_id);

  // Show various bits of UI
  void OpenFile();
  void OpenCreateShortcutsDialog();

  void UpdateDownloadShelfVisibility(bool visible);

  // Commits the current instant, returning true on success. This is intended
  // for use from OpenCurrentURL.
  bool OpenInstant(WindowOpenDisposition disposition);

  /////////////////////////////////////////////////////////////////////////////

  // Helper function to run unload listeners on a WebContents.
  static bool RunUnloadEventsHelper(content::WebContents* contents);

  // Helper function to handle JS out of memory notifications
  static void JSOutOfMemoryHelper(content::WebContents* web_contents);

  // Helper function to register a protocol handler.
  static void RegisterProtocolHandlerHelper(content::WebContents* web_contents,
                                            const std::string& protocol,
                                            const GURL& url,
                                            const string16& title,
                                            bool user_gesture);

  // Helper function to register an intent handler.
  // |data| is the registered handler data. |user_gesture| is true if the call
  // was made in the context of a user gesture.
  static void RegisterIntentHandlerHelper(
      content::WebContents* web_contents,
      const webkit_glue::WebIntentServiceData& data,
      bool user_gesture);

  // Helper function to handle find results.
  static void FindReplyHelper(content::WebContents* web_contents,
                              int request_id,
                              int number_of_matches,
                              const gfx::Rect& selection_rect,
                              int active_match_ordinal,
                              bool final_update);

  // Calls ExecuteCommandWithDisposition with CURRENT_TAB disposition.
  void ExecuteCommand(int id);

  // Calls ExecuteCommandWithDisposition with the given event flags.
  void ExecuteCommand(int id, int event_flags);

  // Executes a command if it's enabled.
  // Returns true if the command is executed.
  bool ExecuteCommandIfEnabled(int id);

  // Returns true if |command_id| is a reserved command whose keyboard shortcuts
  // should not be sent to the renderer or |event| was triggered by a key that
  // we never want to send to the renderer.
  bool IsReservedCommandOrKey(int command_id,
                              const content::NativeWebKeyboardEvent& event);

  // Sets if command execution shall be blocked. If |block| is true then
  // following calls to ExecuteCommand() or ExecuteCommandWithDisposition()
  // method will not execute the command, and the last blocked command will be
  // recorded for retrieval.
  void SetBlockCommandExecution(bool block);

  // Gets the last blocked command after calling SetBlockCommandExecution(true).
  // Returns the command id or -1 if there is no command blocked. The
  // disposition type of the command will be stored in |*disposition| if it's
  // not null.
  int GetLastBlockedCommand(WindowOpenDisposition* disposition);

  // Called by browser::Navigate() when a navigation has occurred in a tab in
  // this Browser. Updates the UI for the start of this navigation.
  void UpdateUIForNavigationInTab(TabContents* contents,
                                  content::PageTransition transition,
                                  bool user_initiated);

  // Interface implementations ////////////////////////////////////////////////

  // Overridden from content::PageNavigator:
  virtual content::WebContents* OpenURL(
      const content::OpenURLParams& params) OVERRIDE;

  // Overridden from CommandUpdater::CommandUpdaterDelegate:
  virtual void ExecuteCommandWithDisposition(
      int id,
      WindowOpenDisposition disposition) OVERRIDE;

  // Overridden from TabRestoreServiceObserver:
  virtual void TabRestoreServiceChanged(TabRestoreService* service) OVERRIDE;
  virtual void TabRestoreServiceDestroyed(TabRestoreService* service) OVERRIDE;

  // Centralized method for creating a TabContents, configuring and
  // installing all its supporting objects and observers.
  static TabContents* TabContentsFactory(
      Profile* profile,
      content::SiteInstance* site_instance,
      int routing_id,
      const content::WebContents* base_web_contents,
      content::SessionStorageNamespace* session_storage_namespace);

  // Overridden from TabStripModelDelegate:
  virtual TabContents* AddBlankTab(bool foreground) OVERRIDE;
  virtual TabContents* AddBlankTabAt(int index,
                                     bool foreground) OVERRIDE;
  virtual Browser* CreateNewStripWithContents(
      TabContents* detached_contents,
      const gfx::Rect& window_bounds,
      const DockInfo& dock_info,
      bool maximize) OVERRIDE;
  virtual int GetDragActions() const OVERRIDE;
  // Construct a TabContents for a given URL, profile and transition type. If
  // instance is not null, its process will be used to render the tab.
  virtual TabContents* CreateTabContentsForURL(
      const GURL& url,
      const content::Referrer& referrer,
      Profile* profile,
      content::PageTransition transition,
      bool defer_load,
      content::SiteInstance* instance) const OVERRIDE;
  virtual bool CanDuplicateContentsAt(int index) OVERRIDE;
  virtual void DuplicateContentsAt(int index) OVERRIDE;
  virtual void CloseFrameAfterDragSession() OVERRIDE;
  virtual void CreateHistoricalTab(TabContents* contents) OVERRIDE;
  virtual bool RunUnloadListenerBeforeClosing(TabContents* contents) OVERRIDE;
  virtual bool CanBookmarkAllTabs() const OVERRIDE;
  virtual void BookmarkAllTabs() OVERRIDE;
  virtual bool CanRestoreTab() OVERRIDE;
  virtual void RestoreTab() OVERRIDE;

  // Overridden from TabStripModelObserver:
  virtual void TabInsertedAt(TabContents* contents,
                             int index,
                             bool foreground) OVERRIDE;
  virtual void TabClosingAt(TabStripModel* tab_strip_model,
                            TabContents* contents,
                            int index) OVERRIDE;
  virtual void TabDetachedAt(TabContents* contents, int index) OVERRIDE;
  virtual void TabDeactivated(TabContents* contents) OVERRIDE;
  virtual void ActiveTabChanged(TabContents* old_contents,
                                TabContents* new_contents,
                                int index,
                                bool user_gesture) OVERRIDE;
  virtual void TabMoved(TabContents* contents,
                        int from_index,
                        int to_index) OVERRIDE;
  virtual void TabReplacedAt(TabStripModel* tab_strip_model,
                             TabContents* old_contents,
                             TabContents* new_contents,
                             int index) OVERRIDE;
  virtual void TabPinnedStateChanged(TabContents* contents,
                                     int index) OVERRIDE;
  virtual void TabStripEmpty() OVERRIDE;

  // Overridden from content::WebContentsDelegate:
  virtual bool PreHandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event,
      bool* is_keyboard_shortcut) OVERRIDE;
  virtual void HandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) OVERRIDE;

  // Fullscreen permission infobar callbacks.
  // TODO(koz): Remove this and have callers call FullscreenController directly.
  void OnAcceptFullscreenPermission(const GURL& url,
                                    FullscreenExitBubbleType bubble_type);
  void OnDenyFullscreenPermission(FullscreenExitBubbleType bubble_type);

  // Figure out if there are tabs that have beforeunload handlers.
  bool TabsNeedBeforeUnloadFired();

  bool is_type_tabbed() const { return type_ == TYPE_TABBED; }
  bool is_type_popup() const { return type_ == TYPE_POPUP; }
  bool is_type_panel() const { return type_ == TYPE_PANEL; }

  bool is_app() const;
  bool is_devtools() const;

  // See FullscreenController::IsFullscreenForTabOrPending.
  bool IsFullscreenForTabOrPending() const;

  // True when the mouse cursor is locked.
  bool IsMouseLocked() const;

  // Called each time the browser window is shown.
  void OnWindowDidShow();

  // Show the first run search engine bubble on the location bar.
  void ShowFirstRunBubble();

  void set_pending_web_app_action(WebAppAction action) {
    pending_web_app_action_ = action;
  }

  ExtensionWindowController* extension_window_controller() const {
    return extension_window_controller_.get();
  }

 protected:
  // Funnel for the factory method in BrowserWindow. This allows subclasses to
  // set their own window.
  virtual BrowserWindow* CreateBrowserWindow();

 private:
  friend class BrowserTest;
  friend class FullscreenControllerTest;
  FRIEND_TEST_ALL_PREFIXES(AppModeTest, EnableAppModeTest);
  FRIEND_TEST_ALL_PREFIXES(BrowserTest, NoTabsInPopups);
  FRIEND_TEST_ALL_PREFIXES(BrowserTest, ConvertTabToAppShortcut);
  FRIEND_TEST_ALL_PREFIXES(BrowserTest, OpenAppWindowLikeNtp);
  FRIEND_TEST_ALL_PREFIXES(BrowserTest, AppIdSwitch);
  FRIEND_TEST_ALL_PREFIXES(FullscreenControllerTest,
                           TabEntersPresentationModeFromWindowed);
  FRIEND_TEST_ALL_PREFIXES(FullscreenExitBubbleControllerTest,
                           DenyExitsFullscreen);
  FRIEND_TEST_ALL_PREFIXES(StartupBrowserCreatorTest, OpenAppShortcutNoPref);
  FRIEND_TEST_ALL_PREFIXES(StartupBrowserCreatorTest,
                           OpenAppShortcutWindowPref);
  FRIEND_TEST_ALL_PREFIXES(StartupBrowserCreatorTest, OpenAppShortcutTabPref);
  FRIEND_TEST_ALL_PREFIXES(StartupBrowserCreatorTest, OpenAppShortcutPanel);

  // Used to describe why a tab is being detached. This is used by
  // TabDetachedAtImpl.
  enum DetachType {
    // Result of TabDetachedAt.
    DETACH_TYPE_DETACH,

    // Result of TabReplacedAt.
    DETACH_TYPE_REPLACE,

    // Result of the tab strip not having any significant tabs.
    DETACH_TYPE_EMPTY
  };

  // Describes where the bookmark bar state change originated from.
  enum BookmarkBarStateChangeReason {
    // From the constructor.
    BOOKMARK_BAR_STATE_CHANGE_INIT,

    // Change is the result of the active tab changing.
    BOOKMARK_BAR_STATE_CHANGE_TAB_SWITCH,

    // Change is the result of the bookmark bar pref changing.
    BOOKMARK_BAR_STATE_CHANGE_PREF_CHANGE,

    // Change is the result of a state change in the active tab.
    BOOKMARK_BAR_STATE_CHANGE_TAB_STATE,

    // Change is the result of window toggling in/out of fullscreen mode.
    BOOKMARK_BAR_STATE_CHANGE_TOGGLE_FULLSCREEN,
  };

  enum FullScreenMode {
    // Not in fullscreen mode.
    FULLSCREEN_DISABLED,

    // Fullscreen mode, occupying the whole screen.
    FULLSCREEN_NORMAL,

    // Fullscreen mode for metro snap, occupying the full height and 20% of
    // the screen width.
    FULLSCREEN_METRO_SNAP,
  };

  // Overridden from content::WebContentsDelegate:
  virtual content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) OVERRIDE;
  virtual void NavigationStateChanged(const content::WebContents* source,
                                      unsigned changed_flags) OVERRIDE;
  virtual void AddNewContents(content::WebContents* source,
                              content::WebContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture) OVERRIDE;
  virtual void ActivateContents(content::WebContents* contents) OVERRIDE;
  virtual void DeactivateContents(content::WebContents* contents) OVERRIDE;
  virtual void LoadingStateChanged(content::WebContents* source) OVERRIDE;
  virtual void CloseContents(content::WebContents* source) OVERRIDE;
  virtual void MoveContents(content::WebContents* source,
                            const gfx::Rect& pos) OVERRIDE;
  virtual void DetachContents(content::WebContents* source) OVERRIDE;
  virtual bool IsPopupOrPanel(
      const content::WebContents* source) const OVERRIDE;
  virtual void UpdateTargetURL(content::WebContents* source, int32 page_id,
                               const GURL& url) OVERRIDE;
  virtual void ContentsMouseEvent(content::WebContents* source,
                                  const gfx::Point& location,
                                  bool motion) OVERRIDE;
  virtual void ContentsZoomChange(bool zoom_in) OVERRIDE;
  virtual void WebContentsFocused(content::WebContents* content) OVERRIDE;
  virtual bool TakeFocus(bool reverse) OVERRIDE;
  virtual bool IsApplication() const OVERRIDE;
  virtual void ConvertContentsToApplication(
      content::WebContents* source) OVERRIDE;
  virtual gfx::Rect GetRootWindowResizerRect() const OVERRIDE;
  virtual void BeforeUnloadFired(content::WebContents* source,
                                 bool proceed,
                                 bool* proceed_to_fire_unload) OVERRIDE;
  virtual void SetFocusToLocationBar(bool select_all) OVERRIDE;
  virtual void RenderWidgetShowing() OVERRIDE;
  virtual int GetExtraRenderViewHeight() const OVERRIDE;
  virtual void OnStartDownload(content::WebContents* source,
                               content::DownloadItem* download) OVERRIDE;
  virtual void ViewSourceForTab(content::WebContents* source,
                                const GURL& page_url) OVERRIDE;
  virtual void ViewSourceForFrame(
      content::WebContents* source,
      const GURL& frame_url,
      const std::string& frame_content_state) OVERRIDE;
  virtual void ShowRepostFormWarningDialog(
      content::WebContents* source) OVERRIDE;
  virtual bool ShouldAddNavigationToHistory(
      const history::HistoryAddPageArgs& add_page_args,
      content::NavigationType navigation_type) OVERRIDE;
  virtual bool ShouldCreateWebContents(
      content::WebContents* web_contents,
      int route_id,
      WindowContainerType window_container_type,
      const string16& frame_name,
      const GURL& target_url) OVERRIDE;
  virtual void WebContentsCreated(content::WebContents* source_contents,
                                  int64 source_frame_id,
                                  const GURL& target_url,
                                  content::WebContents* new_contents) OVERRIDE;
  virtual void ContentRestrictionsChanged(
      content::WebContents* source) OVERRIDE;
  virtual void RendererUnresponsive(content::WebContents* source) OVERRIDE;
  virtual void RendererResponsive(content::WebContents* source) OVERRIDE;
  virtual void WorkerCrashed(content::WebContents* source) OVERRIDE;
  virtual void DidNavigateMainFramePostCommit(
      content::WebContents* web_contents) OVERRIDE;
  virtual void DidNavigateToPendingEntry(
      content::WebContents* web_contents) OVERRIDE;
  virtual content::JavaScriptDialogCreator*
      GetJavaScriptDialogCreator() OVERRIDE;
  virtual content::ColorChooser* OpenColorChooser(
      content::WebContents* web_contents,
      int color_chooser_id,
      SkColor color) OVERRIDE;
  virtual void DidEndColorChooser() OVERRIDE;
  virtual void RunFileChooser(
      content::WebContents* web_contents,
      const content::FileChooserParams& params) OVERRIDE;
  virtual void EnumerateDirectory(content::WebContents* web_contents,
                                  int request_id,
                                  const FilePath& path) OVERRIDE;
  virtual void ToggleFullscreenModeForTab(content::WebContents* web_contents,
      bool enter_fullscreen) OVERRIDE;
  virtual bool IsFullscreenForTabOrPending(
      const content::WebContents* web_contents) const OVERRIDE;
  virtual void JSOutOfMemory(content::WebContents* web_contents) OVERRIDE;
  virtual void RegisterProtocolHandler(content::WebContents* web_contents,
                                       const std::string& protocol,
                                       const GURL& url,
                                       const string16& title,
                                       bool user_gesture) OVERRIDE;
  virtual void RegisterIntentHandler(
      content::WebContents* web_contents,
      const webkit_glue::WebIntentServiceData& data,
      bool user_gesture) OVERRIDE;
  virtual void WebIntentDispatch(
      content::WebContents* web_contents,
      content::WebIntentsDispatcher* intents_dispatcher) OVERRIDE;
  virtual void UpdatePreferredSize(content::WebContents* source,
                                   const gfx::Size& pref_size) OVERRIDE;
  virtual void ResizeDueToAutoResize(content::WebContents* source,
                                     const gfx::Size& new_size) OVERRIDE;
  virtual void FindReply(content::WebContents* web_contents,
                         int request_id,
                         int number_of_matches,
                         const gfx::Rect& selection_rect,
                         int active_match_ordinal,
                         bool final_update) OVERRIDE;
  virtual void RequestToLockMouse(content::WebContents* web_contents,
                                  bool user_gesture,
                                  bool last_unlocked_by_target) OVERRIDE;
  virtual void LostMouseLock() OVERRIDE;
  virtual void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest* request,
      const content::MediaResponseCallback& callback) OVERRIDE;

  // Overridden from CoreTabHelperDelegate:
  // Note that the caller is responsible for deleting |old_tab_contents|.
  virtual void SwapTabContents(TabContents* old_tab_contents,
                               TabContents* new_tab_contents) OVERRIDE;
  virtual bool CanReloadContents(TabContents* source) const OVERRIDE;
  virtual bool CanSaveContents(TabContents* source) const OVERRIDE;

  // Overridden from SearchEngineTabHelperDelegate:
  virtual void ConfirmAddSearchProvider(TemplateURL* template_url,
                                        Profile* profile) OVERRIDE;

  // Overridden from ConstrainedWindowTabHelperDelegate:
  virtual void SetTabContentBlocked(TabContents* contents,
                                    bool blocked) OVERRIDE;

  // Overridden from BlockedContentTabHelperDelegate:
  virtual TabContents* GetConstrainingTabContents(TabContents* source) OVERRIDE;

  // Overridden from BookmarkTabHelperDelegate:
  virtual void URLStarredChanged(TabContents* source,
                                 bool starred) OVERRIDE;

  // Overridden from ZoomObserver:
  virtual void OnZoomIconChanged(TabContents* source,
                                 ZoomController::ZoomIconState state) OVERRIDE;
  virtual void OnZoomChanged(TabContents* source,
                             int zoom_percent,
                             bool can_show_bubble) OVERRIDE;

  // Overridden from ExtensionTabHelperDelegate:
  virtual void OnDidGetApplicationInfo(TabContents* source,
                                       int32 page_id) OVERRIDE;
  virtual void OnInstallApplication(
      TabContents* source,
      const WebApplicationInfo& app_info) OVERRIDE;

  // Overridden from SelectFileDialog::Listener:
  virtual void FileSelected(const FilePath& path,
                            int index,
                            void* params) OVERRIDE;

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Overridden from ProfileSyncServiceObserver:
  virtual void OnStateChanged() OVERRIDE;

  // Overriden from InstantDelegate:
  virtual void ShowInstant(TabContents* preview_contents) OVERRIDE;
  virtual void HideInstant() OVERRIDE;
  virtual void CommitInstant(TabContents* preview_contents) OVERRIDE;
  virtual void SetSuggestedText(const string16& text,
                                InstantCompleteBehavior behavior) OVERRIDE;
  virtual gfx::Rect GetInstantBounds() OVERRIDE;
  virtual void InstantPreviewFocused() OVERRIDE;
  virtual TabContents* GetInstantHostTabContents() const OVERRIDE;

  // Command and state updating ///////////////////////////////////////////////

  // Returns true if the regular Chrome UI (not the fullscreen one and
  // not the single-tab one) is shown. Used for updating window command states
  // only. Consider using SupportsWindowFeature if you need the mentioned
  // functionality anywhere else.
  bool IsShowingMainUI(bool is_fullscreen);

  // Initialize state for all browser commands.
  void InitCommandState();

  // Update commands whose state depends on incognito mode availability.
  void UpdateCommandsForIncognitoAvailability();

  // Update commands whose state depends on the tab's state.
  void UpdateCommandsForTabState();

  // Updates commands when the content's restrictions change.
  void UpdateCommandsForContentRestrictionState();

  // Updates commands for enabling developer tools.
  void UpdateCommandsForDevTools();

  // Updates commands for bookmark editing.
  void UpdateCommandsForBookmarkEditing();

  // Updates commands that affect the bookmark bar.
  void UpdateCommandsForBookmarkBar();

  // Set the preference that indicates that the home page has been changed.
  void MarkHomePageAsChanged(PrefService* pref_service);

  // Update commands whose state depends on the type of fullscreen mode the
  // window is in.
  void UpdateCommandsForFullscreenMode(FullScreenMode fullscreen_mode);

  // Update commands whose state depends on whether multiple profiles are
  // allowed.
  void UpdateCommandsForMultipleProfiles();

  // Updates the printing command state.
  void UpdatePrintingState(int content_restrictions);

  // Updates the save-page-as command state.
  void UpdateSaveAsState(int content_restrictions);

  // Updates the open-file state (Mac Only).
  void UpdateOpenFileState();

  // Ask the Reload/Stop button to change its icon, and update the Stop command
  // state.  |is_loading| is true if the current WebContents is loading.
  // |force| is true if the button should change its icon immediately.
  void UpdateReloadStopState(bool is_loading, bool force);

  // UI update coalescing and handling ////////////////////////////////////////

  // Asks the toolbar (and as such the location bar) to update its state to
  // reflect the current tab's current URL, security state, etc.
  // If |should_restore_state| is true, we're switching (back?) to this tab and
  // should restore any previous location bar state (such as user editing) as
  // well.
  void UpdateToolbar(bool should_restore_state);

  // Updates the browser's search model with the tab's search model.
  void UpdateSearchState(TabContents* contents);

  // Does one or both of the following for each bit in |changed_flags|:
  // . If the update should be processed immediately, it is.
  // . If the update should processed asynchronously (to avoid lots of ui
  //   updates), then scheduled_updates_ is updated for the |source| and update
  //   pair and a task is scheduled (assuming it isn't running already)
  //   that invokes ProcessPendingUIUpdates.
  void ScheduleUIUpdate(const content::WebContents* source,
                        unsigned changed_flags);

  // Processes all pending updates to the UI that have been scheduled by
  // ScheduleUIUpdate in scheduled_updates_.
  void ProcessPendingUIUpdates();

  // Removes all entries from scheduled_updates_ whose source is contents.
  void RemoveScheduledUpdatesFor(content::WebContents* contents);

  // Getters for UI ///////////////////////////////////////////////////////////

  // TODO(beng): remove, and provide AutomationProvider a better way to access
  //             the LocationBarView's edit.
  friend class AutomationProvider;
  friend class BrowserProxy;
  friend class TestingAutomationProvider;

  // Returns the StatusBubble from the current toolbar. It is possible for
  // this to return NULL if called before the toolbar has initialized.
  // TODO(beng): remove this.
  StatusBubble* GetStatusBubble();

  // Session restore functions ////////////////////////////////////////////////

  // Notifies the history database of the index for all tabs whose index is
  // >= index.
  void SyncHistoryWithTabs(int index);

  // OnBeforeUnload handling //////////////////////////////////////////////////

  typedef std::set<content::WebContents*> UnloadListenerSet;

  // Processes the next tab that needs it's beforeunload/unload event fired.
  void ProcessPendingTabs();

  // Whether we've completed firing all the tabs' beforeunload/unload events.
  bool HasCompletedUnloadProcessing() const;

  // Clears all the state associated with processing tabs' beforeunload/unload
  // events since the user cancelled closing the window.
  void CancelWindowClose();

  // Removes |web_contents| from the passed |set|.
  // Returns whether the tab was in the set in the first place.
  // TODO(beng): this method needs a better name!
  bool RemoveFromSet(UnloadListenerSet* set,
                     content::WebContents* web_contents);

  // Cleans up state appropriately when we are trying to close the browser and
  // the tab has finished firing its unload handler. We also use this in the
  // cases where a tab crashes or hangs even if the beforeunload/unload haven't
  // successfully fired. If |process_now| is true |ProcessPendingTabs| is
  // invoked immediately, otherwise it is invoked after a delay (PostTask).
  //
  // Typically you'll want to pass in true for |process_now|. Passing in true
  // may result in deleting |tab|. If you know that shouldn't happen (because of
  // the state of the stack), pass in false.
  void ClearUnloadState(content::WebContents* web_contents, bool process_now);

  // In-progress download termination handling /////////////////////////////////

  // Called when the window is closing to check if potential in-progress
  // downloads should prevent it from closing.
  // Returns true if the window can close, false otherwise.
  bool CanCloseWithInProgressDownloads();

  // Assorted utility functions ///////////////////////////////////////////////

  // Sets the delegate of all the parts of the TabContents that
  // are needed.
  void SetAsDelegate(TabContents* tab, Browser* delegate);

  // Shows the Find Bar, optionally selecting the next entry that matches the
  // existing search string for that Tab. |forward_direction| controls the
  // search direction.
  void FindInPage(bool find_next, bool forward_direction);

  // Closes the frame.
  // TODO(beng): figure out if we need this now that the frame itself closes
  //             after a return to the message loop.
  void CloseFrame();

  void TabDetachedAtImpl(TabContents* contents, int index, DetachType type);

  // Shared code between Reload() and ReloadIgnoringCache().
  void ReloadInternal(WindowOpenDisposition disposition, bool ignore_cache);

  // Depending on the disposition, return the current tab or a clone of the
  // current tab.
  content::WebContents* GetOrCloneTabForDisposition(
      WindowOpenDisposition disposition);

  // Implementation of SupportsWindowFeature and CanSupportWindowFeature. If
  // |check_fullscreen| is true, the set of features reflect the actual state of
  // the browser, otherwise the set of features reflect the possible state of
  // the browser.
  bool SupportsWindowFeatureImpl(WindowFeature feature,
                                 bool check_fullscreen) const;

  // If this browser should have instant one is created, otherwise does nothing.
  void CreateInstantIfNecessary();

  // Retrieves the content restrictions for the currently selected tab.
  // Returns 0 if no tab selected, which is equivalent to no content
  // restrictions active.
  int GetContentRestrictionsForSelectedTab();

  // Resets |bookmark_bar_state_| based on the active tab. Notifies the
  // BrowserWindow if necessary.
  void UpdateBookmarkBarState(BookmarkBarStateChangeReason reason);

  // Creates a BackgroundContents if appropriate; return true if one was
  // created.
  bool MaybeCreateBackgroundContents(int route_id,
                                     content::WebContents* opener_web_contents,
                                     const string16& frame_name,
                                     const GURL& target_url);

  // Data members /////////////////////////////////////////////////////////////

  content::NotificationRegistrar registrar_;

  PrefChangeRegistrar profile_pref_registrar_;

  PrefChangeRegistrar local_pref_registrar_;

  // This Browser's type.
  const Type type_;

  // This Browser's profile.
  Profile* const profile_;

  // This Browser's window.
  BrowserWindow* window_;

  scoped_ptr<TabStripModel> tab_strip_model_;

  // The CommandUpdater that manages the browser window commands.
  CommandUpdater command_updater_;

  // The application name that is also the name of the window to the shell.
  // This name should be set when:
  // 1) we launch an application via an application shortcut or extension API.
  // 2) we launch an undocked devtool window.
  std::string app_name_;

  // Type of app (host or child). See description of AppType.
  AppType app_type_;

  // Unique identifier of this browser for session restore. This id is only
  // unique within the current session, and is not guaranteed to be unique
  // across sessions.
  const SessionID session_id_;

  // The model for the toolbar view.
  scoped_ptr<ToolbarModel> toolbar_model_;

  // The model for the "active" search state.  There are per-tab search models
  // as well.  When a tab is active its model is kept in sync with this one.
  // When a new tab is activated its model state is propagated to this active
  // model.  This way, observers only have to attach to this single model for
  // updates, and don't have to worry about active tab changes directly.
  scoped_ptr<chrome::search::SearchModel> search_model_;

  // UI update coalescing and handling ////////////////////////////////////////

  typedef std::map<const content::WebContents*, int> UpdateMap;

  // Maps from WebContents to pending UI updates that need to be processed.
  // We don't update things like the URL or tab title right away to avoid
  // flickering and extra painting.
  // See ScheduleUIUpdate and ProcessPendingUIUpdates.
  UpdateMap scheduled_updates_;

  // The following factory is used for chrome update coalescing.
  base::WeakPtrFactory<Browser> chrome_updater_factory_;

  // OnBeforeUnload handling //////////////////////////////////////////////////

  // Tracks tabs that need there beforeunload event fired before we can
  // close the browser. Only gets populated when we try to close the browser.
  UnloadListenerSet tabs_needing_before_unload_fired_;

  // Tracks tabs that need there unload event fired before we can
  // close the browser. Only gets populated when we try to close the browser.
  UnloadListenerSet tabs_needing_unload_fired_;

  // Whether we are processing the beforeunload and unload events of each tab
  // in preparation for closing the browser.
  bool is_attempting_to_close_browser_;

  // In-progress download termination handling /////////////////////////////////

  enum CancelDownloadConfirmationState {
    NOT_PROMPTED,          // We have not asked the user.
    WAITING_FOR_RESPONSE,  // We have asked the user and have not received a
                           // reponse yet.
    RESPONSE_RECEIVED      // The user was prompted and made a decision already.
  };

  // State used to figure-out whether we should prompt the user for confirmation
  // when the browser is closed with in-progress downloads.
  CancelDownloadConfirmationState cancel_download_confirmation_state_;

  /////////////////////////////////////////////////////////////////////////////

  // Override values for the bounds of the window and its maximized or minimized
  // state.
  // These are supplied by callers that don't want to use the default values.
  // The default values are typically loaded from local state (last session),
  // obtained from the last window of the same type, or obtained from the
  // shell shortcut's startup info.
  gfx::Rect override_bounds_;
  ui::WindowShowState initial_show_state_;

  // Tracks when this browser is being created by session restore.
  bool is_session_restore_;

  // The following factory is used to close the frame at a later time.
  base::WeakPtrFactory<Browser> weak_factory_;

  // The Find Bar. This may be NULL if there is no Find Bar, and if it is
  // non-NULL, it may or may not be visible.
  scoped_ptr<FindBarController> find_bar_controller_;

  // Dialog box used for opening and saving files.
  scoped_refptr<SelectFileDialog> select_file_dialog_;

  // Keep track of the encoding auto detect pref.
  BooleanPrefMember encoding_auto_detect_;

  // Indicates if command execution is blocked.
  bool block_command_execution_;

  // Stores the last blocked command id when |block_command_execution_| is true.
  int last_blocked_command_id_;

  // Stores the disposition type of the last blocked command.
  WindowOpenDisposition last_blocked_command_disposition_;

  // Which deferred action to perform when OnDidGetApplicationInfo is notified
  // from a WebContents. Currently, only one pending action is allowed.
  WebAppAction pending_web_app_action_;

  // The profile's tab restore service. The service is owned by the profile,
  // and we install ourselves as an observer.
  TabRestoreService* tab_restore_service_;

  // Helper which implements the ContentSettingBubbleModel interface.
  scoped_ptr<BrowserContentSettingBubbleModelDelegate>
      content_setting_bubble_model_delegate_;

  // Helper which implements the ToolbarModelDelegate interface.
  scoped_ptr<BrowserToolbarModelDelegate> toolbar_model_delegate_;

  // A delegate that handles the details of updating the "active"
  // |search_model_| state with the tab's state.
  scoped_ptr<chrome::search::SearchDelegate> search_delegate_;

  // Helper which implements the TabRestoreServiceDelegate interface.
  scoped_ptr<BrowserTabRestoreServiceDelegate> tab_restore_service_delegate_;

  // Helper which implements the SyncedWindowDelegate interface.
  scoped_ptr<BrowserSyncedWindowDelegate> synced_window_delegate_;

  scoped_ptr<InstantController> instant_;
  scoped_ptr<InstantUnloadHandler> instant_unload_handler_;

  BookmarkBar::State bookmark_bar_state_;

  scoped_refptr<FullscreenController> fullscreen_controller_;

  scoped_ptr<ExtensionWindowController> extension_window_controller_;

  // True if the browser window has been shown at least once.
  bool window_has_shown_;

  // Currently open color chooser. Non-NULL after OpenColorChooser is called and
  // before DidEndColorChooser is called.
  scoped_ptr<content::ColorChooser> color_chooser_;

  DISALLOW_COPY_AND_ASSIGN(Browser);
};

#endif  // CHROME_BROWSER_UI_BROWSER_H_
