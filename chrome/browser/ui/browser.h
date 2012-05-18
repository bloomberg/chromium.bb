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
#include "chrome/browser/ui/webui/sync_promo/sync_promo_ui.h"
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
class Extension;
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
class WebDialogDelegate;

namespace content {
class NavigationController;
class SessionStorageNamespace;
}

namespace gfx {
class Point;
}

class Browser : public TabStripModelDelegate,
                public TabStripModelObserver,
                public content::WebContentsDelegate,
                public CoreTabHelperDelegate,
                public SearchEngineTabHelperDelegate,
                public ConstrainedWindowTabHelperDelegate,
                public BlockedContentTabHelperDelegate,
                public BookmarkTabHelperDelegate,
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
  // Browser::OpenApplication), and child browsers created by an app from
  // Browser::CreateForApp (e.g. by windows.open or the extension API).
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
  void set_show_state(ui::WindowShowState show_state) {
    show_state_ = show_state;
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

  // Browser Creation Helpers /////////////////////////////////////////////////

  // Opens a new window with the default blank tab.
  static void NewEmptyWindow(Profile* profile);

  // Opens a new window with the default blank tab. This bypasses metrics and
  // various internal bookkeeping; NewEmptyWindow (above) is preferred.
  static Browser* OpenEmptyWindow(Profile* profile);

  // Opens a new window with the tabs from |profile|'s TabRestoreService.
  static void OpenWindowWithRestoredTabs(Profile* profile);

  // Opens the specified URL in a new browser window in an incognito session.
  // If there is already an existing active incognito session for the specified
  // |profile|, that session is re-used.
  static void OpenURLOffTheRecord(Profile* profile, const GURL& url);

  // Open |extension| in |container|, using |disposition| if container type is
  // TAB. Returns the WebContents* that was created or NULL. If non-empty,
  // |override_url| is used in place of the app launch url.
  static content::WebContents* OpenApplication(
      Profile* profile,
      const Extension* extension,
      extension_misc::LaunchContainer container,
      const GURL& override_url,
      WindowOpenDisposition disposition);

#if defined(USE_ASH)
  // Opens |url| in a new application panel window for the specified url.
  static content::WebContents* OpenApplicationPanel(
      Profile* profile,
      const Extension* extension,
      const GURL& url);
#endif

  // Opens a new application window for the specified url. If |as_panel|
  // is true, the application will be opened as a Browser::Type::APP_PANEL in
  // app panel window, otherwise it will be opened as as either
  // Browser::Type::APP a.k.a. "thin frame" (if |extension| is NULL) or
  // Browser::Type::EXTENSION_APP (if |extension| is non-NULL).
  // If |app_browser| is not NULL, it is set to the browser that hosts the
  // returned tab.
  static content::WebContents* OpenApplicationWindow(
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
  static content::WebContents* OpenAppShortcutWindow(Profile* profile,
                                                     const GURL& url,
                                                     bool update_shortcut);

  // Open an application for |extension| using |disposition|.  Returns NULL if
  // there are no appropriate existing browser windows for |profile|. If
  // non-empty, |override_url| is used in place of the app launch url.
  static content::WebContents* OpenApplicationTab(
      Profile* profile,
      const Extension* extension,
      const GURL& override_url,
      WindowOpenDisposition disposition);

  // Opens a new window and opens the bookmark manager.
  static void OpenBookmarkManagerWindow(Profile* profile);

#if defined(OS_MACOSX)
  // Open a new window with history/downloads/help/options (needed on Mac when
  // there are no windows).
  static void OpenAboutWindow(Profile* profile);
  static void OpenHistoryWindow(Profile* profile);
  static void OpenDownloadsWindow(Profile* profile);
  static void OpenHelpWindow(Profile* profile);
  static void OpenOptionsWindow(Profile* profile);
  static void OpenSyncSetupWindow(Profile* profile,
                                  SyncPromoUI::Source source);
  static void OpenClearBrowsingDataDialogWindow(Profile* profile);
  static void OpenImportSettingsDialogWindow(Profile* profile);
  static void OpenInstantConfirmDialogWindow(Profile* profile);
#endif

  // Opens a window with the extensions tab in it - needed by long-lived
  // extensions which may run with no windows open.
  static void OpenExtensionsWindow(Profile* profile);

  // State Storage and Retrieval for UI ///////////////////////////////////////

  // Save and restore the window position.
  std::string GetWindowPlacementKey() const;
  bool ShouldSaveWindowPlacement() const;
  void SaveWindowPlacement(const gfx::Rect& bounds,
                           ui::WindowShowState show_state);
  gfx::Rect GetSavedWindowBounds() const;
  ui::WindowShowState GetSavedWindowShowState() const;

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

  // TODO(dpapad): Rename to GetActiveTabContentsWrapper().
  TabContentsWrapper* GetSelectedTabContentsWrapper() const;
  // A convenient version of the above which returns the TCW's WebContents.
  content::WebContents* GetSelectedWebContents() const;
  TabContentsWrapper* GetTabContentsWrapperAt(int index) const;
  // A convenient version of the above which returns the TCW's WebContents.
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
  // created TabContentsWrapper.
  TabContentsWrapper* AddSelectedTabWithURL(
      const GURL& url,
      content::PageTransition transition);

  // Add a new tab, given a TabContentsWrapper. A WebContents appropriate to
  // display the last committed entry is created and returned.
  content::WebContents* AddTab(TabContentsWrapper* tab_contents,
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

  // Shows a dialog with HTML content and returns it. |delegate| contains a
  // pointer to the delegate who knows how to display the dialog (which file
  // URL and JSON string input to use during initialization). |parent_window|
  // is the window that should be parent of the dialog, or NULL for the default.
  // |style| customizes this HTML dialog decoration and layout (X button,
  // throbber, default content padding).
  gfx::NativeWindow BrowserShowWebDialog(WebDialogDelegate* delegate,
                                         gfx::NativeWindow parent_window);

  // Called when a popup select is about to be displayed.
  void BrowserRenderWidgetShowing();

  // Notification that the bookmark bar has changed size.  We need to resize the
  // content area and notify our InfoBarContainer.
  void BookmarkBarSizeChanged(bool is_animating);

  // Replaces the state of the currently selected tab with the session
  // history restored from the SessionRestore system.
  void ReplaceRestoredTab(
      const std::vector<TabNavigation>& navigations,
      int selected_navigation,
      bool from_last_session,
      const std::string& extension_app_id,
      content::SessionStorageNamespace* session_storage_namespace);

  // Navigate to an index in the tab history, opening a new tab depending on the
  // disposition.
  bool NavigateToIndexWithDisposition(int index, WindowOpenDisposition disp);

  // Show a given a URL. If a tab with the same URL (ignoring the ref) is
  // already visible in this browser, it becomes selected. Otherwise a new tab
  // is created.
  void ShowSingletonTab(const GURL& url);

  // Same as ShowSingletonTab, but does not ignore ref.
  void ShowSingletonTabRespectRef(const GURL& url);

  // As ShowSingletonTab, but if the current tab is the new tab page or
  // about:blank, then overwrite it with the passed contents.
  void ShowSingletonTabOverwritingNTP(const browser::NavigateParams& params);

  // Creates a NavigateParams struct for a singleton tab navigation.
  browser::NavigateParams GetSingletonTabNavigateParams(const GURL& url);

  // Invoked when the fullscreen state of the window changes.
  // BrowserWindow::EnterFullscreen invokes this after the window has become
  // fullscreen.
  void WindowFullscreenStateChanged();

  // Assorted browser commands ////////////////////////////////////////////////

  // NOTE: Within each of the following sections, the IDs are ordered roughly by
  // how they appear in the GUI/menus (left to right, top to bottom, etc.).

  // Navigation commands
  bool CanGoBack() const;
  void GoBack(WindowOpenDisposition disposition);
  bool CanGoForward() const;
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
  // See the description of
  // FullscreenController::ToggleFullscreenModeWithExtension.
  void ToggleFullscreenModeWithExtension(const GURL& extension_url);
#if defined(OS_MACOSX)
  void TogglePresentationMode();
#endif
  void Exit();

  // Page-related commands
  void BookmarkCurrentPage();
  void SavePage();
  void ViewSelectedSource();
  void ShowFindBar();
  void ShowPageInfo(content::WebContents* web_contents,
                    const GURL& url,
                    const content::SSLStatus& ssl,
                    bool show_history);
  void ShowChromeToMobileBubble();

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
  void AdvancedPrint();
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
  void Zoom(content::PageZoom zoom);

  // Focus various bits of UI
  void FocusToolbar();
  void FocusLocationBar();  // Also selects any existing text.
  void FocusSearch();
  void FocusAppMenu();
  void FocusBookmarksToolbar();
  void FocusNextPane();
  void FocusPreviousPane();

  // Show various bits of UI
  void OpenFile();
  void OpenCreateShortcutsDialog();
  void ToggleDevToolsWindow(DevToolsToggleAction action);
  void OpenTaskManager(bool highlight_background_resources);
  void OpenFeedbackDialog();

  void ToggleBookmarkBar();

  void OpenBookmarkManager();
  void OpenBookmarkManagerForNode(int64 node_id);
  void OpenBookmarkManagerEditNode(int64 node_id);
  void ShowAppMenu();
  void ShowAvatarMenu();
  void ShowHistoryTab();
  void ShowDownloadsTab();
  void ShowExtensionsTab();
  void ShowAboutConflictsTab();
  void ShowBrokenPageTab(content::WebContents* contents);
  void ShowOptionsTab(const std::string& sub_page);
  // Shows the Content Settings page for a given content type.
  void ShowContentSettingsPage(ContentSettingsType content_type);
  void OpenClearBrowsingDataDialog();
  void OpenOptionsDialog();
  void OpenPasswordManager();
  void OpenSyncMyBookmarksDialog();
  void OpenImportSettingsDialog();
  void OpenInstantConfirmDialog();
  void OpenAboutChromeDialog();
  void OpenUpdateChromeDialog();
  void ShowHelpTab();
  void OpenAutofillHelpTabAndActivate();
  void OpenPrivacyDashboardTabAndActivate();
  void OpenSearchEngineOptionsDialog();
  void OpenPluginsTabAndActivate();
  void ShowSyncSetup(SyncPromoUI::Source source);
  void ToggleSpeechInput();

  virtual void UpdateDownloadShelfVisibility(bool visible);

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

  // Helper function to run unload listeners on a WebContents.
  static bool RunUnloadEventsHelper(content::WebContents* contents);

  // Helper function to display the file selection dialog.
  static void RunFileChooserHelper(
      content::WebContents* tab, const content::FileChooserParams& params);

  // Helper function to enumerate a directory.
  static void EnumerateDirectoryHelper(content::WebContents* tab,
                                       int request_id,
                                       const FilePath& path);

  // Helper function to handle JS out of memory notifications
  static void JSOutOfMemoryHelper(content::WebContents* tab);

  // Helper function to register a protocol handler.
  static void RegisterProtocolHandlerHelper(content::WebContents* tab,
                                            const std::string& protocol,
                                            const GURL& url,
                                            const string16& title);

  // Helper function to register an intent handler.
  static void RegisterIntentHandlerHelper(content::WebContents* tab,
                                          const string16& action,
                                          const string16& type,
                                          const string16& href,
                                          const string16& title,
                                          const string16& disposition);

  // Helper function to handle find results.
  static void FindReplyHelper(content::WebContents* tab,
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
  void UpdateUIForNavigationInTab(TabContentsWrapper* contents,
                                  content::PageTransition transition,
                                  bool user_initiated);

  // Shows the cookies collected in the tab contents wrapper.
  void ShowCollectedCookiesDialog(TabContentsWrapper* wrapper);

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

  // Centralized method for creating a TabContentsWrapper, configuring and
  // installing all its supporting objects and observers.
  static TabContentsWrapper* TabContentsFactory(
      Profile* profile,
      content::SiteInstance* site_instance,
      int routing_id,
      const content::WebContents* base_web_contents,
      content::SessionStorageNamespace* session_storage_namespace);

  // Overridden from TabStripModelDelegate:
  virtual TabContentsWrapper* AddBlankTab(bool foreground) OVERRIDE;
  virtual TabContentsWrapper* AddBlankTabAt(int index,
                                            bool foreground) OVERRIDE;
  virtual Browser* CreateNewStripWithContents(
      TabContentsWrapper* detached_contents,
      const gfx::Rect& window_bounds,
      const DockInfo& dock_info,
      bool maximize) OVERRIDE;
  virtual int GetDragActions() const OVERRIDE;
  // Construct a TabContentsWrapper for a given URL, profile and transition
  // type. If instance is not null, its process will be used to render the tab.
  virtual TabContentsWrapper* CreateTabContentsForURL(
      const GURL& url,
      const content::Referrer& referrer,
      Profile* profile,
      content::PageTransition transition,
      bool defer_load,
      content::SiteInstance* instance) const OVERRIDE;
  virtual bool CanDuplicateContentsAt(int index) OVERRIDE;
  virtual void DuplicateContentsAt(int index) OVERRIDE;
  virtual void CloseFrameAfterDragSession() OVERRIDE;
  virtual void CreateHistoricalTab(TabContentsWrapper* contents) OVERRIDE;
  virtual bool RunUnloadListenerBeforeClosing(
      TabContentsWrapper* contents) OVERRIDE;
  virtual bool CanCloseContents(std::vector<int>* indices) OVERRIDE;
  virtual bool CanBookmarkAllTabs() const OVERRIDE;
  virtual void BookmarkAllTabs() OVERRIDE;
  virtual bool CanCloseTab() const OVERRIDE;
  virtual bool CanRestoreTab() OVERRIDE;
  virtual void RestoreTab() OVERRIDE;
  virtual bool LargeIconsPermitted() const OVERRIDE;

  // Overridden from TabStripModelObserver:
  virtual void TabInsertedAt(TabContentsWrapper* contents,
                             int index,
                             bool foreground) OVERRIDE;
  virtual void TabClosingAt(TabStripModel* tab_strip_model,
                            TabContentsWrapper* contents,
                            int index) OVERRIDE;
  virtual void TabDetachedAt(TabContentsWrapper* contents, int index) OVERRIDE;
  virtual void TabDeactivated(TabContentsWrapper* contents) OVERRIDE;
  virtual void ActiveTabChanged(TabContentsWrapper* old_contents,
                                TabContentsWrapper* new_contents,
                                int index,
                                bool user_gesture) OVERRIDE;
  virtual void TabMoved(TabContentsWrapper* contents,
                        int from_index,
                        int to_index) OVERRIDE;
  virtual void TabReplacedAt(TabStripModel* tab_strip_model,
                             TabContentsWrapper* old_contents,
                             TabContentsWrapper* new_contents,
                             int index) OVERRIDE;
  virtual void TabPinnedStateChanged(TabContentsWrapper* contents,
                                     int index) OVERRIDE;
  virtual void TabStripEmpty() OVERRIDE;

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

  ExtensionWindowController* extension_window_controller() const {
    return extension_window_controller_.get();
  }

 protected:
  // Wrapper for the factory method in BrowserWindow. This allows subclasses to
  // set their own window.
  virtual BrowserWindow* CreateBrowserWindow();

 private:
  friend class BrowserTest;
  FRIEND_TEST_ALL_PREFIXES(AppModeTest, EnableAppModeTest);
  FRIEND_TEST_ALL_PREFIXES(BrowserTest, NoTabsInPopups);
  FRIEND_TEST_ALL_PREFIXES(BrowserTest, ConvertTabToAppShortcut);
  FRIEND_TEST_ALL_PREFIXES(BrowserTest, OpenAppWindowLikeNtp);
  FRIEND_TEST_ALL_PREFIXES(BrowserTest, AppIdSwitch);
  FRIEND_TEST_ALL_PREFIXES(BrowserTest, TabEntersPresentationModeFromWindowed);
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
  virtual bool CanReloadContents(content::WebContents* source) const OVERRIDE;
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
  virtual bool PreHandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event,
      bool* is_keyboard_shortcut) OVERRIDE;
  virtual void HandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) OVERRIDE;
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
      content::WebContents* tab) OVERRIDE;
  virtual void DidNavigateToPendingEntry(content::WebContents* tab) OVERRIDE;
  virtual content::JavaScriptDialogCreator*
      GetJavaScriptDialogCreator() OVERRIDE;
  virtual content::ColorChooser* OpenColorChooser(content::WebContents* tab,
                                                  int color_chooser_id,
                                                  SkColor color) OVERRIDE;
  virtual void DidEndColorChooser() OVERRIDE;
  virtual void RunFileChooser(
      content::WebContents* tab,
      const content::FileChooserParams& params) OVERRIDE;
  virtual void EnumerateDirectory(content::WebContents* tab, int request_id,
                                  const FilePath& path) OVERRIDE;
  virtual void ToggleFullscreenModeForTab(content::WebContents* tab,
      bool enter_fullscreen) OVERRIDE;
  virtual bool IsFullscreenForTabOrPending(
      const content::WebContents* tab) const OVERRIDE;
  virtual void JSOutOfMemory(content::WebContents* tab) OVERRIDE;
  virtual void RegisterProtocolHandler(content::WebContents* tab,
                                       const std::string& protocol,
                                       const GURL& url,
                                       const string16& title) OVERRIDE;
  virtual void RegisterIntentHandler(content::WebContents* tab,
                                     const string16& action,
                                     const string16& type,
                                     const string16& href,
                                     const string16& title,
                                     const string16& disposition) OVERRIDE;
  virtual void WebIntentDispatch(
      content::WebContents* tab,
      content::WebIntentsDispatcher* intents_dispatcher) OVERRIDE;
  virtual void UpdatePreferredSize(content::WebContents* source,
                                   const gfx::Size& pref_size) OVERRIDE;
  virtual void ResizeDueToAutoResize(content::WebContents* source,
                                     const gfx::Size& new_size) OVERRIDE;

  virtual void FindReply(content::WebContents* tab,
                         int request_id,
                         int number_of_matches,
                         const gfx::Rect& selection_rect,
                         int active_match_ordinal,
                         bool final_update) OVERRIDE;

  virtual void RequestToLockMouse(content::WebContents* tab,
                                  bool user_gesture) OVERRIDE;
  virtual void LostMouseLock() OVERRIDE;

  // Overridden from CoreTabHelperDelegate:
  // Note that the caller is responsible for deleting |old_tab_contents|.
  virtual void SwapTabContents(TabContentsWrapper* old_tab_contents,
                               TabContentsWrapper* new_tab_contents) OVERRIDE;

  // Overridden from SearchEngineTabHelperDelegate:
  virtual void ConfirmAddSearchProvider(TemplateURL* template_url,
                                        Profile* profile) OVERRIDE;

  // Overridden from ConstrainedWindowTabHelperDelegate:
  virtual void SetTabContentBlocked(TabContentsWrapper* contents,
                                    bool blocked) OVERRIDE;

  // Overridden from BlockedContentTabHelperDelegate:
  virtual TabContentsWrapper* GetConstrainingContentsWrapper(
      TabContentsWrapper* source) OVERRIDE;

  // Overridden from BookmarkTabHelperDelegate:
  virtual void URLStarredChanged(TabContentsWrapper* source,
                                 bool starred) OVERRIDE;

  // Overridden from ExtensionTabHelperDelegate:
  virtual void OnDidGetApplicationInfo(TabContentsWrapper* source,
                                       int32 page_id) OVERRIDE;
  virtual void OnInstallApplication(
      TabContentsWrapper* source,
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
  virtual void ShowInstant(TabContentsWrapper* preview_contents) OVERRIDE;
  virtual void HideInstant() OVERRIDE;
  virtual void CommitInstant(TabContentsWrapper* preview_contents) OVERRIDE;
  virtual void SetSuggestedText(const string16& text,
                                InstantCompleteBehavior behavior) OVERRIDE;
  virtual gfx::Rect GetInstantBounds() OVERRIDE;
  virtual void InstantPreviewFocused() OVERRIDE;

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

  // Update commands whose state depends on whether the window is in fullscreen
  // mode.
  void UpdateCommandsForFullscreenMode(bool is_fullscreen);

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

  // Removes |tab| from the passed |set|.
  // Returns whether the tab was in the set in the first place.
  // TODO(beng): this method needs a better name!
  bool RemoveFromSet(UnloadListenerSet* set, content::WebContents* tab);

  // Cleans up state appropriately when we are trying to close the browser and
  // the tab has finished firing its unload handler. We also use this in the
  // cases where a tab crashes or hangs even if the beforeunload/unload haven't
  // successfully fired. If |process_now| is true |ProcessPendingTabs| is
  // invoked immediately, otherwise it is invoked after a delay (PostTask).
  //
  // Typically you'll want to pass in true for |process_now|. Passing in true
  // may result in deleting |tab|. If you know that shouldn't happen (because of
  // the state of the stack), pass in false.
  void ClearUnloadState(content::WebContents* tab, bool process_now);

  // In-progress download termination handling /////////////////////////////////

  // Called when the window is closing to check if potential in-progress
  // downloads should prevent it from closing.
  // Returns true if the window can close, false otherwise.
  bool CanCloseWithInProgressDownloads();

  // Assorted utility functions ///////////////////////////////////////////////

  // Sets the delegate of all the parts of the |TabContentsWrapper| that
  // are needed.
  void SetAsDelegate(TabContentsWrapper* tab, Browser* delegate);

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

  // Opens view-source tab for any frame within given tab contents.
  void ViewSource(TabContentsWrapper* tab,
                  const GURL& url,
                  const std::string& content_state);

  // Retrieves the content restrictions for the currently selected tab.
  // Returns 0 if no tab selected, which is equivalent to no content
  // restrictions active.
  int GetContentRestrictionsForSelectedTab();

  // Resets |bookmark_bar_state_| based on the active tab. Notifies the
  // BrowserWindow if necessary.
  void UpdateBookmarkBarState(BookmarkBarStateChangeReason reason);

  // Open the bookmark manager with a defined hash action.
  void OpenBookmarkManagerWithHash(const std::string& action, int64 node_id);

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
  ui::WindowShowState show_state_;

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

  // Different types of action when web app info is available.
  // OnDidGetApplicationInfo uses this to dispatch calls.
  enum WebAppAction {
    NONE,             // No action at all.
    CREATE_SHORTCUT,  // Bring up create application shortcut dialog.
    UPDATE_SHORTCUT   // Update icon for app shortcut.
  };

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
