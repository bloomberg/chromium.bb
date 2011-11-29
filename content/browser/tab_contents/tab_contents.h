// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TAB_CONTENTS_TAB_CONTENTS_H_
#define CONTENT_BROWSER_TAB_CONTENTS_TAB_CONTENTS_H_
#pragma once

#include <deque>
#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/property_bag.h"
#include "base/string16.h"
#include "content/browser/download/save_package.h"
#include "content/browser/javascript_dialogs.h"
#include "content/browser/renderer_host/java/java_bridge_dispatcher_host_manager.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/tab_contents/navigation_controller.h"
#include "content/browser/tab_contents/navigation_entry.h"
#include "content/browser/tab_contents/page_navigator.h"
#include "content/browser/tab_contents/render_view_host_manager.h"
#include "content/browser/tab_contents/tab_contents_observer.h"
#include "content/browser/webui/web_ui.h"
#include "content/common/content_export.h"
#include "content/public/common/renderer_preferences.h"
#include "net/base/load_states.h"
#include "ui/gfx/native_widget_types.h"
#include "webkit/glue/resource_type.h"

#if defined(OS_WIN)
#include "base/win/scoped_handle.h"
#endif

class DownloadItem;
class LoadNotificationDetails;
class RenderViewHost;
class SessionStorageNamespace;
class SiteInstance;
class TabContentsDelegate;
class TabContentsObserver;
class TabContentsView;
struct ViewHostMsg_DidFailProvisionalLoadWithError_Params;

namespace webkit_glue {
struct WebIntentData;
}

