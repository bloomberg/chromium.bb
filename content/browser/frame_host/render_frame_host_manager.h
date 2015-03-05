// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_HOST_MANAGER_H_
#define CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_HOST_MANAGER_H_

#include <list>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/site_instance_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/common/referrer.h"
#include "ui/base/page_transition_types.h"

struct FrameMsg_Navigate_Params;

namespace content {
class BrowserContext;
class CrossProcessFrameConnector;
class CrossSiteTransferringRequest;
class FrameTreeNode;
class InterstitialPageImpl;
class NavigationControllerImpl;
class NavigationEntry;
class NavigationEntryImpl;
class NavigationRequest;
class NavigatorTestWithBrowserSideNavigation;
class RenderFrameHost;
class RenderFrameHostDelegate;
class RenderFrameHostImpl;
class RenderFrameHostManagerTest;
class RenderFrameProxyHost;
class RenderViewHost;
class RenderViewHostImpl;
class RenderWidgetHostDelegate;
class RenderWidgetHostView;
class TestWebContents;
class WebUIImpl;
struct CommonNavigationParams;

// Manages RenderFrameHosts for a FrameTreeNode. It maintains a
// current_frame_host() which is the content currently visible to the user. When
// a frame is told to navigate to a different web site (as determined by
// SiteInstance), it will replace its current RenderFrameHost with a new
// RenderFrameHost dedicated to the new SiteInstance, possibly in a new process.
//
// Cross-process navigation works like this:
//
// - RFHM::Navigate determines whether the destination is cross-site, and if so,
//   it creates a pending_render_frame_host_.
//
// - The pending RFH is created in the "navigations suspended" state, meaning no
//   navigation messages are sent to its renderer until the beforeunload handler
//   has a chance to run in the current RFH.
//
// - The current RFH runs its beforeunload handler. If it returns false, we
//   cancel all the pending logic. Otherwise we allow the pending RFH to send
//   the navigation request to its renderer.
//
// - ResourceDispatcherHost receives a ResourceRequest on the IO thread for the
//   main resource load from the pending RFH. It creates a
//   CrossSiteResourceHandler to check whether a process transfer is needed when
//   the request is ready to commit.
//
// - When RDH receives a response, the BufferedResourceHandler determines
//   whether it is a navigation type that doesn't commit (e.g. download, 204 or
//   error page). If so, it sends a message to the new renderer causing it to
//   cancel the request, and the request (e.g. the download) proceeds. In this
//   case, the pending RFH will never become the current RFH, but it remains
//   until the next DidNavigate event for this WebContentsImpl.
//
// - After RDH receives a response and determines that it is safe and not a
//   download, the CrossSiteResourceHandler checks whether a transfer for a
//   redirect is needed. If so, it pauses the network response and starts an
//   identical navigation in a new pending RFH. When the identical request is
//   later received by RDH, the response is transferred and unpaused.
//
// - Otherwise, the network response commits in the pending RFH's renderer,
//   which sends a DidCommitProvisionalLoad message back to the browser process.
//
// - RFHM::CommitPending makes visible the new RFH, and initiates the unload
//   handler in the old RFH. The unload handler will complete in the background.
//
// - RenderFrameHostManager may keep the previous RFH alive as a
//   RenderFrameProxyHost, to be used (for example) if the user goes back. The
//   process only stays live if another tab is using it, but if so, the existing
//   frame relationships will be maintained.
class CONTENT_EXPORT RenderFrameHostManager : public NotificationObserver {
 public:
  // Functions implemented by our owner that we need.
  //
  // TODO(brettw) Clean this up! These are all the functions in WebContentsImpl
  // that are required to run this class. The design should probably be better
  // such that these are more clear.
  //
  // There is additional complexity that some of the functions we need in
  // WebContentsImpl are inherited and non-virtual. These are named with
  // "RenderManager" so that the duplicate implementation of them will be clear.
  //
  // Functions and parameters whose description are prefixed by PlzNavigate are
  // part of a navigation refactoring project, currently behind the
  // enable-browser-side-navigation flag. The idea is to move the logic behind
  // driving navigations from the renderer to the browser.
  class CONTENT_EXPORT Delegate {
   public:
    // Initializes the given renderer if necessary and creates the view ID
    // corresponding to this view host. If this method is not called and the
    // process is not shared, then the WebContentsImpl will act as though the
    // renderer is not running (i.e., it will render "sad tab"). This method is
    // automatically called from LoadURL. |for_main_frame_navigation| indicates
    // whether this RenderViewHost is used to render a top-level frame, so the
    // appropriate RenderWidgetHostView type is used.
    virtual bool CreateRenderViewForRenderManager(
        RenderViewHost* render_view_host,
        int opener_route_id,
        int proxy_routing_id,
        bool for_main_frame_navigation) = 0;
    virtual bool CreateRenderFrameForRenderManager(
        RenderFrameHost* render_frame_host,
        int parent_routing_id,
        int proxy_routing_id) = 0;
    virtual void BeforeUnloadFiredFromRenderManager(
        bool proceed, const base::TimeTicks& proceed_time,
        bool* proceed_to_fire_unload) = 0;
    virtual void RenderProcessGoneFromRenderManager(
        RenderViewHost* render_view_host) = 0;
    virtual void UpdateRenderViewSizeForRenderManager() = 0;
    virtual void CancelModalDialogsForRenderManager() = 0;
    virtual void NotifySwappedFromRenderManager(RenderFrameHost* old_host,
                                                RenderFrameHost* new_host,
                                                bool is_main_frame) = 0;
    // TODO(nasko): This should be removed once extensions no longer use
    // NotificationService. See https://crbug.com/462682.
    virtual void NotifyMainFrameSwappedFromRenderManager(
        RenderViewHost* old_host,
        RenderViewHost* new_host) = 0;
    virtual NavigationControllerImpl&
        GetControllerForRenderManager() = 0;

