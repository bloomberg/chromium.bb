// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_H_
#define CHROME_BROWSER_UI_BROWSER_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/prefs/public/pref_change_registrar.h"
#include "base/prefs/public/pref_member.h"
#include "base/string16.h"
#include "chrome/browser/devtools/devtools_toggle_action.h"
#include "chrome/browser/sessions/session_id.h"
#include "chrome/browser/ui/blocked_content/blocked_content_tab_helper_delegate.h"
#include "chrome/browser/ui/bookmarks/bookmark_bar.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper_delegate.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/search/search_model_observer.h"
#include "chrome/browser/ui/search_engines/search_engine_tab_helper_delegate.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/toolbar/toolbar_model.h"
#include "chrome/browser/ui/web_contents_modal_dialog_manager_delegate.h"
#include "chrome/browser/ui/zoom/zoom_observer.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/content_settings_types.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/common/page_zoom.h"
#include "ui/base/ui_base_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/rect.h"
#include "ui/shell_dialogs/select_file_dialog.h"

class BrowserContentSettingBubbleModelDelegate;
class BrowserSyncedWindowDelegate;
class BrowserToolbarModelDelegate;
class BrowserTabRestoreServiceDelegate;
class BrowserWindow;
class FindBarController;
class FullscreenController;
class PrefService;
class Profile;
class StatusBubble;
class TabNavigation;
class TabStripModel;
class TabStripModelDelegate;
struct WebApplicationInfo;

