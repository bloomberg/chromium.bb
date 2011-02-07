// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
#include "base/scoped_ptr.h"
#include "base/string16.h"
#include "base/task.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/debugger/devtools_toggle_action.h"
#include "chrome/browser/instant/instant_delegate.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/sessions/session_id.h"
#include "chrome/browser/sessions/tab_restore_service_observer.h"
#include "chrome/browser/shell_dialogs.h"
#include "chrome/browser/sync/profile_sync_service_observer.h"
#include "chrome/browser/tabs/tab_handler.h"
#include "chrome/browser/tabs/tab_strip_model_delegate.h"   // TODO(beng): remove
#include "chrome/browser/tabs/tab_strip_model_observer.h"   // TODO(beng): remove
#include "chrome/browser/tab_contents/page_navigator.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper_delegate.h"
#include "chrome/browser/ui/toolbar/toolbar_model.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/page_transition_types.h"
#include "chrome/common/page_zoom.h"
#include "ui/gfx/rect.h"

class BrowserWindow;
class Extension;
class FindBarController;
class InstantController;
class InstantUnloadHandler;
class PrefService;
class Profile;
class SessionStorageNamespace;
class SkBitmap;
class StatusBubble;
class TabNavigation;
class TabStripModel;
struct WebApplicationInfo;
namespace gfx {
class Point;
}