    // Create swapped out RenderViews in the given SiteInstance for each tab in
    // the opener chain of this tab, if any.  This allows the current tab to
    // make cross-process script calls to its opener(s).  Returns the route ID
    // of the immediate opener, if one exists (otherwise MSG_ROUTING_NONE).
    virtual int CreateOpenerRenderViewsForRenderManager(
        SiteInstance* instance) = 0;

    // Creates a WebUI object for the given URL if one applies. Ownership of the
    // returned pointer will be passed to the caller. If no WebUI applies,
    // returns NULL.
    virtual scoped_ptr<WebUIImpl> CreateWebUIForRenderManager(
        const GURL& url) = 0;

    // Returns the navigation entry of the current navigation, or NULL if there
    // is none.
    virtual NavigationEntry*
        GetLastCommittedNavigationEntryForRenderManager() = 0;

    // Returns true if the location bar should be focused by default rather than
    // the page contents. The view calls this function when the tab is focused
    // to see what it should do.
    virtual bool FocusLocationBarByDefault() = 0;

    // Focuses the location bar.
    virtual void SetFocusToLocationBar(bool select_all) = 0;

    // Returns true if views created for this delegate should be created in a
    // hidden state.
    virtual bool IsHidden() = 0;

   protected:
    virtual ~Delegate() {}
  };

  // Used with FrameTree::ForEach to delete RenderFrameHosts pending shutdown
  // from a FrameTreeNode's RenderFrameHostManager. Used during destruction of
  // WebContentsImpl.
  static bool ClearRFHsPendingShutdown(FrameTreeNode* node);

