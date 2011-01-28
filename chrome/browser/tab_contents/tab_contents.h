// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_TAB_CONTENTS_H_
#define CHROME_BROWSER_TAB_CONTENTS_TAB_CONTENTS_H_
#pragma once

#include <deque>
#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/scoped_ptr.h"
#include "base/string16.h"
#include "chrome/browser/dom_ui/dom_ui_factory.h"
#include "chrome/browser/download/save_package.h"
#include "chrome/browser/extensions/image_loading_tracker.h"
#include "chrome/browser/fav_icon_helper.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "chrome/browser/renderer_host/render_view_host_delegate.h"
#include "chrome/browser/tab_contents/constrained_window.h"
#include "chrome/browser/tab_contents/language_state.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/page_navigator.h"
#include "chrome/browser/tab_contents/render_view_host_manager.h"
#include "chrome/browser/tab_contents/tab_specific_content_settings.h"
#include "chrome/browser/ui/app_modal_dialogs/js_modal_dialog.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/find_bar/find_notification_details.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/property_bag.h"
#include "chrome/common/renderer_preferences.h"
#include "chrome/common/translate_errors.h"
#include "chrome/common/web_apps.h"
#include "gfx/native_widget_types.h"
#include "net/base/load_states.h"

#if defined(OS_WIN)
#include "base/win/scoped_handle.h"
#endif

namespace gfx {
class Rect;
}

namespace history {
class HistoryAddPageArgs;
}

namespace printing {
class PrintViewManager;
}

class AutocompleteHistoryManager;
class AutoFillManager;
class BlockedContentContainer;
class DOMUI;
class DesktopNotificationHandler;
class DownloadItem;
class Extension;
class FileSelectHelper;
class InfoBarDelegate;
class LoadNotificationDetails;
class OmniboxSearchHint;
class PluginInstallerInfoBarDelegate;
class Profile;
class PrerenderManager;
class PrerenderPLTRecorder;
class RenderViewHost;
class SessionStorageNamespace;
class SiteInstance;
class SkBitmap;
class TabContents;
class TabContentsDelegate;
class TabContentsObserver;
class TabContentsSSLHelper;
class TabContentsView;
class URLPattern;
struct RendererPreferences;
struct ThumbnailScore;
struct ViewHostMsg_DomMessage_Params;
struct ViewHostMsg_FrameNavigate_Params;
struct WebPreferences;