namespace chrome {
class BrowserCommandController;
class BrowserInstantController;
class UnloadController;
namespace search {
struct Mode;
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
class WindowController;
}

namespace gfx {
class Image;
class Point;
}

namespace ui {
struct SelectedFileInfo;
class WebDialogDelegate;
}

class Browser : public TabStripModelObserver,
                public content::WebContentsDelegate,
                public CoreTabHelperDelegate,
                public SearchEngineTabHelperDelegate,
                public WebContentsModalDialogManagerDelegate,
                public BlockedContentTabHelperDelegate,
                public BookmarkTabHelperDelegate,
                public ZoomObserver,
                public content::PageNavigator,
                public content::NotificationObserver,
                public ui::SelectFileDialog::Listener,
                public chrome::search::SearchModelObserver {
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

  struct CreateParams {
    CreateParams(Profile* profile, chrome::HostDesktopType host_desktop_type);
    CreateParams(Type type,
                 Profile* profile,
                 chrome::HostDesktopType host_desktop_type);

    static CreateParams CreateForApp(Type type,
                                     const std::string& app_name,
                                     const gfx::Rect& window_bounds,
                                     Profile* profile,
                                     chrome::HostDesktopType host_desktop_type);

    static CreateParams CreateForDevTools(
        Profile* profile,
        chrome::HostDesktopType host_desktop_type);

    // The browser type.
    Type type;

    // The associated profile.
    Profile* profile;

    // The host desktop the browser is created on.
    chrome::HostDesktopType host_desktop_type;

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

    // Supply a custom BrowserWindow implementation, to be used instead of the
    // default. Intended for testing.
    BrowserWindow* window;
  };

  // Constructors, Creation, Showing //////////////////////////////////////////

  explicit Browser(const CreateParams& params);
  virtual ~Browser();

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
  chrome::HostDesktopType host_desktop_type() const {
    return host_desktop_type_;
  }

  // Accessors ////////////////////////////////////////////////////////////////

  Type type() const { return type_; }
  const std::string& app_name() const { return app_name_; }
  AppType app_type() const { return app_type_; }
  Profile* profile() const { return profile_; }
  gfx::Rect override_bounds() const { return override_bounds_; }

  // |window()| will return NULL if called before |CreateBrowserWindow()|
  // is done.
  BrowserWindow* window() const { return window_; }
  ToolbarModel* toolbar_model() { return toolbar_model_.get(); }
  const ToolbarModel* toolbar_model() const { return toolbar_model_.get(); }
  TabStripModel* tab_strip_model() const { return tab_strip_model_.get(); }
  chrome::BrowserCommandController* command_controller() {
    return command_controller_.get();
  }
  chrome::search::SearchModel* search_model() { return search_model_.get(); }
  const chrome::search::SearchModel* search_model() const {
      return search_model_.get();
  }
  chrome::search::SearchDelegate* search_delegate() {
    return search_delegate_.get();
  }
  const SessionID& session_id() const { return session_id_; }
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
  chrome::BrowserInstantController* instant_controller() {
    return instant_controller_.get();
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
  gfx::Image GetCurrentPageIcon() const;

  // Gets the title of the window based on the selected tab's title.
  string16 GetWindowTitleForCurrentTab() const;

  // Prepares a title string for display (removes embedded newlines, etc).
  static void FormatTitleForDisplay(string16* title);

  // OnBeforeUnload handling //////////////////////////////////////////////////

  // Gives beforeunload handlers the chance to cancel the close.
  bool ShouldCloseWindow();

  bool IsAttemptingToCloseBrowser() const;

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

  // Tab adding/showing functions /////////////////////////////////////////////

  // Invoked when the fullscreen state of the window changes.
  // BrowserWindow::EnterFullscreen invokes this after the window has become
  // fullscreen.
  void WindowFullscreenStateChanged();

  // Invoked when visible SSL state (as defined by SSLStatus) changes.
  void VisibleSSLStateChanged(content::WebContents* web_contents);

  // Assorted browser commands ////////////////////////////////////////////////

  // NOTE: Within each of the following sections, the IDs are ordered roughly by
  // how they appear in the GUI/menus (left to right, top to bottom, etc.).

  // See the description of
  // FullscreenController::ToggleFullscreenModeWithExtension.
  void ToggleFullscreenModeWithExtension(const GURL& extension_url);
#if defined(OS_WIN)
  // See the description of FullscreenController::ToggleMetroSnapMode.
  void SetMetroSnapMode(bool enable);
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

  void UpdateDownloadShelfVisibility(bool visible);

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
                                            bool user_gesture,
                                            BrowserWindow* window);

  // Helper function to handle find results.
  static void FindReplyHelper(content::WebContents* web_contents,
                              int request_id,
                              int number_of_matches,
                              const gfx::Rect& selection_rect,
                              int active_match_ordinal,
                              bool final_update);

  // Called by chrome::Navigate() when a navigation has occurred in a tab in
  // this Browser. Updates the UI for the start of this navigation.
  void UpdateUIForNavigationInTab(content::WebContents* contents,
                                  content::PageTransition transition,
                                  bool user_initiated);

  // Interface implementations ////////////////////////////////////////////////

  // Overridden from content::PageNavigator:
  virtual content::WebContents* OpenURL(
      const content::OpenURLParams& params) OVERRIDE;

  // Overridden from TabStripModelObserver:
  virtual void TabInsertedAt(content::WebContents* contents,
                             int index,
                             bool foreground) OVERRIDE;
  virtual void TabClosingAt(TabStripModel* tab_strip_model,
                            content::WebContents* contents,
                            int index) OVERRIDE;
  virtual void TabDetachedAt(content::WebContents* contents,
                             int index) OVERRIDE;
  virtual void TabDeactivated(content::WebContents* contents) OVERRIDE;
  virtual void ActiveTabChanged(content::WebContents* old_contents,
                                content::WebContents* new_contents,
                                int index,
                                bool user_gesture) OVERRIDE;
  virtual void TabMoved(content::WebContents* contents,
                        int from_index,
                        int to_index) OVERRIDE;
  virtual void TabReplacedAt(TabStripModel* tab_strip_model,
                             content::WebContents* old_contents,
                             content::WebContents* new_contents,
                             int index) OVERRIDE;
  virtual void TabPinnedStateChanged(content::WebContents* contents,
                                     int index) OVERRIDE;
  virtual void TabStripEmpty() OVERRIDE;

  // Overridden from content::WebContentsDelegate:
  virtual bool CanOverscrollContent() const OVERRIDE;
  virtual bool PreHandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event,
      bool* is_keyboard_shortcut) OVERRIDE;
  virtual void HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) OVERRIDE;

  // Figure out if there are tabs that have beforeunload handlers.
  bool TabsNeedBeforeUnloadFired();

  bool is_type_tabbed() const { return type_ == TYPE_TABBED; }
  bool is_type_popup() const { return type_ == TYPE_POPUP; }
  bool is_type_panel() const { return type_ == TYPE_PANEL; }

  bool is_app() const;
  bool is_devtools() const;

  // True when the mouse cursor is locked.
  bool IsMouseLocked() const;

  // Called each time the browser window is shown.
  void OnWindowDidShow();

  // Show the first run search engine bubble on the location bar.
  void ShowFirstRunBubble();

  // If necessary, update the bookmark bar state according to the instant
  // preview state: when instant preview shows suggestions and bookmark bar is
  // still showing attached, hide it.
  void MaybeUpdateBookmarkBarStateForInstantPreview(
      const chrome::search::Mode& mode);

  FullscreenController* fullscreen_controller() const {
    return fullscreen_controller_.get();
  }

  extensions::WindowController* extension_window_controller() const {
    return extension_window_controller_.get();
  }

 private:
  friend class BrowserTest;
  friend class FullscreenControllerInteractiveTest;
  friend class FullscreenControllerTest;
  FRIEND_TEST_ALL_PREFIXES(AppModeTest, EnableAppModeTest);
  FRIEND_TEST_ALL_PREFIXES(BrowserCommandControllerTest,
                           IsReservedCommandOrKeyIsApp);
  FRIEND_TEST_ALL_PREFIXES(BrowserCommandControllerTest, AppFullScreen);
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
                              bool user_gesture,
                              bool* was_blocked) OVERRIDE;
  virtual void ActivateContents(content::WebContents* contents) OVERRIDE;
  virtual void DeactivateContents(content::WebContents* contents) OVERRIDE;
  virtual void LoadingStateChanged(content::WebContents* source) OVERRIDE;
  virtual void CloseContents(content::WebContents* source) OVERRIDE;
  virtual void MoveContents(content::WebContents* source,
                            const gfx::Rect& pos) OVERRIDE;
  virtual bool IsPopupOrPanel(
      const content::WebContents* source) const OVERRIDE;
  virtual void UpdateTargetURL(content::WebContents* source, int32 page_id,
                               const GURL& url) OVERRIDE;
  virtual void ContentsMouseEvent(content::WebContents* source,
                                  const gfx::Point& location,
                                  bool motion) OVERRIDE;
  virtual void ContentsZoomChange(bool zoom_in) OVERRIDE;
  virtual void WebContentsFocused(content::WebContents* content) OVERRIDE;
  virtual bool TakeFocus(content::WebContents* source, bool reverse) OVERRIDE;
  virtual gfx::Rect GetRootWindowResizerRect() const OVERRIDE;
  virtual void BeforeUnloadFired(content::WebContents* source,
                                 bool proceed,
                                 bool* proceed_to_fire_unload) OVERRIDE;
  virtual bool ShouldFocusLocationBarByDefault(
      content::WebContents* source) OVERRIDE;
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
  virtual content::JavaScriptDialogManager*
      GetJavaScriptDialogManager() OVERRIDE;
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
                                  const base::FilePath& path) OVERRIDE;
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
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback) OVERRIDE;
  virtual bool RequestPpapiBrokerPermission(
      content::WebContents* web_contents,
      const GURL& url,
      const base::FilePath& plugin_path,
      const base::Callback<void(bool)>& callback) OVERRIDE;

  // Overridden from CoreTabHelperDelegate:
  // Note that the caller is responsible for deleting |old_contents|.
  virtual void SwapTabContents(content::WebContents* old_contents,
                               content::WebContents* new_contents) OVERRIDE;
  virtual bool CanReloadContents(
      content::WebContents* web_contents) const OVERRIDE;
  virtual bool CanSaveContents(
      content::WebContents* web_contents) const OVERRIDE;

  // Overridden from SearchEngineTabHelperDelegate:
  virtual void ConfirmAddSearchProvider(TemplateURL* template_url,
                                        Profile* profile) OVERRIDE;

  // Overridden from WebContentsModalDialogManagerDelegate:
  virtual void SetWebContentsBlocked(content::WebContents* web_contents,
                                     bool blocked) OVERRIDE;
  virtual bool GetDialogTopCenter(gfx::Point* point) OVERRIDE;

  // Overridden from BlockedContentTabHelperDelegate:
  virtual content::WebContents* GetConstrainingWebContents(
      content::WebContents* source) OVERRIDE;

  // Overridden from BookmarkTabHelperDelegate:
  virtual void URLStarredChanged(content::WebContents* web_contents,
                                 bool starred) OVERRIDE;

  // Overridden from ZoomObserver:
  virtual void OnZoomChanged(content::WebContents* source,
                             bool can_show_bubble) OVERRIDE;

  // Overridden from SelectFileDialog::Listener:
  virtual void FileSelected(const base::FilePath& path,
                            int index,
                            void* params) OVERRIDE;
  virtual void FileSelectedWithExtraInfo(
      const ui::SelectedFileInfo& file_info,
      int index,
      void* params) OVERRIDE;

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Overridden from chrome::search::SearchModelObserver:
  virtual void ModeChanged(const chrome::search::Mode& old_mode,
                           const chrome::search::Mode& new_mode) OVERRIDE;

  // Command and state updating ///////////////////////////////////////////////

  // Handle changes to kDevTools preference.
  void OnDevToolsDisabledChanged();

  // Set the preference that indicates that the home page has been changed.
  void MarkHomePageAsChanged();

  // UI update coalescing and handling ////////////////////////////////////////

  // Asks the toolbar (and as such the location bar) to update its state to
  // reflect the current tab's current URL, security state, etc.
  // If |should_restore_state| is true, we're switching (back?) to this tab and
  // should restore any previous location bar state (such as user editing) as
  // well.
  void UpdateToolbar(bool should_restore_state);

  // Updates the browser's search model with the tab's search model.
  void UpdateSearchState(content::WebContents* contents);

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

  // In-progress download termination handling /////////////////////////////////

  // Called when the window is closing to check if potential in-progress
  // downloads should prevent it from closing.
  // Returns true if the window can close, false otherwise.
  bool CanCloseWithInProgressDownloads();

  // Assorted utility functions ///////////////////////////////////////////////

  // Sets the specified browser as the delegate of the WebContents and all the
  // associated tab helpers that are needed.
  void SetAsDelegate(content::WebContents* web_contents, Browser* delegate);

  // Shows the Find Bar, optionally selecting the next entry that matches the
  // existing search string for that Tab. |forward_direction| controls the
  // search direction.
  void FindInPage(bool find_next, bool forward_direction);

  // Closes the frame.
  // TODO(beng): figure out if we need this now that the frame itself closes
  //             after a return to the message loop.
  void CloseFrame();

  void TabDetachedAtImpl(content::WebContents* contents,
                         int index,
                         DetachType type);

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

  // Resets |bookmark_bar_state_| based on the active tab. Notifies the
  // BrowserWindow if necessary.
  void UpdateBookmarkBarState(BookmarkBarStateChangeReason reason);

  bool ShouldHideUIForFullscreen() const;

  // Creates a BackgroundContents if appropriate; return true if one was
  // created.
  bool MaybeCreateBackgroundContents(int route_id,
                                     content::WebContents* opener_web_contents,
                                     const string16& frame_name,
                                     const GURL& target_url);

  // Data members /////////////////////////////////////////////////////////////

  content::NotificationRegistrar registrar_;

  PrefChangeRegistrar profile_pref_registrar_;

  // This Browser's type.
  const Type type_;

  // This Browser's profile.
  Profile* const profile_;

  // This Browser's window.
  BrowserWindow* window_;

  scoped_ptr<TabStripModelDelegate> tab_strip_model_delegate_;
  scoped_ptr<TabStripModel> tab_strip_model_;

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

  const chrome::HostDesktopType host_desktop_type_;

  scoped_ptr<chrome::UnloadController> unload_controller_;

  // The following factory is used to close the frame at a later time.
  base::WeakPtrFactory<Browser> weak_factory_;

  // The Find Bar. This may be NULL if there is no Find Bar, and if it is
  // non-NULL, it may or may not be visible.
  scoped_ptr<FindBarController> find_bar_controller_;

  // Dialog box used for opening and saving files.
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;

  // Keep track of the encoding auto detect pref.
  BooleanPrefMember encoding_auto_detect_;

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

  scoped_ptr<chrome::BrowserInstantController> instant_controller_;

  BookmarkBar::State bookmark_bar_state_;

  scoped_ptr<FullscreenController> fullscreen_controller_;

  scoped_ptr<extensions::WindowController> extension_window_controller_;

  scoped_ptr<chrome::BrowserCommandController> command_controller_;

  // True if the browser window has been shown at least once.
  bool window_has_shown_;

  // Currently open color chooser. Non-NULL after OpenColorChooser is called and
  // before DidEndColorChooser is called.
  scoped_ptr<content::ColorChooser> color_chooser_;

  DISALLOW_COPY_AND_ASSIGN(Browser);
};

#endif  // CHROME_BROWSER_UI_BROWSER_H_