  // All three delegate pointers must be non-NULL and are not owned by this
  // class.  They must outlive this class. The RenderViewHostDelegate and
  // RenderWidgetHostDelegate are what will be installed into all
  // RenderViewHosts that are created.
  //
  // You must call Init() before using this class.
  RenderFrameHostManager(
      FrameTreeNode* frame_tree_node,
      RenderFrameHostDelegate* render_frame_delegate,
      RenderViewHostDelegate* render_view_delegate,
      RenderWidgetHostDelegate* render_widget_delegate,
      Delegate* delegate);
  ~RenderFrameHostManager() override;

  // For arguments, see WebContentsImpl constructor.
  void Init(BrowserContext* browser_context,
            SiteInstance* site_instance,
            int view_routing_id,
            int frame_routing_id);

  // Returns the currently active RenderFrameHost.
  //
  // This will be non-NULL between Init() and Shutdown(). You may want to NULL
  // check it in many cases, however. Windows can send us messages during the
  // destruction process after it has been shut down.
  RenderFrameHostImpl* current_frame_host() const {
    return render_frame_host_.get();
  }

  // TODO(creis): Remove this when we no longer use RVH for navigation.
  RenderViewHostImpl* current_host() const;

  // Returns the view associated with the current RenderViewHost, or NULL if
  // there is no current one.
  RenderWidgetHostView* GetRenderWidgetHostView() const;

  RenderFrameProxyHost* GetProxyToParent();

  // Returns the pending RenderFrameHost, or NULL if there is no pending one.
  RenderFrameHostImpl* pending_frame_host() const {
    return pending_render_frame_host_.get();
  }

  // TODO(creis): Remove this when we no longer use RVH for navigation.
  RenderViewHostImpl* pending_render_view_host() const;

  // Returns the current committed Web UI or NULL if none applies.
  WebUIImpl* web_ui() const { return web_ui_.get(); }

  // Returns the Web UI for the pending navigation, or NULL of none applies.
  WebUIImpl* pending_web_ui() const {
    return pending_web_ui_.get() ? pending_web_ui_.get() :
                                   pending_and_current_web_ui_.get();
  }

  // PlzNavigate
  // Returns the speculative WebUI for the navigation (a newly created one or
  // the current one if it should be reused). If none is set returns nullptr.
  WebUIImpl* speculative_web_ui() const {
    return should_reuse_web_ui_ ? web_ui_.get() : speculative_web_ui_.get();
  }

  // Called when we want to instruct the renderer to navigate to the given
  // navigation entry. It may create a new RenderFrameHost or re-use an existing
  // one. The RenderFrameHost to navigate will be returned. Returns NULL if one
  // could not be created.
  RenderFrameHostImpl* Navigate(const NavigationEntryImpl& entry);

  // Instructs the various live views to stop. Called when the user directed the
  // page to stop loading.
  void Stop();

  // Notifies the regular and pending RenderViewHosts that a load is or is not
  // happening. Even though the message is only for one of them, we don't know
  // which one so we tell both.
  void SetIsLoading(bool is_loading);

  // Whether to close the tab or not when there is a hang during an unload
  // handler. If we are mid-crosssite navigation, then we should proceed
  // with the navigation instead of closing the tab.
  bool ShouldCloseTabOnUnresponsiveRenderer();

  // Confirms whether we should close the page or navigate away.  This is called
  // before a cross-site request or before a tab/window is closed (as indicated
  // by the first parameter) to allow the appropriate renderer to approve or
  // deny the request.  |proceed| indicates whether the user chose to proceed.
  // |proceed_time| is the time when the request was allowed to proceed.
  void OnBeforeUnloadACK(bool for_cross_site_transition,
                         bool proceed,
                         const base::TimeTicks& proceed_time);

  // The |pending_render_frame_host| is ready to commit a page.  We should
  // ensure that the old RenderFrameHost runs its unload handler first and
  // determine whether a RenderFrameHost transfer is needed.
  // |cross_site_transferring_request| is NULL if a request is not being
  // transferred between renderers.
  void OnCrossSiteResponse(
      RenderFrameHostImpl* pending_render_frame_host,
      const GlobalRequestID& global_request_id,
      scoped_ptr<CrossSiteTransferringRequest> cross_site_transferring_request,
      const std::vector<GURL>& transfer_url_chain,
      const Referrer& referrer,
      ui::PageTransition page_transition,
      bool should_replace_current_entry);

