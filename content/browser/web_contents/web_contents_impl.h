// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_IMPL_H_
#define CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_IMPL_H_
#pragma once

#include <map>
#include <string>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/property_bag.h"
#include "content/browser/browser_plugin/browser_plugin_web_contents_observer.h"
#include "content/browser/renderer_host/java/java_bridge_dispatcher_host_manager.h"
#include "content/browser/web_contents/navigation_controller_impl.h"
#include "content/browser/web_contents/render_view_host_manager.h"
#include "content/common/content_export.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/render_view_host_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/renderer_preferences.h"
#include "net/base/load_states.h"
#include "ui/gfx/size.h"
#include "webkit/glue/resource_type.h"

#if defined(OS_WIN)
#include "base/win/scoped_handle.h"
#endif

class InterstitialPageImpl;
class SavePackage;
class SessionStorageNamespaceImpl;

namespace content {
class ColorChooser;
class DownloadItem;
class JavaScriptDialogCreator;
class RenderViewHost;
class RenderViewHostImpl;
class SiteInstance;
class TestWebContents;
class WebContentsDelegate;
class WebContentsObserver;
class WebContentsView;
struct LoadNotificationDetails;
}

namespace webkit_glue {
struct WebIntentData;
}

class CONTENT_EXPORT WebContentsImpl
    : public NON_EXPORTED_BASE(content::WebContents),
      public content::RenderViewHostDelegate,
      public RenderViewHostManager::Delegate,
      public content::NotificationObserver {
 public:
  // See WebContents::Create for a description of these parameters.
  WebContentsImpl(content::BrowserContext* browser_context,
                  content::SiteInstance* site_instance,
                  int routing_id,
                  const WebContentsImpl* base_web_contents,
                  WebContentsImpl* opener,
                  SessionStorageNamespaceImpl* session_storage_namespace);
  virtual ~WebContentsImpl();

  // Returns the content specific prefs for the given RVH.
  static WebPreferences GetWebkitPrefs(
      content::RenderViewHost* rvh, const GURL& url);

  // Returns the SavePackage which manages the page saving job. May be NULL.
  SavePackage* save_package() const { return save_package_.get(); }

  // Updates the max page ID for the current SiteInstance in this
  // WebContentsImpl to be at least |page_id|.
  void UpdateMaxPageID(int32 page_id);

  // Updates the max page ID for the given SiteInstance in this WebContentsImpl
  // to be at least |page_id|.
  void UpdateMaxPageIDForSiteInstance(content::SiteInstance* site_instance,
                                      int32 page_id);

  // Copy the current map of SiteInstance ID to max page ID from another tab.
  // This is necessary when this tab adopts the NavigationEntries from
  // |web_contents|.
  void CopyMaxPageIDsFrom(WebContentsImpl* web_contents);

  // Called by the NavigationController to cause the WebContentsImpl to navigate
  // to the current pending entry. The NavigationController should be called
  // back with RendererDidNavigate on success or DiscardPendingEntry on failure.
  // The callbacks can be inside of this function, or at some future time.
  //
  // The entry has a PageID of -1 if newly created (corresponding to navigation
  // to a new URL).
  //
  // If this method returns false, then the navigation is discarded (equivalent
  // to calling DiscardPendingEntry on the NavigationController).
  bool NavigateToPendingEntry(
      content::NavigationController::ReloadType reload_type);

  // Sets the passed passed interstitial as the currently showing interstitial.
  // |interstitial_page| should be non NULL (use the remove_interstitial_page
  // method to unset the interstitial) and no interstitial page should be set
  // when there is already a non NULL interstitial page set.
  void set_interstitial_page(InterstitialPageImpl* interstitial_page) {
    render_manager_.set_interstitial_page(interstitial_page);
  }

  // Unsets the currently showing interstitial.
  void remove_interstitial_page() {
    render_manager_.remove_interstitial_page();
  }

  void set_opener_web_ui_type(content::WebUI::TypeID opener_web_ui_type) {
    opener_web_ui_type_ = opener_web_ui_type;
  }

  JavaBridgeDispatcherHostManager* java_bridge_dispatcher_host_manager() const {
    return java_bridge_dispatcher_host_manager_.get();
  }

  content::BrowserPluginWebContentsObserver*
      browser_plugin_web_contents_observer() const {
    return browser_plugin_web_contents_observer_.get();
  }

  // Like GetController from WebContents, but returns the concrete object.
  NavigationControllerImpl& GetControllerImpl();

  // Expose the render manager for testing.
  RenderViewHostManager* GetRenderManagerForTesting();

  // content::WebContents ------------------------------------------------------
  virtual const base::PropertyBag* GetPropertyBag() const OVERRIDE;
  virtual base::PropertyBag* GetPropertyBag() OVERRIDE;
  virtual content::WebContentsDelegate* GetDelegate() OVERRIDE;
  virtual void SetDelegate(content::WebContentsDelegate* delegate) OVERRIDE;
  virtual content::NavigationController& GetController() OVERRIDE;
  virtual const content::NavigationController& GetController() const OVERRIDE;
  virtual content::BrowserContext* GetBrowserContext() const OVERRIDE;
  virtual void SetViewType(content::ViewType type) OVERRIDE;
  virtual content::ViewType GetViewType() const OVERRIDE;
  virtual content::RenderProcessHost* GetRenderProcessHost() const OVERRIDE;
  virtual content::RenderViewHost* GetRenderViewHost() const OVERRIDE;
  virtual content::RenderWidgetHostView*
      GetRenderWidgetHostView() const OVERRIDE;
  virtual content::WebContentsView* GetView() const OVERRIDE;
  virtual content::WebUI* CreateWebUI(const GURL& url) OVERRIDE;
  virtual content::WebUI* GetWebUI() const OVERRIDE;
  virtual content::WebUI* GetCommittedWebUI() const OVERRIDE;
  virtual const string16& GetTitle() const OVERRIDE;
  virtual int32 GetMaxPageID() OVERRIDE;
  virtual int32 GetMaxPageIDForSiteInstance(
      content::SiteInstance* site_instance) OVERRIDE;
  virtual content::SiteInstance* GetSiteInstance() const OVERRIDE;
  virtual content::SiteInstance* GetPendingSiteInstance() const OVERRIDE;
  virtual bool IsLoading() const OVERRIDE;
  virtual bool IsWaitingForResponse() const OVERRIDE;
  virtual const net::LoadStateWithParam& GetLoadState() const OVERRIDE;
  virtual const string16& GetLoadStateHost() const OVERRIDE;
  virtual uint64 GetUploadSize() const OVERRIDE;
  virtual uint64 GetUploadPosition() const OVERRIDE;
  virtual const std::string& GetEncoding() const OVERRIDE;
  virtual bool DisplayedInsecureContent() const OVERRIDE;
  virtual void SetCapturingContents(bool cap) OVERRIDE;
  virtual bool IsCrashed() const OVERRIDE;
  virtual void SetIsCrashed(base::TerminationStatus status,
                            int error_code) OVERRIDE;
  virtual base::TerminationStatus GetCrashedStatus() const OVERRIDE;
  virtual bool IsBeingDestroyed() const OVERRIDE;
  virtual void NotifyNavigationStateChanged(unsigned changed_flags) OVERRIDE;
  virtual void DidBecomeSelected() OVERRIDE;
  virtual base::TimeTicks GetLastSelectedTime() const OVERRIDE;
  virtual void WasHidden() OVERRIDE;
  virtual void ShowContents() OVERRIDE;
  virtual void HideContents() OVERRIDE;
  virtual bool NeedToFireBeforeUnload() OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual content::WebContents* Clone() OVERRIDE;
  virtual void AddNewContents(content::WebContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture) OVERRIDE;
  virtual gfx::NativeView GetContentNativeView() const OVERRIDE;
  virtual gfx::NativeView GetNativeView() const OVERRIDE;
  virtual void GetContainerBounds(gfx::Rect* out) const OVERRIDE;
  virtual void Focus() OVERRIDE;
  virtual void FocusThroughTabTraversal(bool reverse) OVERRIDE;
  virtual bool ShowingInterstitialPage() const OVERRIDE;
  virtual content::InterstitialPage* GetInterstitialPage() const OVERRIDE;
  virtual bool IsSavable() OVERRIDE;
  virtual void OnSavePage() OVERRIDE;
  virtual bool SavePage(const FilePath& main_file,
                        const FilePath& dir_path,
                        content::SavePageType save_type) OVERRIDE;
  virtual void GenerateMHTML(
      const FilePath& file,
      const base::Callback<void(const FilePath&, int64)>& callback) OVERRIDE;
  virtual bool IsActiveEntry(int32 page_id) OVERRIDE;

  virtual const std::string& GetContentsMimeType() const OVERRIDE;
  virtual bool WillNotifyDisconnection() const OVERRIDE;
  virtual void SetOverrideEncoding(const std::string& encoding) OVERRIDE;
  virtual void ResetOverrideEncoding() OVERRIDE;
  virtual content::RendererPreferences* GetMutableRendererPrefs() OVERRIDE;
  virtual void SetNewTabStartTime(const base::TimeTicks& time) OVERRIDE;
  virtual base::TimeTicks GetNewTabStartTime() const OVERRIDE;
  virtual void OnCloseStarted() OVERRIDE;
  virtual bool ShouldAcceptDragAndDrop() const OVERRIDE;
  virtual void SystemDragEnded() OVERRIDE;
  virtual void SetClosedByUserGesture(bool value) OVERRIDE;
  virtual bool GetClosedByUserGesture() const OVERRIDE;
  virtual double GetZoomLevel() const OVERRIDE;
  virtual int GetZoomPercent(bool* enable_increment,
                             bool* enable_decrement) OVERRIDE;
  virtual void ViewSource() OVERRIDE;
  virtual void ViewFrameSource(const GURL& url,
                               const std::string& content_state) OVERRIDE;
  virtual int GetMinimumZoomPercent() const OVERRIDE;
  virtual int GetMaximumZoomPercent() const OVERRIDE;
  virtual gfx::Size GetPreferredSize() const OVERRIDE;
  virtual int GetContentRestrictions() const OVERRIDE;
  virtual content::WebUI::TypeID GetWebUITypeForCurrentState() OVERRIDE;
  virtual content::WebUI* GetWebUIForCurrentState() OVERRIDE;
  virtual bool GotResponseToLockMouseRequest(bool allowed) OVERRIDE;
  virtual bool HasOpener() const OVERRIDE;
  virtual void DidChooseColorInColorChooser(int color_chooser_id,
                                            SkColor color) OVERRIDE;
  virtual void DidEndColorChooser(int color_chooser_id) OVERRIDE;

  // Implementation of PageNavigator.
  virtual content::WebContents* OpenURL(
      const content::OpenURLParams& params) OVERRIDE;

  // RenderViewHostDelegate ----------------------------------------------------

  virtual content::RenderViewHostDelegate::View* GetViewDelegate() OVERRIDE;
  virtual content::RenderViewHostDelegate::RendererManagement*
      GetRendererManagementDelegate() OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual const GURL& GetURL() const OVERRIDE;
  virtual WebContents* GetAsWebContents() OVERRIDE;
  virtual content::ViewType GetRenderViewType() const OVERRIDE;
  virtual gfx::Rect GetRootWindowResizerRect() const OVERRIDE;
  virtual void RenderViewCreated(
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual void RenderViewReady(
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual void RenderViewGone(content::RenderViewHost* render_view_host,
                              base::TerminationStatus status,
                              int error_code) OVERRIDE;
  virtual void RenderViewDeleted(
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual void DidStartProvisionalLoadForFrame(
      content::RenderViewHost* render_view_host,
      int64 frame_id,
      bool main_frame,
      const GURL& opener_url,
      const GURL& url) OVERRIDE;
  virtual void DidRedirectProvisionalLoad(
      content::RenderViewHost* render_view_host,
      int32 page_id,
      const GURL& opener_url,
      const GURL& source_url,
      const GURL& target_url) OVERRIDE;
  virtual void DidFailProvisionalLoadWithError(
      content::RenderViewHost* render_view_host,
      const ViewHostMsg_DidFailProvisionalLoadWithError_Params& params)
          OVERRIDE;
  virtual void DidNavigate(
      content::RenderViewHost* render_view_host,
      const ViewHostMsg_FrameNavigate_Params& params) OVERRIDE;
  virtual void UpdateState(content::RenderViewHost* render_view_host,
                           int32 page_id,
                           const std::string& state) OVERRIDE;
  virtual void UpdateTitle(content::RenderViewHost* render_view_host,
                           int32 page_id,
                           const string16& title,
                           base::i18n::TextDirection title_direction) OVERRIDE;
  virtual void UpdateEncoding(content::RenderViewHost* render_view_host,
                              const std::string& encoding) OVERRIDE;
  virtual void UpdateTargetURL(int32 page_id, const GURL& url) OVERRIDE;
  virtual void Close(content::RenderViewHost* render_view_host) OVERRIDE;
  virtual void RequestMove(const gfx::Rect& new_bounds) OVERRIDE;
  virtual void SwappedOut(content::RenderViewHost* render_view_host) OVERRIDE;
  virtual void DidStartLoading() OVERRIDE;
  virtual void DidStopLoading() OVERRIDE;
  virtual void DidCancelLoading() OVERRIDE;
  virtual void DidChangeLoadProgress(double progress) OVERRIDE;
  virtual void DocumentAvailableInMainFrame(
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual void DocumentOnLoadCompletedInMainFrame(
      content::RenderViewHost* render_view_host,
      int32 page_id) OVERRIDE;
  virtual void RequestOpenURL(content::RenderViewHost* rvh,
                              const GURL& url,
                              const content::Referrer& referrer,
                              WindowOpenDisposition disposition,
                              int64 source_frame_id) OVERRIDE;
  virtual void RequestTransferURL(
      const GURL& url,
      const content::Referrer& referrer,
      WindowOpenDisposition disposition,
      int64 source_frame_id,
      const content::GlobalRequestID& transferred_global_request_id) OVERRIDE;
  virtual void RouteCloseEvent(content::RenderViewHost* rvh) OVERRIDE;
  virtual void RunJavaScriptMessage(content::RenderViewHost* rvh,
                                    const string16& message,
                                    const string16& default_prompt,
                                    const GURL& frame_url,
                                    ui::JavascriptMessageType type,
                                    IPC::Message* reply_msg,
                                    bool* did_suppress_message) OVERRIDE;
  virtual void RunBeforeUnloadConfirm(content::RenderViewHost* rvh,
                                      const string16& message,
                                      bool is_reload,
                                      IPC::Message* reply_msg) OVERRIDE;
  virtual content::RendererPreferences GetRendererPrefs(
      content::BrowserContext* browser_context) const OVERRIDE;
  virtual WebPreferences GetWebkitPrefs() OVERRIDE;
  virtual void OnUserGesture() OVERRIDE;
  virtual void OnIgnoredUIEvent() OVERRIDE;
  virtual void RendererUnresponsive(content::RenderViewHost* render_view_host,
                                    bool is_during_unload) OVERRIDE;
  virtual void RendererResponsive(
      content::RenderViewHost* render_view_host) OVERRIDE;
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
  virtual void RunFileChooser(
      content::RenderViewHost* render_view_host,
      const content::FileChooserParams& params) OVERRIDE;
  virtual void ToggleFullscreenMode(bool enter_fullscreen) OVERRIDE;
  virtual bool IsFullscreenForCurrentTab() const OVERRIDE;
  virtual void UpdatePreferredSize(const gfx::Size& pref_size) OVERRIDE;
  virtual void ResizeDueToAutoResize(const gfx::Size& new_size) OVERRIDE;
  virtual void RequestToLockMouse(bool user_gesture) OVERRIDE;
  virtual void LostMouseLock() OVERRIDE;

  // RenderViewHostManager::Delegate -------------------------------------------

  virtual bool CreateRenderViewForRenderManager(
      content::RenderViewHost* render_view_host, int opener_route_id) OVERRIDE;
  virtual void BeforeUnloadFiredFromRenderManager(
      bool proceed,
      bool* proceed_to_fire_unload) OVERRIDE;
  virtual void DidStartLoadingFromRenderManager(
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual void RenderViewGoneFromRenderManager(
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual void UpdateRenderViewSizeForRenderManager() OVERRIDE;
  virtual void NotifySwappedFromRenderManager() OVERRIDE;
  virtual int CreateOpenerRenderViewsForRenderManager(
      content::SiteInstance* instance) OVERRIDE;
  virtual NavigationControllerImpl& GetControllerForRenderManager() OVERRIDE;
  virtual WebUIImpl* CreateWebUIForRenderManager(const GURL& url) OVERRIDE;
  virtual content::NavigationEntry*
      GetLastCommittedNavigationEntryForRenderManager() OVERRIDE;
  virtual bool FocusLocationBarByDefault() OVERRIDE;
  virtual void SetFocusToLocationBar(bool select_all) OVERRIDE;
  virtual void CreateViewAndSetSizeForRVH(
      content::RenderViewHost* rvh) OVERRIDE;

  // content::NotificationObserver ---------------------------------------------

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 protected:
  friend class content::WebContentsObserver;

  // Add and remove observers for page navigation notifications. Adding or
  // removing multiple times has no effect. The order in which notifications
  // are sent to observers is undefined. Clients must be sure to remove the
  // observer before they go away.
  void AddObserver(content::WebContentsObserver* observer);
  void RemoveObserver(content::WebContentsObserver* observer);

 private:
  friend class NavigationControllerImpl;

  FRIEND_TEST_ALL_PREFIXES(WebContentsImplTest, NoJSMessageOnInterstitials);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplTest, UpdateTitle);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplTest,
                           CrossSiteCantPreemptAfterUnload);
  FRIEND_TEST_ALL_PREFIXES(FormStructureBrowserTest, HTMLFiles);
  FRIEND_TEST_ALL_PREFIXES(NavigationControllerTest, HistoryNavigate);
  FRIEND_TEST_ALL_PREFIXES(RenderViewHostManagerTest, PageDoesBackAndReload);

  // So InterstitialPageImpl can access SetIsLoading.
  friend class InterstitialPageImpl;

  // TODO(brettw) TestWebContents shouldn't exist!
  friend class content::TestWebContents;

  // Clears this tab's opener if it has been closed.
  void OnWebContentsDestroyed(content::WebContents* web_contents);

  // Callback function when showing JS dialogs.
  void OnDialogClosed(content::RenderViewHost* rvh,
                      IPC::Message* reply_msg,
                      bool success,
                      const string16& user_input);

  // IPC message handlers.
  void OnRegisterIntentService(const string16& action,
                               const string16& type,
                               const string16& href,
                               const string16& title,
                               const string16& disposition);
  void OnWebIntentDispatch(const webkit_glue::WebIntentData& intent,
                           int intent_id);
  void OnDidLoadResourceFromMemoryCache(const GURL& url,
                                        const std::string& security_info,
                                        const std::string& http_request,
                                        ResourceType::Type resource_type);
  void OnDidDisplayInsecureContent();
  void OnDidRunInsecureContent(const std::string& security_origin,
                               const GURL& target_url);
  void OnDocumentLoadedInFrame(int64 frame_id);
  void OnDidFinishLoad(int64 frame_id,
                       const GURL& validated_url,
                       bool is_main_frame);
  void OnDidFailLoadWithError(int64 frame_id,
                              const GURL& validated_url,
                              bool is_main_frame,
                              int error_code,
                              const string16& error_description);
  void OnUpdateContentRestrictions(int restrictions);
  void OnGoToEntryAtOffset(int offset);
  void OnUpdateZoomLimits(int minimum_percent,
                          int maximum_percent,
                          bool remember);
  void OnSaveURL(const GURL& url);
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
  void OnOpenColorChooser(int color_chooser_id, SkColor color);
  void OnEndColorChooser(int color_chooser_id);
  void OnSetSelectedColorInColorChooser(int color_chooser_id, SkColor color);
  void OnPepperPluginHung(int plugin_child_id,
                          const FilePath& path,
                          bool is_hung);
  void OnWebUISend(const GURL& source_url,
                   const std::string& name,
                   const base::ListValue& args);

  // Changes the IsLoading state and notifies delegate as needed
  // |details| is used to provide details on the load that just finished
  // (but can be null if not applicable). Can be overridden.
  void SetIsLoading(bool is_loading,
                    content::LoadNotificationDetails* details);

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
      content::RenderViewHost* render_view_host,
      const content::LoadCommittedDetails& details,
      const ViewHostMsg_FrameNavigate_Params& params);

  // If our controller was restored, update the max page ID associated with the
  // given RenderViewHost to be larger than the number of restored entries.
  // This is called in CreateRenderView before any navigations in the RenderView
  // have begun, to prevent any races in updating RenderView::next_page_id.
  void UpdateMaxPageIDIfNecessary(content::RenderViewHost* rvh);

  // Saves the given title to the navigation entry and does associated work. It
  // will update history and the view for the new title, and also synthesize
  // titles for file URLs that have none (so we require that the URL of the
  // entry already be set).
  //
  // This is used as the backend for state updates, which include a new title,
  // or the dedicated set title message. It returns true if the new title is
  // different and was therefore updated.
  bool UpdateTitleForEntry(content::NavigationEntryImpl* entry,
                           const string16& title);

  // Causes the WebContentsImpl to navigate in the right renderer to |entry|,
  // which must be already part of the entries in the navigation controller.
  // This does not change the NavigationController state.
  bool NavigateToEntry(const content::NavigationEntryImpl& entry,
                       content::NavigationController::ReloadType reload_type);

  // Sets the history for this WebContentsImpl to |history_length| entries, and
  // moves the current page_id to the last entry in the list if it's valid.
  // This is mainly used when a prerendered page is swapped into the current
  // tab. The method is virtual for testing.
  virtual void SetHistoryLengthAndPrune(
      const content::SiteInstance* site_instance,
      int merge_history_length,
      int32 minimum_page_id);

  // Recursively creates swapped out RenderViews for this tab's opener chain
  // (including this tab) in the given SiteInstance, allowing other tabs to send
  // cross-process JavaScript calls to their opener(s).  Returns the route ID of
  // this tab's RenderView for |instance|.
  int CreateOpenerRenderViews(content::SiteInstance* instance);

  // Misc non-view stuff -------------------------------------------------------

  // Helper functions for sending notifications.
  void NotifySwapped();
  void NotifyConnected();
  void NotifyDisconnected();

  void SetEncoding(const std::string& encoding);

  // Save a URL to the local filesystem.
  void SaveURL(const GURL& url, const GURL& referrer, bool is_main_frame);

  content::RenderViewHostImpl* GetRenderViewHostImpl();

  // Stores random bits of data for others to associate with this object.
  // WARNING: this needs to be deleted after NavigationController.
  base::PropertyBag property_bag_;

  // Listen for notifications as well.
  content::NotificationRegistrar registrar_;

  // Data for core operation ---------------------------------------------------

  // Delegate for notifying our owner about stuff. Not owned by us.
  content::WebContentsDelegate* delegate_;

  // Handles the back/forward list and loading.
  NavigationControllerImpl controller_;

  // The corresponding view.
  scoped_ptr<content::WebContentsView> view_;

  // A list of observers notified when page state changes. Weak references.
  // This MUST be listed above render_manager_ since at destruction time the
  // latter might cause RenderViewHost's destructor to call us and we might use
  // the observer list then.
  ObserverList<content::WebContentsObserver> observers_;

  // The tab that opened this tab, if any.  Will be set to null if the opener
  // is closed.
  WebContentsImpl* opener_;

  // Helper classes ------------------------------------------------------------

  // Manages creation and swapping of render views.
  RenderViewHostManager render_manager_;

  // Manages injecting Java objects into all RenderViewHosts associated with
  // this WebContentsImpl.
  scoped_ptr<JavaBridgeDispatcherHostManager>
      java_bridge_dispatcher_host_manager_;

  scoped_ptr<content::BrowserPluginWebContentsObserver>
      browser_plugin_web_contents_observer_;
  // SavePackage, lazily created.
  scoped_refptr<SavePackage> save_package_;

  // Data for loading state ----------------------------------------------------

  // Indicates whether we're currently loading a resource.
  bool is_loading_;

  // Indicates if the tab is considered crashed.
  base::TerminationStatus crashed_status_;
  int crashed_error_code_;

  // Whether this WebContents is waiting for a first-response for the
  // main resource of the page. This controls whether the throbber state is
  // "waiting" or "loading."
  bool waiting_for_response_;

  // Map of SiteInstance ID to max page ID for this tab. A page ID is specific
  // to a given tab and SiteInstance, and must be valid for the lifetime of the
  // WebContentsImpl.
  std::map<int32, int32> max_page_ids_;

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

  // Whether the WebContents is currently being screenshotted.
  bool capturing_contents_;

  // See getter above.
  bool is_being_destroyed_;

  // Indicates whether we should notify about disconnection of this
  // WebContentsImpl. This is used to ensure disconnection notifications only
  // happen if a connection notification has happened and that they happen only
  // once.
  bool notify_disconnection_;

  // Pointer to the JavaScript dialog creator, lazily assigned. Used because the
  // delegate of this WebContentsImpl is nulled before its destructor is called.
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
  content::WebUI::TypeID opener_web_ui_type_;

  // The time that we started to create the new tab page.
  base::TimeTicks new_tab_start_time_;

  // The time that we started to close this WebContents.
  base::TimeTicks close_start_time_;

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

  // The intrinsic size of the page.
  gfx::Size preferred_size_;

  // Content restrictions, used to disable print/copy etc based on content's
  // (full-page plugins for now only) permissions.
  int content_restrictions_;

  // Our view type. Default is VIEW_TYPE_WEB_CONTENTS.
  content::ViewType view_type_;

  // Color chooser that was opened by this tab.
  content::ColorChooser* color_chooser_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsImpl);
};

#endif  // CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_IMPL_H_