// Describes what goes in the main content area of a tab. TabContents is
// the only type of TabContents, and these should be merged together.
class TabContents : public PageNavigator,
                    public NotificationObserver,
                    public RenderViewHostDelegate,
                    public RenderViewHostManager::Delegate,
                    public JavaScriptAppModalDialogDelegate,
                    public ImageLoadingTracker::Observer,
                    public TabSpecificContentSettings::Delegate {
 public:
  // Flags passed to the TabContentsDelegate.NavigationStateChanged to tell it
  // what has changed. Combine them to update more than one thing.
  enum InvalidateTypes {
    INVALIDATE_URL             = 1 << 0,  // The URL has changed.
    INVALIDATE_TAB             = 1 << 1,  // The favicon, app icon, or crashed
                                          // state changed.
    INVALIDATE_LOAD            = 1 << 2,  // The loading state has changed.
    INVALIDATE_PAGE_ACTIONS    = 1 << 3,  // Page action icons have changed.
    INVALIDATE_BOOKMARK_BAR    = 1 << 4,  // State of ShouldShowBookmarkBar
                                          // changed.
    INVALIDATE_TITLE           = 1 << 5,  // The title changed.
  };

  // |base_tab_contents| is used if we want to size the new tab contents view
  // based on an existing tab contents view.  This can be NULL if not needed.
  //
  // The session storage namespace parameter allows multiple render views and
  // tab contentses to share the same session storage (part of the WebStorage
  // spec) space. This is useful when restoring tabs, but most callers should
  // pass in NULL which will cause a new SessionStorageNamespace to be created.
  TabContents(Profile* profile,
              SiteInstance* site_instance,
              int routing_id,
              const TabContents* base_tab_contents,
              SessionStorageNamespace* session_storage_namespace);
  virtual ~TabContents();

  static void RegisterUserPrefs(PrefService* prefs);

  // Intrinsic tab state -------------------------------------------------------

  // Returns the property bag for this tab contents, where callers can add
  // extra data they may wish to associate with the tab. Returns a pointer
  // rather than a reference since the PropertyAccessors expect this.
  const PropertyBag* property_bag() const { return &property_bag_; }
  PropertyBag* property_bag() { return &property_bag_; }

  TabContentsDelegate* delegate() const { return delegate_; }
  void set_delegate(TabContentsDelegate* d) { delegate_ = d; }

  // Gets the controller for this tab contents.
  NavigationController& controller() { return controller_; }
  const NavigationController& controller() const { return controller_; }

  // Returns the user profile associated with this TabContents (via the
  // NavigationController).
  Profile* profile() const { return controller_.profile(); }

  // Returns true if contains content rendered by an extension.
  bool HostsExtension() const;

  // Returns the PluginInstallerInfoBarDelegate, creating it if necessary.
  PluginInstallerInfoBarDelegate* GetPluginInstaller();

  // Returns the TabContentsSSLHelper, creating it if necessary.
  TabContentsSSLHelper* GetSSLHelper();

  // Returns the SavePackage which manages the page saving job. May be NULL.
  SavePackage* save_package() const { return save_package_.get(); }

  // Return the currently active RenderProcessHost and RenderViewHost. Each of
  // these may change over time.
  RenderProcessHost* GetRenderProcessHost() const;
  RenderViewHost* render_view_host() const {
    return render_manager_.current_host();
  }

  DOMUI* dom_ui() const {
    return render_manager_.dom_ui() ? render_manager_.dom_ui()
        : render_manager_.pending_dom_ui();
  }

  // Returns the currently active RenderWidgetHostView. This may change over
  // time and can be NULL (during setup and teardown).
  RenderWidgetHostView* GetRenderWidgetHostView() const {
    return render_manager_.GetRenderWidgetHostView();
  }

  // The TabContentsView will never change and is guaranteed non-NULL.
  TabContentsView* view() const {
    return view_.get();
  }

  // Returns the FavIconHelper of this TabContents.
  FavIconHelper& fav_icon_helper() {
    return fav_icon_helper_;
  }

  // App extensions ------------------------------------------------------------

  // Sets the extension denoting this as an app. If |extension| is non-null this
  // tab becomes an app-tab. TabContents does not listen for unload events for
  // the extension. It's up to consumers of TabContents to do that.
  //
  // NOTE: this should only be manipulated before the tab is added to a browser.
  // TODO(sky): resolve if this is the right way to identify an app tab. If it
  // is, than this should be passed in the constructor.
  void SetExtensionApp(const Extension* extension);

  // Convenience for setting the app extension by id. This does nothing if
  // |extension_app_id| is empty, or an extension can't be found given the
  // specified id.
  void SetExtensionAppById(const std::string& extension_app_id);

  const Extension* extension_app() const { return extension_app_; }
  bool is_app() const { return extension_app_ != NULL; }

  // If an app extension has been explicitly set for this TabContents its icon
  // is returned.
  //
  // NOTE: the returned icon is larger than 16x16 (it's size is
  // Extension::EXTENSION_ICON_SMALLISH).
  SkBitmap* GetExtensionAppIcon();

  // Tab navigation state ------------------------------------------------------

  // Returns the current navigation properties, which if a navigation is
  // pending may be provisional (e.g., the navigation could result in a
  // download, in which case the URL would revert to what it was previously).
  virtual const GURL& GetURL() const;
  virtual const string16& GetTitle() const;

  // Initial title assigned to NavigationEntries from Navigate.
  static string16 GetDefaultTitle();

  // The max PageID of any page that this TabContents has loaded.  PageIDs
  // increase with each new page that is loaded by a tab.  If this is a
  // TabContents, then the max PageID is kept separately on each SiteInstance.
  // Returns -1 if no PageIDs have yet been seen.
  int32 GetMaxPageID();

  // Updates the max PageID to be at least the given PageID.
  void UpdateMaxPageID(int32 page_id);

  // Returns the site instance associated with the current page. By default,
  // there is no site instance. TabContents overrides this to provide proper
  // access to its site instance.
  virtual SiteInstance* GetSiteInstance() const;

  // Defines whether this tab's URL should be displayed in the browser's URL
  // bar. Normally this is true so you can see the URL. This is set to false
  // for the new tab page and related pages so that the URL bar is empty and
  // the user is invited to type into it.
  virtual bool ShouldDisplayURL();

  // Returns the favicon for this tab, or an isNull() bitmap if the tab does not
  // have a favicon. The default implementation uses the current navigation
  // entry.
  SkBitmap GetFavIcon() const;

  // Returns true if we are not using the default favicon.
  bool FavIconIsValid() const;

  // Returns whether the favicon should be displayed. If this returns false, no
  // space is provided for the favicon, and the favicon is never displayed.
  virtual bool ShouldDisplayFavIcon();

  // Returns a human-readable description the tab's loading state.
  virtual string16 GetStatusText() const;

  // Add and remove observers for page navigation notifications. Adding or
  // removing multiple times has no effect. The order in which notifications
  // are sent to observers is undefined. Clients must be sure to remove the
  // observer before they go away.
  void AddObserver(TabContentsObserver* observer);
  void RemoveObserver(TabContentsObserver* observer);

  // Return whether this tab contents is loading a resource.
  bool is_loading() const { return is_loading_; }

  // Returns whether this tab contents is waiting for a first-response for the
  // main resource of the page. This controls whether the throbber state is
  // "waiting" or "loading."
  bool waiting_for_response() const { return waiting_for_response_; }

  bool is_starred() const { return is_starred_; }

  const std::string& encoding() const { return encoding_; }
  void set_encoding(const std::string& encoding);
  void reset_encoding() {
    encoding_.clear();
  }

  const WebApplicationInfo& web_app_info() const {
    return web_app_info_;
  }

  const SkBitmap& app_icon() const { return app_icon_; }

  // Sets an app icon associated with TabContents and fires an INVALIDATE_TITLE
  // navigation state change to trigger repaint of title.
  void SetAppIcon(const SkBitmap& app_icon);

  bool displayed_insecure_content() const {
    return displayed_insecure_content_;
  }

  // Internal state ------------------------------------------------------------

  // This flag indicates whether the tab contents is currently being
  // screenshotted by the DraggedTabController.
  bool capturing_contents() const { return capturing_contents_; }
  void set_capturing_contents(bool cap) { capturing_contents_ = cap; }

  // Indicates whether this tab should be considered crashed. The setter will
  // also notify the delegate when the flag is changed.
  bool is_crashed() const {
    return (crashed_status_ == base::TERMINATION_STATUS_PROCESS_CRASHED ||
            crashed_status_ == base::TERMINATION_STATUS_ABNORMAL_TERMINATION ||
            crashed_status_ == base::TERMINATION_STATUS_PROCESS_WAS_KILLED);
  }
  base::TerminationStatus crashed_status() const { return crashed_status_; }
  int crashed_error_code() const { return crashed_error_code_; }
  void SetIsCrashed(base::TerminationStatus status, int error_code);

  // Call this after updating a page action to notify clients about the changes.
  void PageActionStateChanged();

  // Whether the tab is in the process of being destroyed.
  // Added as a tentative work-around for focus related bug #4633.  This allows
  // us not to store focus when a tab is being closed.
  bool is_being_destroyed() const { return is_being_destroyed_; }

  // Convenience method for notifying the delegate of a navigation state
  // change. See TabContentsDelegate.
  void NotifyNavigationStateChanged(unsigned changed_flags);

  // Invoked when the tab contents becomes selected. If you override, be sure
  // and invoke super's implementation.
  virtual void DidBecomeSelected();
  base::TimeTicks last_selected_time() const {
    return last_selected_time_;
  }

  // Invoked when the tab contents becomes hidden.
  // NOTE: If you override this, call the superclass version too!
  virtual void WasHidden();

  // Activates this contents within its containing window, bringing that window
  // to the foreground if necessary.
  void Activate();

  // Deactivates this contents by deactivating its containing window.
  void Deactivate();

  // TODO(brettw) document these.
  virtual void ShowContents();
  virtual void HideContents();

  // Returns true if the before unload and unload listeners need to be
  // fired. The value of this changes over time. For example, if true and the
  // before unload listener is executed and allows the user to exit, then this
  // returns false.
  bool NeedToFireBeforeUnload();

#ifdef UNIT_TEST
  // Expose the render manager for testing.
  RenderViewHostManager* render_manager() { return &render_manager_; }
#endif

  // In the underlying RenderViewHostManager, swaps in the provided
  // RenderViewHost to replace the current RenderViewHost.  The current RVH
  // will be shutdown and ultimately deleted.
  void SwapInRenderViewHost(RenderViewHost* rvh);

  // Commands ------------------------------------------------------------------

  // Implementation of PageNavigator.
  virtual void OpenURL(const GURL& url, const GURL& referrer,
                       WindowOpenDisposition disposition,
                       PageTransition::Type transition);

  // Called by the NavigationController to cause the TabContents to navigate to
  // the current pending entry. The NavigationController should be called back
  // with CommitPendingEntry/RendererDidNavigate on success or
  // DiscardPendingEntry. The callbacks can be inside of this function, or at
  // some future time.
  //
  // The entry has a PageID of -1 if newly created (corresponding to navigation
  // to a new URL).
  //
  // If this method returns false, then the navigation is discarded (equivalent
  // to calling DiscardPendingEntry on the NavigationController).
  virtual bool NavigateToPendingEntry(
      NavigationController::ReloadType reload_type);

  // Stop any pending navigation.
  virtual void Stop();

  // Called on a TabContents when it isn't a popup, but a new window.
  virtual void DisassociateFromPopupCount();

  // Creates a new TabContents with the same state as this one. The returned
  // heap-allocated pointer is owned by the caller.
  virtual TabContents* Clone();

  // Shows the page info.
  void ShowPageInfo(const GURL& url,
                    const NavigationEntry::SSLStatus& ssl,
                    bool show_history);

  // Saves the favicon for the current page.
  void SaveFavicon();

  // Window management ---------------------------------------------------------

  // Create a new window constrained to this TabContents' clip and visibility.
  // The window is initialized by using the supplied delegate to obtain basic
  // window characteristics, and the supplied view for the content. Note that
  // the returned ConstrainedWindow might not yet be visible.
  ConstrainedWindow* CreateConstrainedDialog(
      ConstrainedWindowDelegate* delegate);

  // Adds a new tab or window with the given already-created contents
  void AddNewContents(TabContents* new_contents,
                      WindowOpenDisposition disposition,
                      const gfx::Rect& initial_pos,
                      bool user_gesture);

  // Execute code in this tab. Returns true if the message was successfully
  // sent.
  bool ExecuteCode(int request_id, const std::string& extension_id,
                   bool is_js_code, const std::string& code_string,
                   bool all_frames);

  // Called when the blocked popup notification is shown or hidden.
  virtual void PopupNotificationVisibilityChanged(bool visible);

  // Returns the number of constrained windows in this tab.  Used by tests.
  size_t constrained_window_count() { return child_windows_.size(); }

  typedef std::deque<ConstrainedWindow*> ConstrainedWindowList;

  // Return an iterator for the first constrained window in this tab contents.
  ConstrainedWindowList::iterator constrained_window_begin()
  { return child_windows_.begin(); }

  // Return an iterator for the last constrained window in this tab contents.
  ConstrainedWindowList::iterator constrained_window_end()
  { return child_windows_.end(); }

  // Views and focus -----------------------------------------------------------
  // TODO(brettw): Most of these should be removed and the caller should call
  // the view directly.

  // Returns the actual window that is focused when this TabContents is shown.
  gfx::NativeView GetContentNativeView() const;

  // Returns the NativeView associated with this TabContents. Outside of
  // automation in the context of the UI, this is required to be implemented.
  gfx::NativeView GetNativeView() const;

  // Returns the bounds of this TabContents in the screen coordinate system.
  void GetContainerBounds(gfx::Rect *out) const;

  // Makes the tab the focused window.
  void Focus();

  // Focuses the first (last if |reverse| is true) element in the page.
  // Invoked when this tab is getting the focus through tab traversal (|reverse|
  // is true when using Shift-Tab).
  void FocusThroughTabTraversal(bool reverse);

  // These next two functions are declared on RenderViewHostManager::Delegate
  // but also accessed directly by other callers.

  // Returns true if the location bar should be focused by default rather than
  // the page contents. The view calls this function when the tab is focused
  // to see what it should do.
  virtual bool FocusLocationBarByDefault();

  // Focuses the location bar.
  virtual void SetFocusToLocationBar(bool select_all);

  // Creates a view and sets the size for the specified RVH.
  virtual void CreateViewAndSetSizeForRVH(RenderViewHost* rvh);

  // Infobars ------------------------------------------------------------------

  // Adds an InfoBar for the specified |delegate|.
  virtual void AddInfoBar(InfoBarDelegate* delegate);

  // Removes the InfoBar for the specified |delegate|.
  void RemoveInfoBar(InfoBarDelegate* delegate);

  // Replaces one infobar with another, without any animation in between.
  void ReplaceInfoBar(InfoBarDelegate* old_delegate,
                      InfoBarDelegate* new_delegate);

  // Enumeration and access functions.
  int infobar_delegate_count() const { return infobar_delegates_.size(); }
  InfoBarDelegate* GetInfoBarDelegateAt(int index) {
    return infobar_delegates_.at(index);
  }

  // Toolbars and such ---------------------------------------------------------

  // Returns true if a Bookmark Bar should be shown for this tab.
  virtual bool ShouldShowBookmarkBar();

  // Notifies the delegate that a download is about to be started.
  // This notification is fired before a local temporary file has been created.
  bool CanDownload(int request_id);

  // Notifies the delegate that a download started.
  void OnStartDownload(DownloadItem* download);

  // Notify our delegate that some of our content has animated.
  void ToolbarSizeChanged(bool is_animating);

  // Called when a ConstrainedWindow we own is about to be closed.
  void WillClose(ConstrainedWindow* window);

  // Called when a BlockedContentContainer we own is about to be closed.
  void WillCloseBlockedContentContainer(BlockedContentContainer* container);

  // Called when a ConstrainedWindow we own is moved or resized.
  void DidMoveOrResize(ConstrainedWindow* window);

  // Interstitials -------------------------------------------------------------

  // Various other systems need to know about our interstitials.
  bool showing_interstitial_page() const {
    return render_manager_.interstitial_page() != NULL;
  }

  // Sets the passed passed interstitial as the currently showing interstitial.
  // |interstitial_page| should be non NULL (use the remove_interstitial_page
  // method to unset the interstitial) and no interstitial page should be set
  // when there is already a non NULL interstitial page set.
  void set_interstitial_page(InterstitialPage* interstitial_page) {
    render_manager_.set_interstitial_page(interstitial_page);
  }

  // Unsets the currently showing interstitial.
  void remove_interstitial_page() {
    render_manager_.remove_interstitial_page();
  }

  // Returns the currently showing interstitial, NULL if no interstitial is
  // showing.
  InterstitialPage* interstitial_page() const {
    return render_manager_.interstitial_page();
  }

  // Find in Page --------------------------------------------------------------

  // Starts the Find operation by calling StartFinding on the Tab. This function
  // can be called from the outside as a result of hot-keys, so it uses the
  // last remembered search string as specified with set_find_string(). This
  // function does not block while a search is in progress. The controller will
  // receive the results through the notification mechanism. See Observe(...)
  // for details.
  void StartFinding(string16 search_string,
                    bool forward_direction,
                    bool case_sensitive);

  // Stops the current Find operation.
  void StopFinding(FindBarController::SelectionAction selection_action);

  // Accessors/Setters for find_ui_active_.
  bool find_ui_active() const { return find_ui_active_; }
  void set_find_ui_active(bool find_ui_active) {
      find_ui_active_ = find_ui_active;
  }

  // Setter for find_op_aborted_.
  void set_find_op_aborted(bool find_op_aborted) {
    find_op_aborted_ = find_op_aborted;
  }

  // Used _only_ by testing to get or set the current request ID.
  int current_find_request_id() { return current_find_request_id_; }
  void set_current_find_request_id(int current_find_request_id) {
    current_find_request_id_ = current_find_request_id;
  }

  // Accessor for find_text_. Used to determine if this TabContents has any
  // active searches.
  string16 find_text() const { return find_text_; }

  // Accessor for the previous search we issued.
  string16 previous_find_text() const { return previous_find_text_; }

  // Accessor for find_result_.
  const FindNotificationDetails& find_result() const {
    return last_search_result_;
  }

  // Misc state & callbacks ----------------------------------------------------

  // Set whether the contents should block javascript message boxes or not.
  // Default is not to block any message boxes.
  void set_suppress_javascript_messages(bool suppress_javascript_messages) {
    suppress_javascript_messages_ = suppress_javascript_messages;
  }

  // Prepare for saving the current web page to disk.
  void OnSavePage();

  // Save page with the main HTML file path, the directory for saving resources,
  // and the save type: HTML only or complete web page. Returns true if the
  // saving process has been initiated successfully.
  bool SavePage(const FilePath& main_file, const FilePath& dir_path,
                SavePackage::SavePackageType save_type);

  // Sets save_package_, taking care to register and unregister the observers.
  void SetSavePackage(SavePackage* save_package);

  // Tells the user's email client to open a compose window containing the
  // current page's URL.
  void EmailPageLocation();

  // Displays asynchronously a print preview (generated by the renderer) if not
  // already displayed and ask the user for its preferred print settings with
  // the "Print..." dialog box. (managed by the print worker thread).
  // TODO(maruel):  Creates a snapshot of the renderer to be used for the new
  // tab for the printing facility.
  void PrintPreview();

  // Prints the current document immediately. Since the rendering is
  // asynchronous, the actual printing will not be completed on the return of
  // this function. Returns false if printing is impossible at the moment.
  bool PrintNow();

  // Notify the completion of a printing job.
  void PrintingDone(int document_cookie, bool success);

  // Returns true if the active NavigationEntry's page_id equals page_id.
  bool IsActiveEntry(int32 page_id);

  const std::string& contents_mime_type() const {
    return contents_mime_type_;
  }

  // Returns true if this TabContents will notify about disconnection.
  bool notify_disconnection() const { return notify_disconnection_; }

  // Override the encoding and reload the page by sending down
  // ViewMsg_SetPageEncoding to the renderer. |UpdateEncoding| is kinda
  // the opposite of this, by which 'browser' is notified of
  // the encoding of the current tab from 'renderer' (determined by
  // auto-detect, http header, meta, bom detection, etc).
  void SetOverrideEncoding(const std::string& encoding);

  // Remove any user-defined override encoding and reload by sending down
  // ViewMsg_ResetPageEncodingToDefault to the renderer.
  void ResetOverrideEncoding();

  void WindowMoveOrResizeStarted();

  // Sets whether all TabContents added by way of |AddNewContents| should be
  // blocked. Transitioning from all blocked to not all blocked results in
  // reevaluating any blocked TabContents, which may result in unblocking some
  // of the blocked TabContents.
  void SetAllContentsBlocked(bool value);

  BlockedContentContainer* blocked_content_container() const {
    return blocked_contents_;
  }

  RendererPreferences* GetMutableRendererPrefs() {
    return &renderer_preferences_;
  }

  void set_opener_dom_ui_type(DOMUITypeID opener_dom_ui_type) {
    opener_dom_ui_type_ = opener_dom_ui_type;
  }

  // We want to time how long it takes to create a new tab page.  This method
  // gets called as parts of the new tab page have loaded.
  void LogNewTabTime(const std::string& event_name);

  // Set the time when we started to create the new tab page.  This time is
  // from before we created this TabContents.
  void set_new_tab_start_time(const base::TimeTicks& time) {
    new_tab_start_time_ = time;
  }

  // Notification that tab closing has started.  This can be called multiple
  // times, subsequent calls are ignored.
  void OnCloseStarted();

  LanguageState& language_state() {
    return language_state_;
  }

  // Returns true if underlying TabContentsView should accept drag-n-drop.
  bool ShouldAcceptDragAndDrop() const;

  // Indicates if this tab was explicitly closed by the user (control-w, close
  // tab menu item...). This is false for actions that indirectly close the tab,
  // such as closing the window.  The setter is maintained by TabStripModel, and
  // the getter only useful from within TAB_CLOSED notification
  void set_closed_by_user_gesture(bool value) {
    closed_by_user_gesture_ = value;
  }
  bool closed_by_user_gesture() const { return closed_by_user_gesture_; }

  // Overridden from JavaScriptAppModalDialogDelegate:
  virtual void OnMessageBoxClosed(IPC::Message* reply_msg,
                                  bool success,
                                  const std::wstring& prompt);
  virtual void SetSuppressMessageBoxes(bool suppress_message_boxes);
  virtual gfx::NativeWindow GetMessageBoxRootWindow();
  virtual TabContents* AsTabContents();
  virtual ExtensionHost* AsExtensionHost();

  // The BookmarkDragDelegate is used to forward bookmark drag and drop events
  // to extensions.
  virtual RenderViewHostDelegate::BookmarkDrag* GetBookmarkDragDelegate();

  // It is up to callers to call SetBookmarkDragDelegate(NULL) when
  // |bookmark_drag| is deleted since this class does not take ownership of
  // |bookmark_drag|.
  virtual void SetBookmarkDragDelegate(
      RenderViewHostDelegate::BookmarkDrag* bookmark_drag);

  // The TabSpecificContentSettings object is used to query the blocked content
  // state by various UI elements.
  TabSpecificContentSettings* GetTabSpecificContentSettings() const;

  // Updates history with the specified navigation. This is called by
  // OnMsgNavigate to update history state.
  void UpdateHistoryForNavigation(
      scoped_refptr<history::HistoryAddPageArgs> add_page_args);

  // Sends the page title to the history service. This is called when we receive
  // the page title and we know we want to update history.
  void UpdateHistoryPageTitle(const NavigationEntry& entry);

  // Gets the zoom level for this tab.
  double GetZoomLevel() const;

  // Gets the zoom percent for this tab.
  int GetZoomPercent(bool* enable_increment, bool* enable_decrement);

  // Shows a fade effect over this tab contents. Repeated calls will be ignored
  // until the fade is canceled. If |animate| is true the fade should animate.
  void FadeForInstant(bool animate);

  // Immediately removes the fade.
  void CancelInstantFade();

  // Opens view-source tab for this contents.
  void ViewSource();

  // Gets the minimum/maximum zoom percent.
  int minimum_zoom_percent() const { return minimum_zoom_percent_; }
  int maximum_zoom_percent() const { return maximum_zoom_percent_; }

  int content_restrictions() const { return content_restrictions_; }

  AutocompleteHistoryManager* autocomplete_history_manager() {
    return autocomplete_history_manager_.get();
  }
  AutoFillManager* autofill_manager() { return autofill_manager_.get(); }

 protected:
  // from RenderViewHostDelegate.
  virtual bool OnMessageReceived(const IPC::Message& message);

 private:
  friend class NavigationController;
  // Used to access the child_windows_ (ConstrainedWindowList) for testing
  // automation purposes.
  friend class TestingAutomationProvider;

  FRIEND_TEST_ALL_PREFIXES(TabContentsTest, NoJSMessageOnInterstitials);
  FRIEND_TEST_ALL_PREFIXES(TabContentsTest, UpdateTitle);
  FRIEND_TEST_ALL_PREFIXES(TabContentsTest, CrossSiteCantPreemptAfterUnload);
  FRIEND_TEST_ALL_PREFIXES(FormStructureBrowserTest, HTMLFiles);
  FRIEND_TEST_ALL_PREFIXES(NavigationControllerTest, HistoryNavigate);
  FRIEND_TEST_ALL_PREFIXES(RenderViewHostManagerTest, PageDoesBackAndReload);

  // Temporary until the view/contents separation is complete.
  friend class TabContentsView;
#if defined(OS_WIN)
  friend class TabContentsViewWin;
#elif defined(OS_MACOSX)
  friend class TabContentsViewMac;
#elif defined(TOOLKIT_USES_GTK)
  friend class TabContentsViewGtk;
#endif

  // So InterstitialPage can access SetIsLoading.
  friend class InterstitialPage;

  // TODO(brettw) TestTabContents shouldn't exist!
  friend class TestTabContents;

  // Used to access the CreateHistoryAddPageArgs member function.
  friend class ExternalTabContainer;

  // Used to access RVH Delegates.
  friend class PrerenderManager;

  // Message handlers.
  void OnDidStartProvisionalLoadForFrame(int64 frame_id,
                                         bool main_frame,
                                         const GURL& url);
  void OnDidRedirectProvisionalLoad(int32 page_id,
                                    const GURL& source_url,
                                    const GURL& target_url);
  void OnDidFailProvisionalLoadWithError(int64 frame_id,
                                         bool main_frame,
                                         int error_code,
                                         const GURL& url,
                                         bool showing_repost_interstitial);
  void OnDidLoadResourceFromMemoryCache(const GURL& url,
                                        const std::string& frame_origin,
                                        const std::string& main_frame_origin,
                                        const std::string& security_info);
  void OnDidDisplayInsecureContent();
  void OnDidRunInsecureContent(const std::string& security_origin,
                               const GURL& target_url);
  void OnDocumentLoadedInFrame(int64 frame_id);
  void OnDidFinishLoad(int64 frame_id);
  void OnUpdateContentRestrictions(int restrictions);
  void OnPDFHasUnsupportedFeature();

  void OnFindReply(int request_id,
                   int number_of_matches,
                   const gfx::Rect& selection_rect,
                   int active_match_ordinal,
                   bool final_update);
  void OnGoToEntryAtOffset(int offset);
  void OnMissingPluginStatus(int status);
  void OnCrashedPlugin(const FilePath& plugin_path);
  void OnDidGetApplicationInfo(int32 page_id, const WebApplicationInfo& info);
  void OnInstallApplication(const WebApplicationInfo& info);
  void OnBlockedOutdatedPlugin(const string16& name, const GURL& update_url);
  void OnPageContents(const GURL& url,
                      int32 page_id,
                      const string16& contents,
                      const std::string& language,
                      bool page_translatable);
  void OnPageTranslated(int32 page_id,
                        const std::string& original_lang,
                        const std::string& translated_lang,
                        TranslateErrors::Type error_type);
  void OnSetSuggestions(int32 page_id,
                        const std::vector<std::string>& suggestions);
  void OnInstantSupportDetermined(int32 page_id, bool result);
  void OnRunFileChooser(const ViewHostMsg_RunFileChooser_Params& params);

  // Changes the IsLoading state and notifies delegate as needed
  // |details| is used to provide details on the load that just finished
  // (but can be null if not applicable). Can be overridden.
  void SetIsLoading(bool is_loading,
                    LoadNotificationDetails* details);

  // Adds the incoming |new_contents| to the |blocked_contents_| container.
  void AddPopup(TabContents* new_contents,
                const gfx::Rect& initial_pos);

  // Called by derived classes to indicate that we're no longer waiting for a
  // response. This won't actually update the throbber, but it will get picked
  // up at the next animation step if the throbber is going.
  void SetNotWaitingForResponse() { waiting_for_response_ = false; }

  ConstrainedWindowList child_windows_;

  // Expires InfoBars that need to be expired, according to the state carried
  // in |details|, in response to a new NavigationEntry being committed (the
  // user navigated to another page).
  void ExpireInfoBars(
      const NavigationController::LoadCommittedDetails& details);

  // Returns the DOMUI for the current state of the tab. This will either be
  // the pending DOMUI, the committed DOMUI, or NULL.
  DOMUI* GetDOMUIForCurrentState();

  // Navigation helpers --------------------------------------------------------
  //
  // These functions are helpers for Navigate() and DidNavigate().

  // Handles post-navigation tasks in DidNavigate AFTER the entry has been
  // committed to the navigation controller. Note that the navigation entry is
  // not provided since it may be invalid/changed after being committed. The
  // current navigation entry is in the NavigationController at this point.
  void DidNavigateMainFramePostCommit(
      const NavigationController::LoadCommittedDetails& details,
      const ViewHostMsg_FrameNavigate_Params& params);
  void DidNavigateAnyFramePostCommit(
      RenderViewHost* render_view_host,
      const NavigationController::LoadCommittedDetails& details,
      const ViewHostMsg_FrameNavigate_Params& params);

  // Closes all constrained windows.
  void CloseConstrainedWindows();

  // Updates the starred state from the bookmark bar model. If the state has
  // changed, the delegate is notified.
  void UpdateStarredStateForCurrentURL();

  // Send the alternate error page URL to the renderer. This method is virtual
  // so special html pages can override this (e.g., the new tab page).
  virtual void UpdateAlternateErrorPageURL();

  // Send webkit specific settings to the renderer.
  void UpdateWebPreferences();

  // Instruct the renderer to update the zoom level.
  void UpdateZoomLevel();

  // If our controller was restored and the page id is > than the site
  // instance's page id, the site instances page id is updated as well as the
  // renderers max page id.
  void UpdateMaxPageIDIfNecessary(SiteInstance* site_instance,
                                  RenderViewHost* rvh);

  // Returns the history::HistoryAddPageArgs to use for adding a page to
  // history.
  scoped_refptr<history::HistoryAddPageArgs> CreateHistoryAddPageArgs(
      const GURL& virtual_url,
      const NavigationController::LoadCommittedDetails& details,
      const ViewHostMsg_FrameNavigate_Params& params);

  // Saves the given title to the navigation entry and does associated work. It
  // will update history and the view for the new title, and also synthesize
  // titles for file URLs that have none (so we require that the URL of the
  // entry already be set).
  //
  // This is used as the backend for state updates, which include a new title,
  // or the dedicated set title message. It returns true if the new title is
  // different and was therefore updated.
  bool UpdateTitleForEntry(NavigationEntry* entry, const std::wstring& title);

  // Causes the TabContents to navigate in the right renderer to |entry|, which
  // must be already part of the entries in the navigation controller.
  // This does not change the NavigationController state.
  bool NavigateToEntry(const NavigationEntry& entry,
                       NavigationController::ReloadType reload_type);

  // Misc non-view stuff -------------------------------------------------------

  // Helper functions for sending notifications.
  void NotifySwapped();
  void NotifyConnected();
  void NotifyDisconnected();

  // If params has a searchable form, this tries to create a new keyword.
  void GenerateKeywordIfNecessary(
      const ViewHostMsg_FrameNavigate_Params& params);

  // TabSpecificContentSettings::Delegate implementation.
  virtual void OnContentSettingsAccessed(bool content_was_blocked);

  // RenderViewHostDelegate ----------------------------------------------------

  // RenderViewHostDelegate implementation.
  virtual RenderViewHostDelegate::View* GetViewDelegate();
  virtual RenderViewHostDelegate::RendererManagement*
      GetRendererManagementDelegate();
  virtual RenderViewHostDelegate::ContentSettings* GetContentSettingsDelegate();
  virtual RenderViewHostDelegate::SSL* GetSSLDelegate();
  virtual AutomationResourceRoutingDelegate*
      GetAutomationResourceRoutingDelegate();
  virtual TabContents* GetAsTabContents();
  virtual ViewType::Type GetRenderViewType() const;
  virtual int GetBrowserWindowID() const;
  virtual void RenderViewCreated(RenderViewHost* render_view_host);
  virtual void RenderViewReady(RenderViewHost* render_view_host);
  virtual void RenderViewGone(RenderViewHost* render_view_host,
                              base::TerminationStatus status,
                              int error_code);
  virtual void RenderViewDeleted(RenderViewHost* render_view_host);
  virtual void DidNavigate(RenderViewHost* render_view_host,
                           const ViewHostMsg_FrameNavigate_Params& params);
  virtual void UpdateState(RenderViewHost* render_view_host,
                           int32 page_id,
                           const std::string& state);
  virtual void UpdateTitle(RenderViewHost* render_view_host,
                           int32 page_id,
                           const std::wstring& title);
  virtual void UpdateEncoding(RenderViewHost* render_view_host,
                              const std::string& encoding);
  virtual void UpdateTargetURL(int32 page_id, const GURL& url);
  virtual void UpdateThumbnail(const GURL& url,
                               const SkBitmap& bitmap,
                               const ThumbnailScore& score);
  virtual void UpdateInspectorSetting(const std::string& key,
                                      const std::string& value);
  virtual void ClearInspectorSettings();
  virtual void Close(RenderViewHost* render_view_host);
  virtual void RequestMove(const gfx::Rect& new_bounds);
  virtual void DidStartLoading();
  virtual void DidStopLoading();
  virtual void DidChangeLoadProgress(double progress);
  virtual void DocumentOnLoadCompletedInMainFrame(
      RenderViewHost* render_view_host,
      int32 page_id);
  virtual void RequestOpenURL(const GURL& url, const GURL& referrer,
                              WindowOpenDisposition disposition);
  virtual void DomOperationResponse(const std::string& json_string,
                                    int automation_id);
  virtual void ProcessDOMUIMessage(const ViewHostMsg_DomMessage_Params& params);
  virtual void ProcessExternalHostMessage(const std::string& message,
                                          const std::string& origin,
                                          const std::string& target);
  virtual void RunJavaScriptMessage(const std::wstring& message,
                                    const std::wstring& default_prompt,
                                    const GURL& frame_url,
                                    const int flags,
                                    IPC::Message* reply_msg,
                                    bool* did_suppress_message);
  virtual void RunBeforeUnloadConfirm(const std::wstring& message,
                                      IPC::Message* reply_msg);
  virtual void ShowModalHTMLDialog(const GURL& url, int width, int height,
                                   const std::string& json_arguments,
                                   IPC::Message* reply_msg);
  virtual void PageHasOSDD(RenderViewHost* render_view_host,
                           int32 page_id,
                           const GURL& url,
                           const ViewHostMsg_PageHasOSDD_Type& provider_type);
  virtual GURL GetAlternateErrorPageURL() const;
  virtual RendererPreferences GetRendererPrefs(Profile* profile) const;
  virtual WebPreferences GetWebkitPrefs();
  virtual void OnUserGesture();
  virtual void OnIgnoredUIEvent();
  virtual void OnJSOutOfMemory();
  virtual void OnCrossSiteResponse(int new_render_process_host_id,
                                   int new_request_id);
  virtual void RendererUnresponsive(RenderViewHost* render_view_host,
                                    bool is_during_unload);
  virtual void RendererResponsive(RenderViewHost* render_view_host);
  virtual void LoadStateChanged(const GURL& url, net::LoadState load_state,
                                uint64 upload_position, uint64 upload_size);
  virtual bool IsExternalTabContainer() const;
  virtual void DidInsertCSS();
  virtual void FocusedNodeChanged(bool is_editable_node);
  virtual void UpdateZoomLimits(int minimum_percent,
                                int maximum_percent,
                                bool remember);
  virtual void WorkerCrashed();

  // RenderViewHostManager::Delegate -------------------------------------------

  // Blocks/unblocks interaction with renderer process.
  void BlockTabContent(bool blocked);

  virtual void BeforeUnloadFiredFromRenderManager(
      bool proceed,
      bool* proceed_to_fire_unload);
  virtual void DidStartLoadingFromRenderManager(
      RenderViewHost* render_view_host);
  virtual void RenderViewGoneFromRenderManager(
      RenderViewHost* render_view_host);
  virtual void UpdateRenderViewSizeForRenderManager();
  virtual void NotifySwappedFromRenderManager();
  virtual NavigationController& GetControllerForRenderManager();
  virtual DOMUI* CreateDOMUIForRenderManager(const GURL& url);
  virtual NavigationEntry* GetLastCommittedNavigationEntryForRenderManager();

  // Initializes the given renderer if necessary and creates the view ID
  // corresponding to this view host. If this method is not called and the
  // process is not shared, then the TabContents will act as though the renderer
  // is not running (i.e., it will render "sad tab"). This method is
  // automatically called from LoadURL.
  //
  // If you are attaching to an already-existing RenderView, you should call
  // InitWithExistingID.
  virtual bool CreateRenderViewForRenderManager(
      RenderViewHost* render_view_host);

  // NotificationObserver ------------------------------------------------------

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // App extensions related methods:

  // Returns the first extension whose extent contains |url|.
  const Extension* GetExtensionContaining(const GURL& url);

  // Resets app_icon_ and if |extension| is non-null creates a new
  // ImageLoadingTracker to load the extension's image.
  void UpdateExtensionAppIcon(const Extension* extension);

  // ImageLoadingTracker::Observer.
  virtual void OnImageLoaded(SkBitmap* image, ExtensionResource resource,
                             int index);

  // Checks with the PrerenderManager if the specified URL has been preloaded,
  // and if so, swap the RenderViewHost with the preload into this TabContents
  // object.
  bool MaybeUsePreloadedPage(const GURL& url);

  // Data for core operation ---------------------------------------------------

  // Delegate for notifying our owner about stuff. Not owned by us.
  TabContentsDelegate* delegate_;

  // Handles the back/forward list and loading.
  NavigationController controller_;

  // The corresponding view.
  scoped_ptr<TabContentsView> view_;

  // Helper classes ------------------------------------------------------------

  // Manages creation and swapping of render views.
  RenderViewHostManager render_manager_;

  // Stores random bits of data for others to associate with this object.
  PropertyBag property_bag_;

  // Registers and unregisters us for notifications.
  NotificationRegistrar registrar_;

  // Registers and unregisters for pref notifications.
  PrefChangeRegistrar pref_change_registrar_;

  // Handles print preview and print job for this contents.
  scoped_ptr<printing::PrintViewManager> printing_;

  // SavePackage, lazily created.
  scoped_refptr<SavePackage> save_package_;

  // AutocompleteHistoryManager.
  scoped_ptr<AutocompleteHistoryManager> autocomplete_history_manager_;

  // AutoFillManager.
  scoped_ptr<AutoFillManager> autofill_manager_;

  // PluginInstallerInfoBarDelegate, lazily created.
  scoped_ptr<PluginInstallerInfoBarDelegate> plugin_installer_;

  // Prerender PageLoadTime Recorder.
  scoped_ptr<PrerenderPLTRecorder> prerender_plt_recorder_;

  // TabContentsSSLHelper, lazily created.
  scoped_ptr<TabContentsSSLHelper> ssl_helper_;

  // FileSelectHelper, lazily created.
  scoped_ptr<FileSelectHelper> file_select_helper_;

  // Handles drag and drop event forwarding to extensions.
  BookmarkDrag* bookmark_drag_;

  // Handles downloading favicons.
  FavIconHelper fav_icon_helper_;

  // Cached web app info data.
  WebApplicationInfo web_app_info_;

  // Cached web app icon.
  SkBitmap app_icon_;

  // RenderViewHost::ContentSettingsDelegate.
  scoped_ptr<TabSpecificContentSettings> content_settings_delegate_;

  // Handles desktop notification IPCs.
  scoped_ptr<DesktopNotificationHandler> desktop_notification_handler_;


  // Data for loading state ----------------------------------------------------

  // Indicates whether we're currently loading a resource.
  bool is_loading_;

  // Indicates if the tab is considered crashed.
  base::TerminationStatus crashed_status_;
  int crashed_error_code_;

  // See waiting_for_response() above.
  bool waiting_for_response_;

  // Indicates the largest PageID we've seen.  This field is ignored if we are
  // a TabContents, in which case the max page ID is stored separately with
  // each SiteInstance.
  // TODO(brettw) this seems like it can be removed according to the comment.
  int32 max_page_id_;

  // System time at which the current load was started.
  base::TimeTicks current_load_start_;

  // The current load state and the URL associated with it.
  net::LoadState load_state_;
  string16 load_state_host_;
  // Upload progress, for displaying in the status bar.
  // Set to zero when there is no significant upload happening.
  uint64 upload_size_;
  uint64 upload_position_;

  // Data for current page -----------------------------------------------------

  // Whether we have a (non-empty) title for the current page.
  // Used to prevent subsequent title updates from affecting history. This
  // prevents some weirdness because some AJAXy apps use titles for status
  // messages.
  bool received_page_title_;

  // Whether the current URL is starred
  bool is_starred_;

  // When a navigation occurs, we record its contents MIME type. It can be
  // used to check whether we can do something for some special contents.
  std::string contents_mime_type_;

  // Character encoding.
  std::string encoding_;

  // Object that holds any blocked TabContents spawned from this TabContents.
  BlockedContentContainer* blocked_contents_;

  // Should we block all child TabContents this attempts to spawn.
  bool all_contents_blocked_;

  // TODO(pkasting): Hack to try and fix Linux browser tests.
  bool dont_notify_render_view_;

  // True if this is a secure page which displayed insecure content.
  bool displayed_insecure_content_;

  // Data for shelves and stuff ------------------------------------------------

  // Delegates for InfoBars associated with this TabContents.
  std::vector<InfoBarDelegate*> infobar_delegates_;

  // Data for find in page -----------------------------------------------------

  // TODO(brettw) this should be separated into a helper class.

  // Each time a search request comes in we assign it an id before passing it
  // over the IPC so that when the results come in we can evaluate whether we
  // still care about the results of the search (in some cases we don't because
  // the user has issued a new search).
  static int find_request_id_counter_;

  // True if the Find UI is active for this Tab.
  bool find_ui_active_;

  // True if a Find operation was aborted. This can happen if the Find box is
  // closed or if the search term inside the Find box is erased while a search
  // is in progress. This can also be set if a page has been reloaded, and will
  // on FindNext result in a full Find operation so that the highlighting for
  // inactive matches can be repainted.
  bool find_op_aborted_;

  // This variable keeps track of what the most recent request id is.
  int current_find_request_id_;

  // The current string we are/just finished searching for. This is used to
  // figure out if this is a Find or a FindNext operation (FindNext should not
  // increase the request id).
  string16 find_text_;

  // The string we searched for before |find_text_|.
  string16 previous_find_text_;

  // Whether the last search was case sensitive or not.
  bool last_search_case_sensitive_;

  // The last find result. This object contains details about the number of
  // matches, the find selection rectangle, etc. The UI can access this
  // information to build its presentation.
  FindNotificationDetails last_search_result_;

  // Data for app extensions ---------------------------------------------------

  // If non-null this tab is an app tab and this is the extension the tab was
  // created for.
  const Extension* extension_app_;

  // Icon for extension_app_ (if non-null) or extension_for_current_page_.
  SkBitmap extension_app_icon_;

  // Used for loading extension_app_icon_.
  scoped_ptr<ImageLoadingTracker> extension_app_image_loader_;

  // Data for misc internal state ----------------------------------------------

  // See capturing_contents() above.
  bool capturing_contents_;

  // See getter above.
  bool is_being_destroyed_;

  // Indicates whether we should notify about disconnection of this
  // TabContents. This is used to ensure disconnection notifications only
  // happen if a connection notification has happened and that they happen only
  // once.
  bool notify_disconnection_;

  // Maps from handle to page_id.
  typedef std::map<FaviconService::Handle, int32> HistoryRequestMap;
  HistoryRequestMap history_requests_;

#if defined(OS_WIN)
  // Handle to an event that's set when the page is showing a message box (or
  // equivalent constrained window).  Plugin processes check this to know if
  // they should pump messages then.
  base::win::ScopedHandle message_box_active_;
#endif

  // The time that the last javascript message was dismissed.
  base::TimeTicks last_javascript_message_dismissal_;

  // True if the user has decided to block future javascript messages. This is
  // reset on navigations to false on navigations.
  bool suppress_javascript_messages_;

  // Set to true when there is an active "before unload" dialog.  When true,
  // we've forced the throbber to start in Navigate, and we need to remember to
  // turn it off in OnJavaScriptMessageBoxClosed if the navigation is canceled.
  bool is_showing_before_unload_dialog_;

  // Shows an info-bar to users when they search from a known search engine and
  // have never used the monibox for search before.
  scoped_ptr<OmniboxSearchHint> omnibox_search_hint_;

  // Settings that get passed to the renderer process.
  RendererPreferences renderer_preferences_;

  // If this tab was created from a renderer using window.open, this will be
  // non-NULL and represent the DOMUI of the opening renderer.
  DOMUITypeID opener_dom_ui_type_;

  // The time that we started to create the new tab page.
  base::TimeTicks new_tab_start_time_;

  // The time that we started to close the tab.
  base::TimeTicks tab_close_start_time_;

  // The time that this tab was last selected.
  base::TimeTicks last_selected_time_;

  // Information about the language the page is in and has been translated to.
  LanguageState language_state_;

  // See description above setter.
  bool closed_by_user_gesture_;

  // Minimum/maximum zoom percent.
  int minimum_zoom_percent_;
  int maximum_zoom_percent_;
  // If true, the default zoom limits have been overriden for this tab, in which
  // case we don't want saved settings to apply to it and we don't want to
  // remember it.
  bool temporary_zoom_settings_;

  // A list of observers notified when page state changes. Weak references.
  ObserverList<TabContentsObserver> observers_;

  // Content restrictions, used to disable print/copy etc based on content's
  // (full-page plugins for now only) permissions.
  int content_restrictions_;

  DISALLOW_COPY_AND_ASSIGN(TabContents);
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_TAB_CONTENTS_H_