  // Received a response from CrossSiteResourceHandler. If the navigation
  // specifies a transition, this is called and the navigation will not resume
  // until ResumeResponseDeferredAtStart.
  void OnDeferredAfterResponseStarted(
      const GlobalRequestID& global_request_id,
      RenderFrameHostImpl* pending_render_frame_host);

  // Resume navigation paused after receiving response headers.
  void ResumeResponseDeferredAtStart();

  // Clear navigation transition data.
  void ClearNavigationTransitionData();

  // Called when a renderer's frame navigates.
  void DidNavigateFrame(RenderFrameHostImpl* render_frame_host,
                        bool was_caused_by_user_gesture);

  // Called when a renderer sets its opener to null.
  void DidDisownOpener(RenderFrameHost* render_frame_host);

  // Sets the pending Web UI for the pending navigation, ensuring that the
  // bindings are appropriate compared to |bindings|.
  void SetPendingWebUI(const GURL& url, int bindings);

  // Creates and initializes a RenderFrameHost. The |web_ui| is an optional
  // input parameter used to double check bindings when swapping back in a
  // previously existing RenderFrameHost. If |flags| has the
  // CREATE_RF_SWAPPED_OUT bit set from the CreateRenderFrameFlags enum, it will
  // initially be placed on the swapped out hosts list. If |view_routing_id_ptr|
  // is not nullptr it will be set to the routing id of the view associated with
  // the frame.
  scoped_ptr<RenderFrameHostImpl> CreateRenderFrame(SiteInstance* instance,
                                                    WebUIImpl* web_ui,
                                                    int opener_route_id,
                                                    int flags,
                                                    int* view_routing_id_ptr);

  // Helper method to create and initialize a RenderFrameProxyHost and return
  // its routing id.
  int CreateRenderFrameProxy(SiteInstance* instance);

  // Sets the passed passed interstitial as the currently showing interstitial.
  // |interstitial_page| should be non NULL (use the remove_interstitial_page
  // method to unset the interstitial) and no interstitial page should be set
  // when there is already a non NULL interstitial page set.
  void set_interstitial_page(InterstitialPageImpl* interstitial_page) {
    DCHECK(!interstitial_page_ && interstitial_page);
    interstitial_page_ = interstitial_page;
  }

  // Unsets the currently showing interstitial.
  void remove_interstitial_page() {
    DCHECK(interstitial_page_);
    interstitial_page_ = NULL;
  }

  // Returns the currently showing interstitial, NULL if no interstitial is
  // showing.
  InterstitialPageImpl* interstitial_page() const { return interstitial_page_; }

  // NotificationObserver implementation.
  void Observe(int type,
               const NotificationSource& source,
               const NotificationDetails& details) override;

  // Returns whether the given RenderFrameHost (or its associated
  // RenderViewHost) is on the list of swapped out RenderFrameHosts.
  bool IsRVHOnSwappedOutList(RenderViewHostImpl* rvh) const;
  bool IsOnSwappedOutList(RenderFrameHostImpl* rfh) const;

  // Returns the swapped out RenderViewHost or RenderFrameHost for the given
  // SiteInstance, if any. This method is *deprecated* and
  // GetRenderFrameProxyHost should be used.
  RenderViewHostImpl* GetSwappedOutRenderViewHost(SiteInstance* instance) const;
  RenderFrameProxyHost* GetRenderFrameProxyHost(
      SiteInstance* instance) const;

  // Returns whether |render_frame_host| is on the pending deletion list.
  bool IsPendingDeletion(RenderFrameHostImpl* render_frame_host);