class Browser : public TabHandlerDelegate,
                public TabContentsDelegate,
                public TabContentsWrapperDelegate,
                public PageNavigator,
                public CommandUpdater::CommandUpdaterDelegate,
                public NotificationObserver,
                public SelectFileDialog::Listener,
                public TabRestoreServiceObserver,
                public ProfileSyncServiceObserver,
                public InstantDelegate {
 public:
  // SessionService::WindowType mirrors these values.  If you add to this
  // enum, look at SessionService::WindowType to see if it needs to be
  // updated.
  enum Type {
    TYPE_NORMAL = 1,
    TYPE_POPUP = 2,
    // The old-style app created via "Create application shortcuts".
    // Shortcuts to a URL and shortcuts to an installed application
    // both have this type.
    TYPE_APP = 4,
    TYPE_APP_POPUP = TYPE_APP | TYPE_POPUP,
    TYPE_DEVTOOLS = TYPE_APP | 8,

    // TODO(skerner): crbug/56776: Until the panel UI is complete on all
    // platforms, apps that set app.launch.container = "panel" have type
    // APP_POPUP. (see Browser::CreateForApp)
    // NOTE: TYPE_APP_PANEL is a superset of TYPE_APP_POPUP.
    TYPE_APP_PANEL = TYPE_APP | TYPE_POPUP | 16,
    TYPE_ANY = TYPE_NORMAL |
               TYPE_POPUP |
               TYPE_APP |
               TYPE_DEVTOOLS |
               TYPE_APP_PANEL
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
    FEATURE_SIDEBAR = 64,
    FEATURE_DOWNLOADSHELF = 128
  };

  // Maximized state on creation.
  enum MaximizedState {
    // The maximized state is set to the default, which varies depending upon
    // what the user has done.
    MAXIMIZED_STATE_DEFAULT,

    // Maximized state is explicitly maximized.
    MAXIMIZED_STATE_MAXIMIZED,

    // Maximized state is explicitly not maximized (normal).
    MAXIMIZED_STATE_UNMAXIMIZED
  };

  // Constructors, Creation, Showing //////////////////////////////////////////

  // Creates a new browser of the given |type| and for the given |profile|. The
  // Browser has a NULL window after its construction, CreateBrowserWindow must
  // be called after configuration for window() to be valid.
  // Avoid using this constructor directly if you can use one of the Create*()
  // methods below. This applies to almost all non-testing code.
  Browser(Type type, Profile* profile);
  virtual ~Browser();

  // Creates a normal tabbed browser with the specified profile. The Browser's
  // window is created by this function call.
  static Browser* Create(Profile* profile);

  // Like Create, but creates a browser of the specified (popup) type, with the
  // specified contents, in a popup window of the specified size/position.
  static Browser* CreateForPopup(Type type, Profile* profile,
                                 TabContents* new_contents,
                                 const gfx::Rect& initial_bounds);

  // Like Create, but creates a browser of the specified type.
  static Browser* CreateForType(Type type, Profile* profile);

  // Like Create, but creates a toolbar-less "app" window for the specified
  // app. |app_name| is required and is used to identify the window to the
  // shell.  If |extension| is set, it is used to determine the size of the
  // window to open.
  static Browser* CreateForApp(const std::string& app_name,
                               const gfx::Size& window_size,
                               Profile* profile,
                               bool is_panel);

  // Like Create, but creates a tabstrip-less and toolbar-less
  // DevTools "app" window.
  static Browser* CreateForDevTools(Profile* profile);

  // Set overrides for the initial window bounds and maximized state.
  void set_override_bounds(const gfx::Rect& bounds) {
    override_bounds_ = bounds;
  }
  void set_maximized_state(MaximizedState state) {
    maximized_state_ = state;
  }
  // Return true if the initial window bounds have been overridden.
  bool bounds_overridden() const {
    return !override_bounds_.IsEmpty();
  }

  // Creates the Browser Window. Prefer to use the static helpers above where
  // possible. This does not show the window. You need to call window()->Show()
  // to show it.
  void CreateBrowserWindow();

  // Accessors ////////////////////////////////////////////////////////////////

  Type type() const { return type_; }
  Profile* profile() const { return profile_; }
  const std::vector<std::wstring>& user_data_dir_profiles() const;

  // Returns the InstantController or NULL if there is no InstantController for
  // this Browser.
  InstantController* instant() const { return instant_.get(); }

#if defined(UNIT_TEST)
  // Sets the BrowserWindow. This is intended for testing and generally not
  // useful outside of testing. Use CreateBrowserWindow outside of testing, or
  // the static convenience methods that create a BrowserWindow for you.
  void set_window(BrowserWindow* window) {
    DCHECK(!window_);
    window_ = window;
  }
#endif

  // |window()| will return NULL if called before |CreateBrowserWindow()|
  // is done.
  BrowserWindow* window() const { return window_; }
  ToolbarModel* toolbar_model() { return &toolbar_model_; }
  const SessionID& session_id() const { return session_id_; }
  CommandUpdater* command_updater() { return &command_updater_; }
  bool block_command_execution() const { return block_command_execution_; }

  // Get the FindBarController for this browser, creating it if it does not
  // yet exist.
  FindBarController* GetFindBarController();

  // Returns true if a FindBarController exists for this browser.
  bool HasFindBarController() const;

  // Setters /////////////////////////////////////////////////////////////////

  void set_user_data_dir_profiles(const std::vector<std::wstring>& profiles);

  // Browser Creation Helpers /////////////////////////////////////////////////

  // Opens a new window with the default blank tab.
  static void OpenEmptyWindow(Profile* profile);

  // Opens a new window with the tabs from |profile|'s TabRestoreService.
  static void OpenWindowWithRestoredTabs(Profile* profile);

  // Opens the specified URL in a new browser window in an incognito session.
  // If there is already an existing active incognito session for the specified
  // |profile|, that session is re-used.
  static void OpenURLOffTheRecord(Profile* profile, const GURL& url);

  // Open an application specified by |app_id| in the appropriate launch
  // container. |existing_tab| is reused if it is not NULL and the launch
  // container is a tab. Returns NULL if the app_id is invalid or if
  // ExtensionService isn't ready/available.
  static TabContents* OpenApplication(Profile* profile,
                                      const std::string& app_id,
                                      TabContents* existing_tab);

  // Open |extension| in |container|, using |existing_tab| if not NULL and if
  // the correct container type.  Returns the TabContents* that was created or
  // NULL.
  static TabContents* OpenApplication(
      Profile* profile,
      const Extension* extension,
      extension_misc::LaunchContainer container,
      TabContents* existing_tab);

  // Opens a new application window for the specified url. If |as_panel|
  // is true, the application will be opened as a Browser::Type::APP_PANEL in
  // app panel window, otherwise it will be opened as as either
  // Browser::Type::APP a.k.a. "thin frame" (if |extension| is NULL) or
  // Browser::Type::EXTENSION_APP (if |extension| is non-NULL).
  // If |app_browser| is not NULL, it is set to the browser that hosts the
  // returned tab.
  static TabContents* OpenApplicationWindow(
      Profile* profile,
      const Extension* extension,
      extension_misc::LaunchContainer container,
      const GURL& url,
      Browser** app_browser);

  // Open |url| in an app shortcut window.  If |update_shortcut| is true,
  // update the name, description, and favicon of the shortcut.
  // There are two kinds of app shortcuts: Shortcuts to a URL,
  // and shortcuts that open an installed application.  This function
  // is used to open the former.  To open the latter, use
  // Browser::OpenApplicationWindow().
  static TabContents* OpenAppShortcutWindow(Profile* profile,
                                            const GURL& url,
                                            bool update_shortcut);

  // Open an application for |extension| in a new application tab, or
  // |existing_tab| if not NULL.  Returns NULL if there are no appropriate
  // existing browser windows for |profile|.
  static TabContents* OpenApplicationTab(Profile* profile,
                                         const Extension* extension,
                                         TabContents* existing_tab);

  // Opens a new window and opens the bookmark manager.
  static void OpenBookmarkManagerWindow(Profile* profile);

#if defined(OS_MACOSX)
  // Open a new window with history/downloads/help/options (needed on Mac when
  // there are no windows).
  static void OpenHistoryWindow(Profile* profile);
  static void OpenDownloadsWindow(Profile* profile);
  static void OpenHelpWindow(Profile* profile);
  static void OpenOptionsWindow(Profile* profile);
  static void OpenClearBrowingDataDialogWindow(Profile* profile);
  static void OpenImportSettingsDialogWindow(Profile* profile);
#endif

  // Opens a window with the extensions tab in it - needed by long-lived
  // extensions which may run with no windows open.
  static void OpenExtensionsWindow(Profile* profile);

  // State Storage and Retrieval for UI ///////////////////////////////////////

  // Save and restore the window position.
  std::string GetWindowPlacementKey() const;
  bool ShouldSaveWindowPlacement() const;
  void SaveWindowPlacement(const gfx::Rect& bounds, bool maximized);
  gfx::Rect GetSavedWindowBounds() const;
  bool GetSavedMaximizedState() const;

  // Gets the FavIcon of the page in the selected tab.
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

  // In-progress download termination handling /////////////////////////////////

  // Are normal and/or incognito downloads in progress?
  void CheckDownloadsInProgress(bool* normal_downloads,
                                bool* incognito_downloads);

  // Called when the user has decided whether to proceed or not with the browser
  // closure.  |cancel_downloads| is true if the downloads should be canceled
  // and the browser closed, false if the browser should stay open and the
  // downloads running.
  void InProgressDownloadResponse(bool cancel_downloads);

  // TabStripModel pass-thrus /////////////////////////////////////////////////

  TabStripModel* tabstrip_model() const {
    // TODO(beng): remove this accessor. It violates google style.
    return tab_handler_->GetTabStripModel();
  }

  int tab_count() const;
  int selected_index() const;
  int GetIndexOfController(const NavigationController* controller) const;
  TabContentsWrapper* GetSelectedTabContentsWrapper() const;
  TabContentsWrapper* GetTabContentsWrapperAt(int index) const;
  // Same as above but correctly handles if GetSelectedTabContents() is NULL
  // in the model before dereferencing to get the raw TabContents.
  // TODO(pinkerton): These should really be returning TabContentsWrapper
  // objects, but that would require changing about 50+ other files. In order
  // to keep changes localized, the default is to return a TabContents. Note
  // this differs from the TabStripModel because it has far fewer clients.
  TabContents* GetSelectedTabContents() const;
  TabContents* GetTabContentsAt(int index) const;
  void SelectTabContentsAt(int index, bool user_gesture);
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
  TabContentsWrapper* AddSelectedTabWithURL(
      const GURL& url,
      PageTransition::Type transition);

  // Add a new tab, given a TabContents. A TabContents appropriate to
  // display the last committed entry is created and returned.
  TabContents* AddTab(TabContentsWrapper* tab_contents,
                      PageTransition::Type type);

  // Add a tab with its session history restored from the SessionRestore
  // system. If select is true, the tab is selected. |tab_index| gives the index
  // to insert the tab at. |selected_navigation| is the index of the
  // TabNavigation in |navigations| to select. If |extension_app_id| is
  // non-empty the tab is an app tab and |extension_app_id| is the id of the
  // extension. If |pin| is true and |tab_index|/ is the last pinned tab, then
  // the newly created tab is pinned. If |from_last_session| is true,
  // |navigations| are from the previous session.
  TabContents* AddRestoredTab(const std::vector<TabNavigation>& navigations,
                              int tab_index,
                              int selected_navigation,
                              const std::string& extension_app_id,
                              bool select,
                              bool pin,
                              bool from_last_session,
                              SessionStorageNamespace* storage_namespace);
  // Creates a new tab with the already-created TabContents 'new_contents'.
  // The window for the added contents will be reparented correctly when this
  // method returns.  If |disposition| is NEW_POPUP, |pos| should hold the
  // initial position.
  void AddTabContents(TabContents* new_contents,
                      WindowOpenDisposition disposition,
                      const gfx::Rect& initial_pos,
                      bool user_gesture);
  void CloseTabContents(TabContents* contents);

  // Show a dialog with HTML content. |delegate| contains a pointer to the
  // delegate who knows how to display the dialog (which file URL and JSON
  // string input to use during initialization). |parent_window| is the window
  // that should be parent of the dialog, or NULL for the default.
  void BrowserShowHtmlDialog(HtmlDialogUIDelegate* delegate,
                             gfx::NativeWindow parent_window);

  // Called when a popup select is about to be displayed.
  void BrowserRenderWidgetShowing();

  // Notification that some of our content has changed size as
  // part of an animation.
  void ToolbarSizeChanged(bool is_animating);

  // Replaces the state of the currently selected tab with the session
  // history restored from the SessionRestore system.
  void ReplaceRestoredTab(
      const std::vector<TabNavigation>& navigations,
      int selected_navigation,
      bool from_last_session,
      const std::string& extension_app_id,
      SessionStorageNamespace* session_storage_namespace);

  // Navigate to an index in the tab history, opening a new tab depending on the
  // disposition.
  bool NavigateToIndexWithDisposition(int index, WindowOpenDisposition disp);

  // Show a given a URL. If a tab with the same URL (ignoring the ref) is
  // already visible in this browser, it becomes selected. Otherwise a new tab
  // is created. If |ignore_path| is true, the paths of the URLs are ignored
  // when locating the singleton tab.
  void ShowSingletonTab(const GURL& url, bool ignore_path);

  // Update commands whose state depends on whether the window is in fullscreen
  // mode. This is a public function because on Linux, fullscreen mode is an
  // async call to X. Once we get the fullscreen callback, the browser window
  // will call this method.
  void UpdateCommandsForFullscreenMode(bool is_fullscreen);

  // Assorted browser commands ////////////////////////////////////////////////

  // NOTE: Within each of the following sections, the IDs are ordered roughly by
  // how they appear in the GUI/menus (left to right, top to bottom, etc.).

  // Navigation commands
  void GoBack(WindowOpenDisposition disposition);
  void GoForward(WindowOpenDisposition disposition);
  void Reload(WindowOpenDisposition disposition);
  void ReloadIgnoringCache(WindowOpenDisposition disposition);  // Shift-reload.
  void Home(WindowOpenDisposition disposition);
  void OpenCurrentURL();
  void Stop();
  // Window management commands
  void NewWindow();
  void NewIncognitoWindow();
  void CloseWindow();
  void NewTab();
  void CloseTab();
  void SelectNextTab();
  void SelectPreviousTab();
  void OpenTabpose();
  void MoveTabNext();
  void MoveTabPrevious();
  void SelectNumberedTab(int index);
  void SelectLastTab();
  void DuplicateTab();
  void WriteCurrentURLToClipboard();
  void ConvertPopupToTabbedBrowser();
  // In kiosk mode, the first toggle is valid, the rest is discarded.
  void ToggleFullscreenMode();
  void Exit();
#if defined(OS_CHROMEOS)
  void ToggleCompactNavigationBar();
  void Search();
  void ShowKeyboardOverlay();
#endif

  // Page-related commands
  void BookmarkCurrentPage();
  void SavePage();
  void ViewSelectedSource();
  void ShowFindBar();

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
  void Print();
  void EmailPageLocation();
  void ToggleEncodingAutoDetect();
  void OverrideEncoding(int encoding_id);

  // Clipboard commands
  void Cut();
  void Copy();
  void Paste();

  // Find-in-page
  void Find();
  void FindNext();
  void FindPrevious();

  // Zoom
  void Zoom(PageZoom::Function zoom_function);

  // Focus various bits of UI
  void FocusToolbar();
  void FocusLocationBar();  // Also selects any existing text.
  void FocusSearch();
  void FocusAppMenu();
  void FocusBookmarksToolbar();
  void FocusChromeOSStatus();
  void FocusNextPane();
  void FocusPreviousPane();

  // Show various bits of UI
  void OpenFile();
  void OpenCreateShortcutsDialog();
  void ToggleDevToolsWindow(DevToolsToggleAction action);
  void OpenTaskManager(bool highlight_background_resources);
  void OpenBugReportDialog();

  void ToggleBookmarkBar();

  void OpenBookmarkManager();
  void ShowAppMenu();
  void ShowBookmarkManagerTab();
  void ShowHistoryTab();
  void ShowDownloadsTab();
  void ShowExtensionsTab();
  void ShowAboutConflictsTab();
  void ShowBrokenPageTab(TabContents* contents);
  void ShowOptionsTab(const std::string& sub_page);
  void OpenClearBrowsingDataDialog();
  void OpenOptionsDialog();
  void OpenKeywordEditor();
  void OpenPasswordManager();
  void OpenSyncMyBookmarksDialog();
#if defined(ENABLE_REMOTING)
  void OpenRemotingSetupDialog();
#endif
  void OpenImportSettingsDialog();
  void OpenAboutChromeDialog();
  void OpenUpdateChromeDialog();
  void OpenHelpTab();
  // Used by the "Get themes" link in the options dialog.
  void OpenThemeGalleryTabAndActivate();
  void OpenAutoFillHelpTabAndActivate();
  void OpenPrivacyDashboardTabAndActivate();
  void OpenSearchEngineOptionsDialog();
#if defined(OS_CHROMEOS)
  void OpenSystemOptionsDialog();
  void OpenInternetOptionsDialog();
  void OpenLanguageOptionsDialog();
  void OpenSystemTabAndActivate();
  void OpenMobilePlanTabAndActivate();
#endif
  void OpenPluginsTabAndActivate();

  virtual void UpdateDownloadShelfVisibility(bool visible);

  // Overridden from TabStripModelDelegate:
  virtual bool UseVerticalTabs() const;

  /////////////////////////////////////////////////////////////////////////////

  // Sets the value of homepage related prefs to new values. Since we do not
  // want to change these values for existing users, we can not change the
  // default values under RegisterUserPrefs. Also if user already has an
  // existing profile we do not want to override those preferences so we only
  // set new values if they have not been set already. This method gets called
  // during First Run.
  static void SetNewHomePagePrefs(PrefService* prefs);

  static void RegisterPrefs(PrefService* prefs);
  static void RegisterUserPrefs(PrefService* prefs);

  // Helper function to run unload listeners on a TabContents.
  static bool RunUnloadEventsHelper(TabContents* contents);

  // Returns the Browser which contains the tab with the given
  // NavigationController, also filling in |index| (if valid) with the tab's
  // index in the tab strip.
  // Returns NULL if not found.
  // This call is O(N) in the number of tabs.
  static Browser* GetBrowserForController(
      const NavigationController* controller, int* index);

  // Retrieve the last active tabbed browser with a profile matching |profile|.
  static Browser* GetTabbedBrowser(Profile* profile, bool match_incognito);

  // Retrieve the last active tabbed browser with a profile matching |profile|.
  // Creates a new Browser if none are available.
  static Browser* GetOrCreateTabbedBrowser(Profile* profile);

  // Calls ExecuteCommandWithDisposition with the given disposition.
  void ExecuteCommandWithDisposition(int id, WindowOpenDisposition);

  // Executes a command if it's enabled.
  // Returns true if the command is executed.
  bool ExecuteCommandIfEnabled(int id);

  // Returns whether the |id| is a reserved command, whose keyboard shortcuts
  // should not be sent to the renderer.
  bool IsReservedCommand(int id);

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
  void UpdateUIForNavigationInTab(TabContentsWrapper* contents,
                                  PageTransition::Type transition,
                                  bool user_initiated);

  // Called by browser::Navigate() to retrieve the home page if no URL is
  // specified.
  GURL GetHomePage() const;

  // Interface implementations ////////////////////////////////////////////////

  // Overridden from PageNavigator:
  virtual void OpenURL(const GURL& url, const GURL& referrer,
                       WindowOpenDisposition disposition,
                       PageTransition::Type transition);

  // Overridden from CommandUpdater::CommandUpdaterDelegate:
  virtual void ExecuteCommand(int id);

  // Overridden from TabRestoreServiceObserver:
  virtual void TabRestoreServiceChanged(TabRestoreService* service);
  virtual void TabRestoreServiceDestroyed(TabRestoreService* service);

  // Centralized method for creating a TabContents, configuring and installing
  // all its supporting objects and observers.
  static TabContentsWrapper*
      TabContentsFactory(Profile* profile,
                         SiteInstance* site_instance,
                         int routing_id,
                         const TabContents* base_tab_contents,
                         SessionStorageNamespace* session_storage_namespace);

  // Overridden from TabHandlerDelegate:
  virtual Profile* GetProfile() const;
  virtual Browser* AsBrowser();

  // Overridden from TabStripModelDelegate:
  virtual TabContentsWrapper* AddBlankTab(bool foreground);
  virtual TabContentsWrapper* AddBlankTabAt(int index, bool foreground);
  virtual Browser* CreateNewStripWithContents(
      TabContentsWrapper* detached_contents,
      const gfx::Rect& window_bounds,
      const DockInfo& dock_info,
      bool maximize);
  virtual int GetDragActions() const;
  // Construct a TabContents for a given URL, profile and transition type.
  // If instance is not null, its process will be used to render the tab.
  virtual TabContentsWrapper* CreateTabContentsForURL(const GURL& url,
                                               const GURL& referrer,
                                               Profile* profile,
                                               PageTransition::Type transition,
                                               bool defer_load,
                                               SiteInstance* instance) const;
  virtual bool CanDuplicateContentsAt(int index);
  virtual void DuplicateContentsAt(int index);
  virtual void CloseFrameAfterDragSession();
  virtual void CreateHistoricalTab(TabContentsWrapper* contents);
  virtual bool RunUnloadListenerBeforeClosing(TabContentsWrapper* contents);
  virtual bool CanCloseContentsAt(int index);
  virtual bool CanBookmarkAllTabs() const;
  virtual void BookmarkAllTabs();
  virtual bool CanCloseTab() const;
  virtual void ToggleUseVerticalTabs();
  virtual bool CanRestoreTab();
  virtual void RestoreTab();
  virtual bool LargeIconsPermitted() const;

  // Overridden from TabStripModelObserver:
  virtual void TabInsertedAt(TabContentsWrapper* contents,
                             int index,
                             bool foreground);
  virtual void TabClosingAt(TabStripModel* tab_strip_model,
                            TabContentsWrapper* contents,
                            int index);
  virtual void TabDetachedAt(TabContentsWrapper* contents, int index);
  virtual void TabDeselectedAt(TabContentsWrapper* contents, int index);
  virtual void TabSelectedAt(TabContentsWrapper* old_contents,
                             TabContentsWrapper* new_contents,
                             int index,
                             bool user_gesture);
  virtual void TabMoved(TabContentsWrapper* contents,
                        int from_index,
                        int to_index);
  virtual void TabReplacedAt(TabStripModel* tab_strip_model,
                             TabContentsWrapper* old_contents,
                             TabContentsWrapper* new_contents,
                             int index);
  virtual void TabPinnedStateChanged(TabContentsWrapper* contents, int index);
  virtual void TabStripEmpty();

  // Figure out if there are tabs that have beforeunload handlers.
  bool TabsNeedBeforeUnloadFired();

 private:
  FRIEND_TEST_ALL_PREFIXES(BrowserTest, NoTabsInPopups);
  FRIEND_TEST_ALL_PREFIXES(BrowserTest, ConvertTabToAppShortcut);
  FRIEND_TEST_ALL_PREFIXES(BrowserTest, OpenAppWindowLikeNtp);
  FRIEND_TEST_ALL_PREFIXES(BrowserTest, AppIdSwitch);

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

  // Overridden from TabContentsDelegate:
  virtual void OpenURLFromTab(TabContents* source,
                              const GURL& url,
                              const GURL& referrer,
                              WindowOpenDisposition disposition,
                              PageTransition::Type transition);
  virtual void NavigationStateChanged(const TabContents* source,
                                      unsigned changed_flags);
  virtual void AddNewContents(TabContents* source,
                              TabContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture);
  virtual void ActivateContents(TabContents* contents);
  virtual void DeactivateContents(TabContents* contents);
  virtual void LoadingStateChanged(TabContents* source);
  virtual void CloseContents(TabContents* source);
  virtual void MoveContents(TabContents* source, const gfx::Rect& pos);
  virtual void DetachContents(TabContents* source);
  virtual bool IsPopup(const TabContents* source) const;
  virtual bool CanReloadContents(TabContents* source) const;
  virtual void ToolbarSizeChanged(TabContents* source, bool is_animating);
  virtual void UpdateTargetURL(TabContents* source, const GURL& url);
  virtual void ContentsMouseEvent(
      TabContents* source, const gfx::Point& location, bool motion);
  virtual void ContentsZoomChange(bool zoom_in);
  virtual void OnContentSettingsChange(TabContents* source);
  virtual void SetTabContentBlocked(TabContents* contents, bool blocked);
  virtual void TabContentsFocused(TabContents* tab_content);
  virtual bool TakeFocus(bool reverse);
  virtual bool IsApplication() const;
  virtual void ConvertContentsToApplication(TabContents* source);
  virtual bool ShouldDisplayURLField();
  virtual void ShowHtmlDialog(HtmlDialogUIDelegate* delegate,
                              gfx::NativeWindow parent_window);
  virtual void BeforeUnloadFired(TabContents* source,
                                 bool proceed,
                                 bool* proceed_to_fire_unload);
  virtual void SetFocusToLocationBar(bool select_all);
  virtual void RenderWidgetShowing();
  virtual int GetExtraRenderViewHeight() const;
  virtual void OnStartDownload(DownloadItem* download, TabContents* tab);
  virtual void ConfirmSetDefaultSearchProvider(
      TabContents* tab_contents,
      TemplateURL* template_url,
      TemplateURLModel* template_url_model);
  virtual void ConfirmAddSearchProvider(const TemplateURL* template_url,
                                        Profile* profile);
  virtual void ShowPageInfo(Profile* profile,
                            const GURL& url,
                            const NavigationEntry::SSLStatus& ssl,
                            bool show_history);
  virtual void ViewSourceForTab(TabContents* source, const GURL& page_url);
  virtual bool PreHandleKeyboardEvent(const NativeWebKeyboardEvent& event,
                                        bool* is_keyboard_shortcut);
  virtual void HandleKeyboardEvent(const NativeWebKeyboardEvent& event);
  virtual void ShowRepostFormWarningDialog(TabContents* tab_contents);
  virtual void ShowContentSettingsWindow(ContentSettingsType content_type);
  virtual void ShowCollectedCookiesDialog(TabContents* tab_contents);
  virtual bool ShouldAddNavigationToHistory(
      const history::HistoryAddPageArgs& add_page_args,
      NavigationType::Type navigation_type);
  virtual void OnDidGetApplicationInfo(TabContents* tab_contents,
                                       int32 page_id);
  virtual void OnInstallApplication(TabContents* tab_contents,
                                    const WebApplicationInfo& app_info);
  virtual void ContentRestrictionsChanged(TabContents* source);

  // Overridden from TabContentsWrapperDelegate:
  virtual void URLStarredChanged(TabContentsWrapper* source,
                                 bool starred) OVERRIDE;

  // Overridden from SelectFileDialog::Listener:
  virtual void FileSelected(const FilePath& path, int index, void* params);

  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Overridden from ProfileSyncServiceObserver:
  virtual void OnStateChanged();

  // Overriden from InstantDelegate:
  virtual void PrepareForInstant();
  virtual void ShowInstant(TabContentsWrapper* preview_contents);
  virtual void HideInstant();
  virtual void CommitInstant(TabContentsWrapper* preview_contents);
  virtual void SetSuggestedText(const string16& text);
  virtual gfx::Rect GetInstantBounds();

  // Command and state updating ///////////////////////////////////////////////

  // Initialize state for all browser commands.
  void InitCommandState();

  // Update commands whose state depends on the tab's state.
  void UpdateCommandsForTabState();

  // Updates commands when the content's restrictions change.
  void UpdateCommandsForContentRestrictionState();

  // Updates commands for enabling developer tools.
  void UpdateCommandsForDevTools();

  // Updates the printing command state.
  void UpdatePrintingState(int content_restrictions);

  // Ask the Reload/Stop button to change its icon, and update the Stop command
  // state.  |is_loading| is true if the current TabContents is loading.
  // |force| is true if the button should change its icon immediately.
  void UpdateReloadStopState(bool is_loading, bool force);

  // UI update coalescing and handling ////////////////////////////////////////

  // Asks the toolbar (and as such the location bar) to update its state to
  // reflect the current tab's current URL, security state, etc.
  // If |should_restore_state| is true, we're switching (back?) to this tab and
  // should restore any previous location bar state (such as user editing) as
  // well.
  void UpdateToolbar(bool should_restore_state);

  // Does one or both of the following for each bit in |changed_flags|:
  // . If the update should be processed immediately, it is.
  // . If the update should processed asynchronously (to avoid lots of ui
  //   updates), then scheduled_updates_ is updated for the |source| and update
  //   pair and a task is scheduled (assuming it isn't running already)
  //   that invokes ProcessPendingUIUpdates.
  void ScheduleUIUpdate(const TabContents* source, unsigned changed_flags);

  // Processes all pending updates to the UI that have been scheduled by
  // ScheduleUIUpdate in scheduled_updates_.
  void ProcessPendingUIUpdates();

  // Removes all entries from scheduled_updates_ whose source is contents.
  void RemoveScheduledUpdatesFor(TabContents* contents);

  // Getters for UI ///////////////////////////////////////////////////////////

  // TODO(beng): remove, and provide AutomationProvider a better way to access
  //             the LocationBarView's edit.
  friend class AutomationProvider;
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

  typedef std::set<TabContents*> UnloadListenerSet;

  // Processes the next tab that needs it's beforeunload/unload event fired.
  void ProcessPendingTabs();

  // Whether we've completed firing all the tabs' beforeunload/unload events.
  bool HasCompletedUnloadProcessing() const;

  // Clears all the state associated with processing tabs' beforeunload/unload
  // events since the user cancelled closing the window.
  void CancelWindowClose();

  // Removes |tab| from the passed |set|.
  // Returns whether the tab was in the set in the first place.
  // TODO(beng): this method needs a better name!
  bool RemoveFromSet(UnloadListenerSet* set, TabContents* tab);

  // Cleans up state appropriately when we are trying to close the browser and
  // the tab has finished firing its unload handler. We also use this in the
  // cases where a tab crashes or hangs even if the beforeunload/unload haven't
  // successfully fired. If |process_now| is true |ProcessPendingTabs| is
  // invoked immediately, otherwise it is invoked after a delay (PostTask).
  //
  // Typically you'll want to pass in true for |process_now|. Passing in true
  // may result in deleting |tab|. If you know that shouldn't happen (because of
  // the state of the stack), pass in false.
  void ClearUnloadState(TabContents* tab, bool process_now);

  // In-progress download termination handling /////////////////////////////////

  // Called when the window is closing to check if potential in-progress
  // downloads should prevent it from closing.
  // Returns true if the window can close, false otherwise.
  bool CanCloseWithInProgressDownloads();

  // Assorted utility functions ///////////////////////////////////////////////

  // Checks whether |source| is about to navigate across extension extents, and
  // if so, navigates in the correct window. For example if this is a normal
  // browser and we're about to navigate into an extent, this method will
  // navigate the app's window instead. If we're in an app window and
  // navigating out of the app, this method will find and navigate a normal
  // browser instead.
  //
  // Returns true if the navigation was handled, eg, it was opened in some other
  // browser.
  //
  // Returns false if it was not handled. In this case, the method may also
  // modify |disposition| to a more suitable value.
  bool HandleCrossAppNavigation(TabContents* source,
                                const GURL& url,
                                const GURL& referrer,
                                WindowOpenDisposition *disposition,
                                PageTransition::Type transition);

  // Shows the Find Bar, optionally selecting the next entry that matches the
  // existing search string for that Tab. |forward_direction| controls the
  // search direction.
  void FindInPage(bool find_next, bool forward_direction);

  // Closes the frame.
  // TODO(beng): figure out if we need this now that the frame itself closes
  //             after a return to the message loop.
  void CloseFrame();

  void TabDetachedAtImpl(TabContentsWrapper* contents,
      int index, DetachType type);

  // Create a preference dictionary for the provided application name, in the
  // given user profile. This is done only once per application name / per
  // session / per user profile.
  static void RegisterAppPrefs(const std::string& app_name, Profile* profile);

  // Shared code between Reload() and ReloadIgnoringCache().
  void ReloadInternal(WindowOpenDisposition disposition, bool ignore_cache);

  // Return true if the window dispositions means opening a new tab.
  bool ShouldOpenNewTabForWindowDisposition(WindowOpenDisposition disposition);

  // Depending on the disposition, return the current tab or a clone of the
  // current tab.
  TabContents* GetOrCloneTabForDisposition(WindowOpenDisposition disposition);

  // Sets the insertion policy of the tabstrip based on whether vertical tabs
  // are enabled.
  void UpdateTabStripModelInsertionPolicy();

  // Invoked when the use vertical tabs preference changes. Resets the insertion
  // policy of the tab strip model and notifies the window.
  void UseVerticalTabsChanged();

  // Implementation of SupportsWindowFeature and CanSupportWindowFeature. If
  // |check_fullscreen| is true, the set of features reflect the actual state of
  // the browser, otherwise the set of features reflect the possible state of
  // the browser.
  bool SupportsWindowFeatureImpl(WindowFeature feature,
                                 bool check_fullscreen) const;

  // Determines if closing of browser can really be permitted after normal
  // sequence of downloads and unload handlers have given the go-ahead to close.
  // It is called from ShouldCloseWindow.  It checks with
  // TabCloseableStateWatcher to confirm if browser can really be closed.
  // Appropriate action is taken by watcher as it sees fit.
  // If watcher denies closing of browser, CancelWindowClose is called to
  // cancel closing of window.
  bool IsClosingPermitted();

  // Commits the current instant, returning true on success. This is intended
  // for use from OpenCurrentURL.
  bool OpenInstant(WindowOpenDisposition disposition);

  // If this browser should have instant one is created, otherwise does nothing.
  void CreateInstantIfNecessary();

  // Opens view-source tab for given tab contents.
  void ViewSource(TabContentsWrapper* tab);

  // Data members /////////////////////////////////////////////////////////////

  NotificationRegistrar registrar_;

  // This Browser's type.
  const Type type_;

  // This Browser's profile.
  Profile* const profile_;

  // This Browser's window.
  BrowserWindow* window_;

  // This Browser's current TabHandler.
  scoped_ptr<TabHandler> tab_handler_;

  // The CommandUpdater that manages the browser window commands.
  CommandUpdater command_updater_;

  // An optional application name which is used to retrieve and save window
  // positions.
  std::string app_name_;

  // Unique identifier of this browser for session restore. This id is only
  // unique within the current session, and is not guaranteed to be unique
  // across sessions.
  const SessionID session_id_;

  // The model for the toolbar view.
  ToolbarModel toolbar_model_;

  // UI update coalescing and handling ////////////////////////////////////////

  typedef std::map<const TabContents*, int> UpdateMap;

  // Maps from TabContents to pending UI updates that need to be processed.
  // We don't update things like the URL or tab title right away to avoid
  // flickering and extra painting.
  // See ScheduleUIUpdate and ProcessPendingUIUpdates.
  UpdateMap scheduled_updates_;

  // The following factory is used for chrome update coalescing.
  ScopedRunnableMethodFactory<Browser> chrome_updater_factory_;

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

  // Override values for the bounds of the window and its maximized state.
  // These are supplied by callers that don't want to use the default values.
  // The default values are typically loaded from local state (last session),
  // obtained from the last window of the same type, or obtained from the
  // shell shortcut's startup info.
  gfx::Rect override_bounds_;
  MaximizedState maximized_state_;

  // The following factory is used to close the frame at a later time.
  ScopedRunnableMethodFactory<Browser> method_factory_;

  // The Find Bar. This may be NULL if there is no Find Bar, and if it is
  // non-NULL, it may or may not be visible.
  scoped_ptr<FindBarController> find_bar_controller_;

  // Dialog box used for opening and saving files.
  scoped_refptr<SelectFileDialog> select_file_dialog_;

  // Keep track of the encoding auto detect pref.
  BooleanPrefMember encoding_auto_detect_;

  // Keep track of the printing enabled pref.
  BooleanPrefMember printing_enabled_;

  // Keep track of the development tools disabled pref.
  BooleanPrefMember dev_tools_disabled_;

  // Keep track of when instant enabled changes.
  BooleanPrefMember instant_enabled_;

  // Tracks the preference that controls whether incognito mode is allowed.
  BooleanPrefMember incognito_mode_allowed_;

  // Indicates if command execution is blocked.
  bool block_command_execution_;

  // Stores the last blocked command id when |block_command_execution_| is true.
  int last_blocked_command_id_;

  // Stores the disposition type of the last blocked command.
  WindowOpenDisposition last_blocked_command_disposition_;

  // Different types of action when web app info is available.
  // OnDidGetApplicationInfo uses this to dispatch calls.
  enum WebAppAction {
    NONE,             // No action at all.
    CREATE_SHORTCUT,  // Bring up create application shortcut dialog.
    UPDATE_SHORTCUT   // Update icon for app shortcut.
  };

  // Which deferred action to perform when OnDidGetApplicationInfo is notified
  // from a TabContents. Currently, only one pending action is allowed.
  WebAppAction pending_web_app_action_;

  // Tracks the display mode of the tabstrip.
  mutable BooleanPrefMember use_vertical_tabs_;

  // The profile's tab restore service. The service is owned by the profile,
  // and we install ourselves as an observer.
  TabRestoreService* tab_restore_service_;

  scoped_ptr<InstantController> instant_;
  scoped_ptr<InstantUnloadHandler> instant_unload_handler_;

  DISALLOW_COPY_AND_ASSIGN(Browser);
};

#endif  // CHROME_BROWSER_UI_BROWSER_H_