// Describes what goes in the main content area of a tab. TabContents is
// the only type of TabContents, and these should be merged together.
class CONTENT_EXPORT TabContents : public PageNavigator,
                                   public RenderViewHostDelegate,
                                   public RenderViewHostManager::Delegate,
                                   public content::JavaScriptDialogDelegate {
 public:
  // Flags passed to the TabContentsDelegate.NavigationStateChanged to tell it
  // what has changed. Combine them to update more than one thing.
  enum InvalidateTypes {
    INVALIDATE_URL             = 1 << 0,  // The URL has changed.
    INVALIDATE_TAB             = 1 << 1,  // The favicon, app icon, or crashed
                                          // state changed.
    INVALIDATE_LOAD            = 1 << 2,  // The loading state has changed.
    INVALIDATE_PAGE_ACTIONS    = 1 << 3,  // Page action icons have changed.
    INVALIDATE_TITLE           = 1 << 4,  // The title changed.
  };

  // |base_tab_contents| is used if we want to size the new tab contents view
  // based on an existing tab contents view.  This can be NULL if not needed.
  //
  // The session storage namespace parameter allows multiple render views and
  // tab contentses to share the same session storage (part of the WebStorage
  // spec) space. This is useful when restoring tabs, but most callers should
  // pass in NULL which will cause a new SessionStorageNamespace to be created.
  TabContents(content::BrowserContext* browser_context,
              SiteInstance* site_instance,
              int routing_id,
              const TabContents* base_tab_contents,
              SessionStorageNamespace* session_storage_namespace);
  virtual ~TabContents();

  // Intrinsic tab state -------------------------------------------------------

  // Returns the property bag for this tab contents, where callers can add
  // extra data they may wish to associate with the tab. Returns a pointer
  // rather than a reference since the PropertyAccessors expect this.
  const base::PropertyBag* property_bag() const { return &property_bag_; }
  base::PropertyBag* property_bag() { return &property_bag_; }

  TabContentsDelegate* delegate() const { return delegate_; }
  void set_delegate(TabContentsDelegate* delegate);

  // Gets the controller for this tab contents.
  NavigationController& controller() { return controller_; }
  const NavigationController& controller() const { return controller_; }

  // Returns the user browser context associated with this TabContents (via the
  // NavigationController).
  content::BrowserContext* browser_context() const {
    return controller_.browser_context();
  }

  // Allows overriding the type of this tab.
  void set_view_type(content::ViewType type) { view_type_ = type; }

  // Returns the SavePackage which manages the page saving job. May be NULL.
  SavePackage* save_package() const { return save_package_.get(); }

  // Return the currently active RenderProcessHost and RenderViewHost. Each of
  // these may change over time.
  content::RenderProcessHost* GetRenderProcessHost() const;
  RenderViewHost* render_view_host() const {
    return render_manager_.current_host();
  }

  WebUI* committed_web_ui() const {
    return render_manager_.web_ui();
  }

  // Returns the committed WebUI if one exists, otherwise the pending one.
  // Callers who want to use the pending WebUI for the pending navigation entry
  // should use GetWebUIForCurrentState instead.
  WebUI* web_ui() const {
    return render_manager_.web_ui() ? render_manager_.web_ui()
        : render_manager_.pending_web_ui();
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

  // Tab navigation state ------------------------------------------------------

  // Returns the current navigation properties, which if a navigation is
  // pending may be provisional (e.g., the navigation could result in a
  // download, in which case the URL would revert to what it was previously).
  virtual const GURL& GetURL() const OVERRIDE;
  virtual const string16& GetTitle() const;

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

  // Returns the SiteInstance for the pending navigation, if any.  Otherwise
  // returns the current SiteInstance.
  SiteInstance* GetPendingSiteInstance() const;

  // Return whether this tab contents is loading a resource.
  bool IsLoading() const { return is_loading_; }

  // Returns whether this tab contents is waiting for a first-response for the
  // main resource of the page. This controls whether the throbber state is
  // "waiting" or "loading."
  bool waiting_for_response() const { return waiting_for_response_; }

  const net::LoadStateWithParam& load_state() const { return load_state_; }
  const string16& load_state_host() const { return load_state_host_; }
  uint64 upload_size() const { return upload_size_; }
  uint64 upload_position() const { return upload_position_; }

  const std::string& encoding() const { return encoding_; }
  void set_encoding(const std::string& encoding);
  void reset_encoding() {
    encoding_.clear();
  }

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

  // TODO(brettw) document these.
  virtual void ShowContents();
  virtual void HideContents();

  // Returns true if the before unload and unload listeners need to be
  // fired. The value of this changes over time. For example, if true and the
  // before unload listener is executed and allows the user to exit, then this
  // returns false.
  bool NeedToFireBeforeUnload();

  // Expose the render manager for testing.
  RenderViewHostManager* render_manager_for_testing() {
    return &render_manager_;
  }

  // Commands ------------------------------------------------------------------

  // Implementation of PageNavigator.

  // Deprecated. Please use the one-argument variant instead.
  // TODO(adriansc): Remove this method once refactoring changed all call sites.
  virtual TabContents* OpenURL(const GURL& url,
                               const GURL& referrer,
                               WindowOpenDisposition disposition,
                               content::PageTransition transition) OVERRIDE;

  virtual TabContents* OpenURL(const OpenURLParams& params) OVERRIDE;

  // Called by the NavigationController to cause the TabContents to navigate to
  // the current pending entry. The NavigationController should be called back
  // with RendererDidNavigate on success or DiscardPendingEntry on failure.
  // The callbacks can be inside of this function, or at some future time.
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

  // Creates a new TabContents with the same state as this one. The returned
  // heap-allocated pointer is owned by the caller.
  virtual TabContents* Clone();

  // Shows the page info.
  void ShowPageInfo(const GURL& url,
                    const NavigationEntry::SSLStatus& ssl,
                    bool show_history);

  // Window management ---------------------------------------------------------

  // Adds a new tab or window with the given already-created contents.
  void AddNewContents(TabContents* new_contents,
                      WindowOpenDisposition disposition,
                      const gfx::Rect& initial_pos,
                      bool user_gesture);

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
  virtual bool FocusLocationBarByDefault() OVERRIDE;

  // Focuses the location bar.
  virtual void SetFocusToLocationBar(bool select_all) OVERRIDE;

  // Creates a view and sets the size for the specified RVH.
  virtual void CreateViewAndSetSizeForRVH(RenderViewHost* rvh) OVERRIDE;

  // Toolbars and such ---------------------------------------------------------

  // Notifies the delegate that a download is about to be started.
  // This notification is fired before a local temporary file has been created.
  bool CanDownload(int request_id);

  // Notifies the delegate that a download started.
  void OnStartDownload(DownloadItem* download);

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

  // Misc state & callbacks ----------------------------------------------------

  // Prepare for saving the current web page to disk.
  void OnSavePage();

  // Save page with the main HTML file path, the directory for saving resources,
  // and the save type: HTML only or complete web page. Returns true if the
  // saving process has been initiated successfully.
  bool SavePage(const FilePath& main_file, const FilePath& dir_path,
                SavePackage::SavePackageType save_type);

  // Prepare for saving the URL to disk.
  // URL may refer to the iframe on the page.
  void OnSaveURL(const GURL& url);

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

  content::RendererPreferences* GetMutableRendererPrefs() {
    return &renderer_preferences_;
  }

  void set_opener_web_ui_type(WebUI::TypeID opener_web_ui_type) {
    opener_web_ui_type_ = opener_web_ui_type;
  }

  // Set the time when we started to create the new tab page.  This time is
  // from before we created this TabContents.
  void set_new_tab_start_time(const base::TimeTicks& time) {
    new_tab_start_time_ = time;
  }
  base::TimeTicks new_tab_start_time() const { return new_tab_start_time_; }

  // Notification that tab closing has started.  This can be called multiple
  // times, subsequent calls are ignored.
  void OnCloseStarted();

  // Returns true if underlying TabContentsView should accept drag-n-drop.
  bool ShouldAcceptDragAndDrop() const;

  // A render view-originated drag has ended. Informs the render view host and
  // tab contents delegate.
  void SystemDragEnded();

  // Indicates if this tab was explicitly closed by the user (control-w, close
  // tab menu item...). This is false for actions that indirectly close the tab,
  // such as closing the window.  The setter is maintained by TabStripModel, and
  // the getter only useful from within TAB_CLOSED notification
  void set_closed_by_user_gesture(bool value) {
    closed_by_user_gesture_ = value;
  }
  bool closed_by_user_gesture() const { return closed_by_user_gesture_; }

  // Overridden from JavaScriptDialogDelegate:
  virtual void OnDialogClosed(IPC::Message* reply_msg,
                              bool success,
                              const string16& user_input) OVERRIDE;
  virtual gfx::NativeWindow GetDialogRootWindow() const OVERRIDE;
  virtual void OnDialogShown() OVERRIDE;

  // Gets the zoom level for this tab.
  double GetZoomLevel() const;

  // Gets the zoom percent for this tab.
  int GetZoomPercent(bool* enable_increment, bool* enable_decrement);

  // Opens view-source tab for this contents.
  void ViewSource();

  void ViewFrameSource(const GURL& url,
                       const std::string& content_state);

  // Gets the minimum/maximum zoom percent.
  int minimum_zoom_percent() const { return minimum_zoom_percent_; }
  int maximum_zoom_percent() const { return maximum_zoom_percent_; }

  int content_restrictions() const { return content_restrictions_; }
  void SetContentRestrictions(int restrictions);

  // Query the WebUIFactory for the TypeID for the current URL.
  WebUI::TypeID GetWebUITypeForCurrentState();

  // Returns the WebUI for the current state of the tab. This will either be
  // the pending WebUI, the committed WebUI, or NULL.
  WebUI* GetWebUIForCurrentState();

  // Called when the reponse to a pending mouse lock request has arrived.
  // Returns true if |allowed| is true and the mouse has been successfully
  // locked.
  bool GotResponseToLockMouseRequest(bool allowed);

  JavaBridgeDispatcherHostManager* java_bridge_dispatcher_host_manager() const {
    return java_bridge_dispatcher_host_manager_.get();
  }

  // RenderViewHostDelegate ----------------------------------------------------

  virtual RenderViewHostDelegate::View* GetViewDelegate() OVERRIDE;
  virtual RenderViewHostDelegate::RendererManagement*
      GetRendererManagementDelegate() OVERRIDE;
  virtual TabContents* GetAsTabContents() OVERRIDE;
  virtual content::ViewType GetRenderViewType() const OVERRIDE;
  virtual void RenderViewCreated(RenderViewHost* render_view_host) OVERRIDE;
  virtual void RenderViewReady(RenderViewHost* render_view_host) OVERRIDE;
  virtual void RenderViewGone(RenderViewHost* render_view_host,
                              base::TerminationStatus status,
                              int error_code) OVERRIDE;
  virtual void RenderViewDeleted(RenderViewHost* render_view_host) OVERRIDE;
  virtual void DidNavigate(
      RenderViewHost* render_view_host,
      const ViewHostMsg_FrameNavigate_Params& params) OVERRIDE;
  virtual void UpdateState(RenderViewHost* render_view_host,
                           int32 page_id,
                           const std::string& state) OVERRIDE;
  virtual void UpdateTitle(RenderViewHost* render_view_host,
                           int32 page_id,
                           const string16& title,
                           base::i18n::TextDirection title_direction) OVERRIDE;
  virtual void UpdateEncoding(RenderViewHost* render_view_host,
                              const std::string& encoding) OVERRIDE;
  virtual void UpdateTargetURL(int32 page_id, const GURL& url) OVERRIDE;
  virtual void Close(RenderViewHost* render_view_host) OVERRIDE;
  virtual void RequestMove(const gfx::Rect& new_bounds) OVERRIDE;
  virtual void SwappedOut(RenderViewHost* render_view_host) OVERRIDE;
  virtual void DidStartLoading() OVERRIDE;
  virtual void DidStopLoading() OVERRIDE;
  virtual void DidCancelLoading() OVERRIDE;
  virtual void DidChangeLoadProgress(double progress) OVERRIDE;
  virtual void DocumentAvailableInMainFrame(
      RenderViewHost* render_view_host) OVERRIDE;
  virtual void DocumentOnLoadCompletedInMainFrame(
      RenderViewHost* render_view_host,
      int32 page_id) OVERRIDE;
  virtual void RequestOpenURL(const GURL& url,
                              const GURL& referrer,
                              WindowOpenDisposition disposition,
                              int64 source_frame_id) OVERRIDE;
  virtual void RunJavaScriptMessage(const RenderViewHost* rvh,
                                    const string16& message,
                                    const string16& default_prompt,
                                    const GURL& frame_url,
                                    const int flags,
                                    IPC::Message* reply_msg,
                                    bool* did_suppress_message) OVERRIDE;
  virtual void RunBeforeUnloadConfirm(const RenderViewHost* rvh,
                                      const string16& message,
                                      IPC::Message* reply_msg) OVERRIDE;
  virtual content::RendererPreferences GetRendererPrefs(
      content::BrowserContext* browser_context) const OVERRIDE;
  virtual WebPreferences GetWebkitPrefs() OVERRIDE;
  virtual void OnUserGesture() OVERRIDE;
  virtual void OnIgnoredUIEvent() OVERRIDE;
  virtual void RendererUnresponsive(RenderViewHost* render_view_host,
                                    bool is_during_unload) OVERRIDE;
  virtual void RendererResponsive(RenderViewHost* render_view_host) OVERRIDE;
  virtual void LoadStateChanged(const GURL& url,
                                const net::LoadStateWithParam& load_state,
                                uint64 upload_position,
                                uint64 upload_size) OVERRIDE;
  virtual void WorkerCrashed() OVERRIDE;
  virtual void Activate() OVERRIDE;
  virtual void Deactivate() OVERRIDE;
  virtual void LostCapture() OVERRIDE;
  virtual bool PreHandleKeyboardEvent(const NativeWebKeyboardEvent& event,
                                      bool* is_keyboard_shortcut) OVERRIDE;
  virtual void HandleKeyboardEvent(
      const NativeWebKeyboardEvent& event) OVERRIDE;
  virtual void HandleMouseDown() OVERRIDE;
  virtual void HandleMouseUp() OVERRIDE;
  virtual void HandleMouseActivate() OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void RunFileChooser(
      RenderViewHost* render_view_host,
      const content::FileChooserParams& params) OVERRIDE;
  virtual void ToggleFullscreenMode(bool enter_fullscreen) OVERRIDE;
  virtual bool IsFullscreenForCurrentTab() const OVERRIDE;
  virtual void UpdatePreferredSize(const gfx::Size& pref_size) OVERRIDE;
  virtual void WebUISend(RenderViewHost* render_view_host,
                       const GURL& source_url,
                       const std::string& name,
                       const base::ListValue& args) OVERRIDE;
  virtual void RequestToLockMouse() OVERRIDE;
  virtual void LostMouseLock() OVERRIDE;

 protected:
  friend class TabContentsObserver;

  // Add and remove observers for page navigation notifications. Adding or
  // removing multiple times has no effect. The order in which notifications
  // are sent to observers is undefined. Clients must be sure to remove the
  // observer before they go away.
  void AddObserver(TabContentsObserver* observer);
  void RemoveObserver(TabContentsObserver* observer);

 private:
  friend class NavigationController;

  FRIEND_TEST_ALL_PREFIXES(TabContentsTest, NoJSMessageOnInterstitials);
  FRIEND_TEST_ALL_PREFIXES(TabContentsTest, UpdateTitle);
  FRIEND_TEST_ALL_PREFIXES(TabContentsTest, CrossSiteCantPreemptAfterUnload);
  FRIEND_TEST_ALL_PREFIXES(FormStructureBrowserTest, HTMLFiles);
  FRIEND_TEST_ALL_PREFIXES(NavigationControllerTest, HistoryNavigate);
  FRIEND_TEST_ALL_PREFIXES(RenderViewHostManagerTest, PageDoesBackAndReload);

  // Temporary until the view/contents separation is complete.
  friend class TabContentsView;
#if defined(TOOLKIT_VIEWS)
  friend class TabContentsViewViews;
#elif defined(OS_MACOSX)
  friend class TabContentsViewMac;
#elif defined(TOOLKIT_USES_GTK)
  friend class TabContentsViewGtk;
#endif

  // So InterstitialPage can access SetIsLoading.
  friend class InterstitialPage;

  // TODO(brettw) TestTabContents shouldn't exist!
  friend class TestTabContents;

  // Message handlers.
  void OnRegisterIntentService(const string16& action,
                               const string16& type,
                               const string16& href,
                               const string16& title,
                               const string16& disposition);
  void OnWebIntentDispatch(const IPC::Message& message,
                           const webkit_glue::WebIntentData& intent,
                           int intent_id);
  void OnDidStartProvisionalLoadForFrame(int64 frame_id,
                                         bool main_frame,
                                         const GURL& opener_url,
                                         const GURL& url);
  void OnDidRedirectProvisionalLoad(int32 page_id,
                                    const GURL& opener_url,
                                    const GURL& source_url,
                                    const GURL& target_url);
  void OnDidFailProvisionalLoadWithError(
      const ViewHostMsg_DidFailProvisionalLoadWithError_Params& params);
  void OnDidLoadResourceFromMemoryCache(const GURL& url,
                                        const std::string& security_info,
                                        const std::string& http_request,
                                        ResourceType::Type resource_type);
  void OnDidDisplayInsecureContent();
  void OnDidRunInsecureContent(const std::string& security_origin,
                               const GURL& target_url);
  void OnDocumentLoadedInFrame(int64 frame_id);
  void OnDidFinishLoad(int64 frame_id);
  void OnUpdateContentRestrictions(int restrictions);
  void OnGoToEntryAtOffset(int offset);
  void OnUpdateZoomLimits(int minimum_percent,
                          int maximum_percent,
                          bool remember);
  void OnFocusedNodeChanged(bool is_editable_node);
  void OnEnumerateDirectory(int request_id, const FilePath& path);
  void OnJSOutOfMemory();
  void OnRegisterProtocolHandler(const std::string& protocol,
                                 const GURL& url,
                                 const string16& title);
  void OnFindReply(int request_id, int number_of_matches,
                   const gfx::Rect& selection_rect, int active_match_ordinal,
                   bool final_update);
  void OnCrashedPlugin(const FilePath& plugin_path);
  void OnAppCacheAccessed(const GURL& manifest_url, bool blocked_by_policy);

  // Changes the IsLoading state and notifies delegate as needed
  // |details| is used to provide details on the load that just finished
  // (but can be null if not applicable). Can be overridden.
  void SetIsLoading(bool is_loading,
                    LoadNotificationDetails* details);

  // Called by derived classes to indicate that we're no longer waiting for a
  // response. This won't actually update the throbber, but it will get picked
  // up at the next animation step if the throbber is going.
  void SetNotWaitingForResponse() { waiting_for_response_ = false; }

  // Navigation helpers --------------------------------------------------------
  //
  // These functions are helpers for Navigate() and DidNavigate().

  // Handles post-navigation tasks in DidNavigate AFTER the entry has been
  // committed to the navigation controller. Note that the navigation entry is
  // not provided since it may be invalid/changed after being committed. The
  // current navigation entry is in the NavigationController at this point.
  void DidNavigateMainFramePostCommit(
      const content::LoadCommittedDetails& details,
      const ViewHostMsg_FrameNavigate_Params& params);
  void DidNavigateAnyFramePostCommit(
      RenderViewHost* render_view_host,
      const content::LoadCommittedDetails& details,
      const ViewHostMsg_FrameNavigate_Params& params);

  // If our controller was restored and the page id is > than the site
  // instance's page id, the site instances page id is updated as well as the
  // renderers max page id.
  void UpdateMaxPageIDIfNecessary(SiteInstance* site_instance,
                                  RenderViewHost* rvh);

  // Saves the given title to the navigation entry and does associated work. It
  // will update history and the view for the new title, and also synthesize
  // titles for file URLs that have none (so we require that the URL of the
  // entry already be set).
  //
  // This is used as the backend for state updates, which include a new title,
  // or the dedicated set title message. It returns true if the new title is
  // different and was therefore updated.
  bool UpdateTitleForEntry(NavigationEntry* entry, const string16& title);

  // Causes the TabContents to navigate in the right renderer to |entry|, which
  // must be already part of the entries in the navigation controller.
  // This does not change the NavigationController state.
  bool NavigateToEntry(const NavigationEntry& entry,
                       NavigationController::ReloadType reload_type);

  // Sets the history for this tab_contents to |history_length| entries, and
  // moves the current page_id to the last entry in the list if it's valid.
  // This is mainly used when a prerendered page is swapped into the current
  // tab. The method is virtual for testing.
  virtual void SetHistoryLengthAndPrune(const SiteInstance* site_instance,
                                        int merge_history_length,
                                        int32 minimum_page_id);

  // Misc non-view stuff -------------------------------------------------------

  // Helper functions for sending notifications.
  void NotifySwapped();
  void NotifyConnected();
  void NotifyDisconnected();

  // RenderViewHostManager::Delegate -------------------------------------------

  virtual void BeforeUnloadFiredFromRenderManager(
      bool proceed,
      bool* proceed_to_fire_unload) OVERRIDE;
  virtual void DidStartLoadingFromRenderManager(
      RenderViewHost* render_view_host) OVERRIDE;
  virtual void RenderViewGoneFromRenderManager(
      RenderViewHost* render_view_host) OVERRIDE;
  virtual void UpdateRenderViewSizeForRenderManager() OVERRIDE;
  virtual void NotifySwappedFromRenderManager() OVERRIDE;
  virtual NavigationController& GetControllerForRenderManager() OVERRIDE;
  virtual WebUI* CreateWebUIForRenderManager(const GURL& url) OVERRIDE;
  virtual NavigationEntry*
      GetLastCommittedNavigationEntryForRenderManager() OVERRIDE;

  // Initializes the given renderer if necessary and creates the view ID
  // corresponding to this view host. If this method is not called and the
  // process is not shared, then the TabContents will act as though the renderer
  // is not running (i.e., it will render "sad tab"). This method is
  // automatically called from LoadURL.
  //
  // If you are attaching to an already-existing RenderView, you should call
  // InitWithExistingID.
  virtual bool CreateRenderViewForRenderManager(
      RenderViewHost* render_view_host) OVERRIDE;

  // Stores random bits of data for others to associate with this object.
  // WARNING: this needs to be deleted after NavigationController.
  base::PropertyBag property_bag_;

  // Data for core operation ---------------------------------------------------

  // Delegate for notifying our owner about stuff. Not owned by us.
  TabContentsDelegate* delegate_;

  // Handles the back/forward list and loading.
  NavigationController controller_;

  // The corresponding view.
  scoped_ptr<TabContentsView> view_;

  // A list of observers notified when page state changes. Weak references.
  // This MUST be listed above render_manager_ since at destruction time the
  // latter might cause RenderViewHost's destructor to call us and we might use
  // the observer list then.
  ObserverList<TabContentsObserver> observers_;

  // Helper classes ------------------------------------------------------------

  // Manages creation and swapping of render views.
  RenderViewHostManager render_manager_;

  // Manages injecting Java objects into all RenderViewHosts associated with
  // this TabContents.
  scoped_ptr<JavaBridgeDispatcherHostManager>
      java_bridge_dispatcher_host_manager_;

  // SavePackage, lazily created.
  scoped_refptr<SavePackage> save_package_;

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
  net::LoadStateWithParam load_state_;
  string16 load_state_host_;
  // Upload progress, for displaying in the status bar.
  // Set to zero when there is no significant upload happening.
  uint64 upload_size_;
  uint64 upload_position_;

  // Data for current page -----------------------------------------------------

  // When a title cannot be taken from any entry, this title will be used.
  string16 page_title_when_no_navigation_entry_;

  // When a navigation occurs, we record its contents MIME type. It can be
  // used to check whether we can do something for some special contents.
  std::string contents_mime_type_;

  // Character encoding.
  std::string encoding_;

  // True if this is a secure page which displayed insecure content.
  bool displayed_insecure_content_;

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

  // Pointer to the JavaScript dialog creator, lazily assigned. Used because the
  // delegate of this TabContents is nulled before its destructor is called.
  content::JavaScriptDialogCreator* dialog_creator_;

#if defined(OS_WIN)
  // Handle to an event that's set when the page is showing a message box (or
  // equivalent constrained window).  Plugin processes check this to know if
  // they should pump messages then.
  base::win::ScopedHandle message_box_active_;
#endif

  // Set to true when there is an active "before unload" dialog.  When true,
  // we've forced the throbber to start in Navigate, and we need to remember to
  // turn it off in OnJavaScriptMessageBoxClosed if the navigation is canceled.
  bool is_showing_before_unload_dialog_;

  // Settings that get passed to the renderer process.
  content::RendererPreferences renderer_preferences_;

  // If this tab was created from a renderer using window.open, this will be
  // non-NULL and represent the WebUI of the opening renderer.
  WebUI::TypeID opener_web_ui_type_;

  // The time that we started to create the new tab page.
  base::TimeTicks new_tab_start_time_;

  // The time that we started to close the tab.
  base::TimeTicks tab_close_start_time_;

  // The time that this tab was last selected.
  base::TimeTicks last_selected_time_;

  // See description above setter.
  bool closed_by_user_gesture_;

  // Minimum/maximum zoom percent.
  int minimum_zoom_percent_;
  int maximum_zoom_percent_;
  // If true, the default zoom limits have been overriden for this tab, in which
  // case we don't want saved settings to apply to it and we don't want to
  // remember it.
  bool temporary_zoom_settings_;

  // Content restrictions, used to disable print/copy etc based on content's
  // (full-page plugins for now only) permissions.
  int content_restrictions_;

  // Our view type. Default is VIEW_TYPE_TAB_CONTENTS.
  content::ViewType view_type_;

  DISALLOW_COPY_AND_ASSIGN(TabContents);
};

#endif  // CONTENT_BROWSER_TAB_CONTENTS_TAB_CONTENTS_H_