  // If |render_frame_host| is on the pending deletion list, this deletes it.
  // Returns whether it was deleted.
  bool DeleteFromPendingList(RenderFrameHostImpl* render_frame_host);

  // Deletes any proxy hosts associated with this node. Used during destruction
  // of WebContentsImpl.
  void ResetProxyHosts();

  // Returns the routing id for a RenderFrameHost or RenderFrameHostProxy
  // that has the given SiteInstance and is associated with this
  // RenderFrameHostManager. Returns MSG_ROUTING_NONE if none is found.
  int GetRoutingIdForSiteInstance(SiteInstance* site_instance);

  // PlzNavigate
  // Notifies the RFHM that a navigation has begun so that it can speculatively
  // create a new RenderFrameHost (and potentially a new process) if needed.
  void BeginNavigation(const NavigationRequest& request);

  // PlzNavigate
  // Called (possibly several times) during a navigation to select or create an
  // appropriate RenderFrameHost for the provided URL. The returned pointer will
  // be for the current or the speculative RenderFrameHost and the instance is
  // owned by this manager.
  RenderFrameHostImpl* GetFrameHostForNavigation(
      const NavigationRequest& request);

  // PlzNavigate
  // Clean up any state for any ongoing navigation.
  void CleanUpNavigation();

  // PlzNavigate
  // Clears the speculative members, returning the RenderFrameHost to the caller
  // for disposal.
  scoped_ptr<RenderFrameHostImpl> UnsetSpeculativeRenderFrameHost();

  // Notification methods to tell this RenderFrameHostManager that the frame it
  // is responsible for has started or stopped loading a document.
  void OnDidStartLoading();
  void OnDidStopLoading();

  void EnsureRenderViewInitialized(FrameTreeNode* source,
                                   RenderViewHostImpl* render_view_host,
                                   SiteInstance* instance);

 private:
  friend class NavigatorTestWithBrowserSideNavigation;
  friend class RenderFrameHostManagerTest;
  friend class TestWebContents;

  FRIEND_TEST_ALL_PREFIXES(CrossProcessFrameTreeBrowserTest,
                           CreateCrossProcessSubframeProxies);

  // Used with FrameTree::ForEach to erase RenderFrameProxyHosts from a
  // FrameTreeNode's RenderFrameHostManager.
  static bool ClearProxiesInSiteInstance(int32 site_instance_id,
                                         FrameTreeNode* node);
  // Used with FrameTree::ForEach to reset initialized state of
  // RenderFrameProxyHosts from a FrameTreeNode's RenderFrameHostManager.
  static bool ResetProxiesInSiteInstance(int32 site_instance_id,
                                         FrameTreeNode* node);

  // Returns whether this tab should transition to a new renderer for
  // cross-site URLs.  Enabled unless we see the --process-per-tab command line
  // switch.  Can be overridden in unit tests.
  bool ShouldTransitionCrossSite();

  // Returns true if for the navigation from |current_effective_url| to
  // |new_effective_url|, a new SiteInstance and BrowsingInstance should be
  // created (even if we are in a process model that doesn't usually swap).
  // This forces a process swap and severs script connections with existing
  // tabs.  Cases where this can happen include transitions between WebUI and
  // regular web pages. |new_site_instance| may be null.
  // If there is no current NavigationEntry, then |current_is_view_source_mode|
  // should be the same as |new_is_view_source_mode|.
  //
  // We use the effective URL here, since that's what is used in the
  // SiteInstance's site and when we later call IsSameWebSite.  If there is no
  // current NavigationEntry, check the current SiteInstance's site, which might
  // already be committed to a Web UI URL (such as the NTP).
  bool ShouldSwapBrowsingInstancesForNavigation(
      const GURL& current_effective_url,
      bool current_is_view_source_mode,
      SiteInstance* new_site_instance,
      const GURL& new_effective_url,
      bool new_is_view_source_mode) const;

  // Creates a new Web UI, ensuring that the bindings are appropriate compared
  // to |bindings|.
  scoped_ptr<WebUIImpl> CreateWebUI(const GURL& url, int bindings);

  // Returns true if it is safe to reuse the current WebUI when navigating from
  // |current_entry| to |new_url|.
  bool ShouldReuseWebUI(
      const NavigationEntry* current_entry,
      const GURL& new_url) const;

  // Returns the SiteInstance to use for the navigation.
  SiteInstance* GetSiteInstanceForNavigation(const GURL& dest_url,
                                             SiteInstance* source_instance,
                                             SiteInstance* dest_instance,
                                             ui::PageTransition transition,
                                             bool dest_is_restore,
                                             bool dest_is_view_source_mode);

  // Returns an appropriate SiteInstance object for the given |dest_url|,
  // possibly reusing the current SiteInstance.  If --process-per-tab is used,
  // this is only called when ShouldSwapBrowsingInstancesForNavigation returns
  // true. |source_instance| is the SiteInstance of the frame that initiated the
  // navigation. |current_instance| is the SiteInstance of the frame that is
  // currently navigating. |dest_instance|, is a predetermined SiteInstance
  // that'll be used if not null.
  // For example, if you have a parent frame A, which has a child frame B, and
  // A is trying to change the src attribute of B, this will cause a navigation
  // where the source SiteInstance is A and B is the current SiteInstance.
  // This is a helper function for GetSiteInstanceForNavigation.
  SiteInstance* GetSiteInstanceForURL(const GURL& dest_url,
                                      SiteInstance* source_instance,
                                      SiteInstance* current_instance,
                                      SiteInstance* dest_instance,
                                      ui::PageTransition transition,
                                      bool dest_is_restore,
                                      bool dest_is_view_source_mode,
                                      bool force_browsing_instance_swap);

  // Determines the appropriate url to use as the current url for SiteInstance
  // selection.
  const GURL& GetCurrentURLForSiteInstance(
      SiteInstance* current_instance,
      NavigationEntry* current_entry);

  // Creates a new RenderFrameHostImpl for the |new_instance| and assign it to
  // |pending_render_frame_host_| while respecting the opener route if needed
  // and stores it in pending_render_frame_host_.
  void CreatePendingRenderFrameHost(SiteInstance* old_instance,
                                    SiteInstance* new_instance,
                                    bool is_main_frame);

  // Ensure that we have created RFHs for the new RFH's opener chain if
  // we are staying in the same BrowsingInstance. This allows the new RFH
  // to send cross-process script calls to its opener(s). Returns the opener
  // route ID to be used for the new RenderView to be created.
  // |create_render_frame_flags| allows the method to set additional flags.
  int CreateOpenerRenderViewsIfNeeded(SiteInstance* old_instance,
                                      SiteInstance* new_instance,
                                      int* create_render_frame_flags);

  // Creates a RenderFrameHost and corresponding RenderViewHost if necessary.
  scoped_ptr<RenderFrameHostImpl> CreateRenderFrameHost(SiteInstance* instance,
                                                        int view_routing_id,
                                                        int frame_routing_id,
                                                        int flags);

  // PlzNavigate
  // Creates and initializes a speculative RenderFrameHost and/or WebUI for an
  // ongoing navigation. They might be destroyed and re-created later if the
  // navigation is redirected to a different SiteInstance.
  bool CreateSpeculativeRenderFrameHost(const GURL& url,
                                        SiteInstance* old_instance,
                                        SiteInstance* new_instance,
                                        int bindings);

  // Sets up the necessary state for a new RenderViewHost with the given opener,
  // if necessary.  It creates a RenderFrameProxy in the target renderer process
  // with the given |proxy_routing_id|, which is used to route IPC messages when
  // in swapped out state.  Returns early if the RenderViewHost has already been
  // initialized for another RenderFrameHost.
  // TODO(creis): opener_route_id is currently for the RenderViewHost but should
  // be for the RenderFrame, since frames can have openers.
  bool InitRenderView(RenderViewHostImpl* render_view_host,
                      int opener_route_id,
                      int proxy_routing_id,
                      bool for_main_frame_navigation);

  // Initialization for RenderFrameHost uses the same sequence as InitRenderView
  // above.
  bool InitRenderFrame(RenderFrameHostImpl* render_frame_host);

  // Sets the pending RenderFrameHost/WebUI to be the active one. Note that this
  // doesn't require the pending render_frame_host_ pointer to be non-NULL,
  // since there could be Web UI switching as well. Call this for every commit.
  // If PlzNavigate is enabled the method will set the speculative (not pending)
  // RenderFrameHost to be the active one.
  void CommitPending();

  // Helper to call CommitPending() in all necessary cases.
  void CommitPendingIfNecessary(RenderFrameHostImpl* render_frame_host,
                                bool was_caused_by_user_gesture);

  // Commits any pending sandbox flag updates when the renderer's frame
  // navigates.
  void CommitPendingSandboxFlags();

  // Runs the unload handler in the old RenderFrameHost, after the new
  // RenderFrameHost has committed.  |old_render_frame_host| will either be
  // deleted or put on the pending delete list during this call.
  void SwapOutOldFrame(scoped_ptr<RenderFrameHostImpl> old_render_frame_host);

  // Discards a RenderFrameHost that was never made active (for active ones
  // SwapOutOldFrame is used instead).
  void DiscardUnusedFrame(scoped_ptr<RenderFrameHostImpl> render_frame_host);

  // Holds |render_frame_host| until it can be deleted when its swap out ACK
  // arrives.
  void MoveToPendingDeleteHosts(
      scoped_ptr<RenderFrameHostImpl> render_frame_host);

  // Shutdown all RenderFrameProxyHosts in a SiteInstance. This is called to
  // shutdown frames when all the frames in a SiteInstance are confirmed to be
  // swapped out.
  void ShutdownRenderFrameProxyHostsInSiteInstance(int32 site_instance_id);

  // Helper method to terminate the pending RenderFrameHost. The frame may be
  // deleted immediately, or it may be kept around in hopes of later reuse.
  void CancelPending();

  // Clears pending_render_frame_host_, returning it to the caller for disposal.
  scoped_ptr<RenderFrameHostImpl> UnsetPendingRenderFrameHost();

  // Helper method to set the active RenderFrameHost. Returns the old
  // RenderFrameHost and updates counts.
  scoped_ptr<RenderFrameHostImpl> SetRenderFrameHost(
      scoped_ptr<RenderFrameHostImpl> render_frame_host);

  RenderFrameHostImpl* UpdateStateForNavigate(
      const GURL& dest_url,
      SiteInstance* source_instance,
      SiteInstance* dest_instance,
      ui::PageTransition transition,
      bool dest_is_restore,
      bool dest_is_view_source_mode,
      const GlobalRequestID& transferred_request_id,
      int bindings);

  // Called when a renderer process is starting to close.  We should not
  // schedule new navigations in its swapped out RenderFrameHosts after this.
  void RendererProcessClosing(RenderProcessHost* render_process_host);

  // Helper method to delete a RenderFrameProxyHost from the list, if one exists
  // for the given |instance|.
  void DeleteRenderFrameProxyHost(SiteInstance* instance);

  // For use in creating RenderFrameHosts.
  FrameTreeNode* frame_tree_node_;

  // Our delegate, not owned by us. Guaranteed non-NULL.
  Delegate* delegate_;

  // Whether a navigation requiring different RenderFrameHosts is pending. This
  // is either for cross-site requests or when required for the process type
  // (like WebUI).
  // PlzNavigate: |cross_navigation_pending_| is not used for browser-side
  // navigation.
  bool cross_navigation_pending_;

  // Implemented by the owner of this class.  These delegates are installed into
  // all the RenderFrameHosts that we create.
  RenderFrameHostDelegate* render_frame_delegate_;
  RenderViewHostDelegate* render_view_delegate_;
  RenderWidgetHostDelegate* render_widget_delegate_;

  // Our RenderFrameHost and its associated Web UI (if any, will be NULL for
  // non-WebUI pages). This object is responsible for all communication with
  // a child RenderFrame instance.
  // For now, RenderFrameHost keeps a RenderViewHost in its SiteInstance alive.
  // Eventually, RenderViewHost will be replaced with a page context.
  scoped_ptr<RenderFrameHostImpl> render_frame_host_;
  scoped_ptr<WebUIImpl> web_ui_;

  // A RenderFrameHost used to load a cross-site page. This remains hidden
  // while a cross-site request is pending until it calls DidNavigate. It may
  // have an associated Web UI, in which case the Web UI pointer will be non-
  // NULL.
  //
  // The |pending_web_ui_| may be non-NULL even when the
  // |pending_render_frame_host_| is NULL. This will happen when we're
  // transitioning between two Web UI pages: the RFH won't be swapped, so the
  // pending pointer will be unused, but there will be a pending Web UI
  // associated with the navigation.
  // Note: This is not used in PlzNavigate.
  scoped_ptr<RenderFrameHostImpl> pending_render_frame_host_;

  // If a pending request needs to be transferred to another process, this
  // owns the request until it's transferred to the new process, so it will be
  // cleaned up if the navigation is cancelled.  Otherwise, this is NULL.
  scoped_ptr<CrossSiteTransferringRequest> cross_site_transferring_request_;

  // Tracks information about any navigation paused after receiving response
  // headers.
  scoped_ptr<GlobalRequestID> response_started_id_;

  // If either of these is non-NULL, the pending navigation is to a chrome:
  // page. The scoped_ptr is used if pending_web_ui_ != web_ui_, the WeakPtr is
  // used for when they reference the same object. If either is non-NULL, the
  // other should be NULL.
  // Note: These are not used in PlzNavigate.
  scoped_ptr<WebUIImpl> pending_web_ui_;
  base::WeakPtr<WebUIImpl> pending_and_current_web_ui_;

  // A map of site instance ID to RenderFrameProxyHosts.
  typedef base::hash_map<int32, RenderFrameProxyHost*> RenderFrameProxyHostMap;
  RenderFrameProxyHostMap proxy_hosts_;

  // A list of RenderFrameHosts waiting to shut down after swapping out.  We use
  // a linked list since we expect frequent deletes and no indexed access, and
  // because sets don't appear to support linked_ptrs.
  typedef std::list<linked_ptr<RenderFrameHostImpl> > RFHPendingDeleteList;
  RFHPendingDeleteList pending_delete_hosts_;

  // The intersitial page currently shown if any, not own by this class
  // (the InterstitialPage is self-owned, it deletes itself when hidden).
  InterstitialPageImpl* interstitial_page_;

  NotificationRegistrar registrar_;

  // PlzNavigate
  // These members store a speculative RenderFrameHost and WebUI. They are
  // created early in a navigation so a renderer process can be started in
  // parallel, if needed. This is purely a performance optimization and is not
  // required for correct behavior. The created RenderFrameHost might be
  // discarded later on if the final URL's SiteInstance isn't compatible with
  // what was used to create it.
  // Note: PlzNavigate only uses speculative RenderFrameHost and WebUI, not
  // the pending ones.
  scoped_ptr<RenderFrameHostImpl> speculative_render_frame_host_;
  scoped_ptr<WebUIImpl> speculative_web_ui_;

  // PlzNavigate
  // If true at navigation commit time the current WebUI will be kept instead of
  // creating a new one.
  bool should_reuse_web_ui_;

  base::WeakPtrFactory<RenderFrameHostManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RenderFrameHostManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_HOST_MANAGER_H_
