// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_HOST_MANAGER_H_
#define CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_HOST_MANAGER_H_

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

struct FrameHostMsg_BeginNavigation_Params;
struct FrameMsg_Navigate_Params;

namespace content {
class BrowserContext;
class CrossProcessFrameConnector;
class CrossSiteTransferringRequest;
class InterstitialPageImpl;
class FrameTreeNode;
class NavigationControllerImpl;
class NavigationEntry;
class NavigationEntryImpl;
class NavigationRequest;
class RenderFrameHost;
class RenderFrameHostDelegate;
class RenderFrameHost;
class RenderFrameHostImpl;
class RenderFrameHostManagerTest;
class RenderFrameProxyHost;
class RenderViewHost;
class RenderViewHostImpl;
class RenderWidgetHostDelegate;
class RenderWidgetHostView;
class TestWebContents;
class WebUIImpl;
struct NavigationBeforeCommitInfo;

// Manages RenderFrameHosts for a FrameTreeNode.  This class acts as a state
// machine to make cross-process navigations in a frame possible.
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
        int parent_routing_id) = 0;
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
    virtual WebUIImpl* CreateWebUIForRenderManager(const GURL& url) = 0;

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

    // Creates a view and sets the size for the specified RVH.
    virtual void CreateViewAndSetSizeForRVH(RenderViewHost* rvh) = 0;

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
  virtual ~RenderFrameHostManager();

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

  // Sets the pending Web UI for the pending navigation, ensuring that the
  // bindings are appropriate for the given NavigationEntry.
  void SetPendingWebUI(const NavigationEntryImpl& entry);

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
      PageTransition page_transition,
      bool should_replace_current_entry);

  // Received a response from CrossSiteResourceHandler. If the navigation
  // specifies a transition, this is called and the navigation will not resume
  // until ResumeResponseDeferredAtStart.
  void OnDeferredAfterResponseStarted(
      const GlobalRequestID& global_request_id,
      RenderFrameHostImpl* pending_render_frame_host);

  // Resume navigation paused after receiving response headers.
  void ResumeResponseDeferredAtStart();

  // Called when a renderer's frame navigates.
  void DidNavigateFrame(RenderFrameHostImpl* render_frame_host);

  // Called when a renderer sets its opener to null.
  void DidDisownOpener(RenderViewHost* render_view_host);

  // Helper method to create and initialize a RenderFrameHost.  If |swapped_out|
  // is true, it will be initially placed on the swapped out hosts list.
  // Returns the routing id of the *view* associated with the frame.
  int CreateRenderFrame(SiteInstance* instance,
                        int opener_route_id,
                        bool swapped_out,
                        bool for_main_frame_navigation,
                        bool hidden);

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
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

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

  // Deletes a RenderFrameHost that was pending shutdown.
  void ClearPendingShutdownRFHForSiteInstance(int32 site_instance_id,
                                              RenderFrameHostImpl* rfh);

  // Deletes any proxy hosts associated with this node. Used during destruction
  // of WebContentsImpl.
  void ResetProxyHosts();

  // Returns the routing id for a RenderFrameHost or RenderFrameHostProxy
  // that has the given SiteInstance and is associated with this
  // RenderFrameHostManager. Returns MSG_ROUTING_NONE if none is found.
  int GetRoutingIdForSiteInstance(SiteInstance* site_instance);

  // PlzNavigate: sends a RequestNavigation IPC to the renderer to ask it to
  // navigate. If no live renderer is present, then the navigation request will
  // be sent directly to the ResourceDispatcherHost.
  bool RequestNavigation(const NavigationEntryImpl& entry,
                         const FrameMsg_Navigate_Params& navigate_params);

  // PlzNavigate: Used to start a navigation. OnBeginNavigation is called
  // directly by RequestNavigation when there is no live renderer. Otherwise, it
  // is called following a BeginNavigation IPC from the renderer (which in
  // browser-initiated navigation also happens after RequestNavigation has been
  // called).
  void OnBeginNavigation(const FrameHostMsg_BeginNavigation_Params& params);

  // PlzNavigate: Called when a navigation request has received a response, to
  // select a renderer to use for the navigation.
  void CommitNavigation(const NavigationBeforeCommitInfo& info);

 private:
  friend class RenderFrameHostManagerTest;
  friend class TestWebContents;

  FRIEND_TEST_ALL_PREFIXES(CrossProcessFrameTreeBrowserTest,
                           CreateCrossProcessSubframeProxies);

  // Returns the current navigation request (used in the PlzNavigate navigation
  // logic refactoring project).
  NavigationRequest* navigation_request_for_testing() const {
    return navigation_request_.get(); }

  // Used with FrameTree::ForEach to erase RenderFrameProxyHosts from a
  // FrameTreeNode's RenderFrameHostManager.
  static bool ClearProxiesInSiteInstance(int32 site_instance_id,
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

  // Returns true if it is safe to reuse the current WebUI when navigating from
  // |current_entry| to |new_entry|.
  bool ShouldReuseWebUI(
      const NavigationEntry* current_entry,
      const NavigationEntryImpl* new_entry) const;

  // Returns the SiteInstance to use for the navigation.
  SiteInstance* GetSiteInstanceForNavigation(
      const GURL& dest_url,
      SiteInstance* dest_instance,
      PageTransition dest_transition,
      bool dest_is_restore,
      bool dest_is_view_source_mode);

  // Returns an appropriate SiteInstance object for the given |dest_url|,
  // possibly reusing the current SiteInstance.  If --process-per-tab is used,
  // this is only called when ShouldSwapBrowsingInstancesForNavigation returns
  // true. |dest_instance| will be used if it is not null.
  // This is a helper function for GetSiteInstanceForNavigation.
  SiteInstance* GetSiteInstanceForURL(
      const GURL& dest_url,
      SiteInstance* dest_instance,
      PageTransition dest_transition,
      bool dest_is_restore,
      bool dest_is_view_source_mode,
      SiteInstance* current_instance,
      bool force_browsing_instance_swap);

  // Creates a new RenderFrameHostImpl for the |new_instance| while respecting
  // the opener route if needed and stores it in pending_render_frame_host_.
  void CreateRenderFrameHostForNewSiteInstance(
      SiteInstance* old_instance,
      SiteInstance* new_instance,
      bool is_main_frame);

  // Creates a RenderFrameHost and corresponding RenderViewHost if necessary.
  scoped_ptr<RenderFrameHostImpl> CreateRenderFrameHost(SiteInstance* instance,
                                                        int view_routing_id,
                                                        int frame_routing_id,
                                                        bool swapped_out,
                                                        bool hidden);

  // Sets up the necessary state for a new RenderViewHost with the given opener,
  // if necessary.  It creates a RenderFrameProxy in the target renderer process
  // with the given |proxy_routing_id|, which is used to route IPC messages when
  // in swapped out state.  Returns early if the RenderViewHost has already been
  // initialized for another RenderFrameHost.
  // TODO(creis): opener_route_id is currently for the RenderViewHost but should
  // be for the RenderFrame, since frames can have openers.
  bool InitRenderView(RenderViewHost* render_view_host,
                      int opener_route_id,
                      int proxy_routing_id,
                      bool for_main_frame_navigation);

  // Initialization for RenderFrameHost uses the same sequence as InitRenderView
  // above.
  bool InitRenderFrame(RenderFrameHost* render_frame_host);

  // Sets the pending RenderFrameHost/WebUI to be the active one. Note that this
  // doesn't require the pending render_frame_host_ pointer to be non-NULL,
  // since there could be Web UI switching as well. Call this for every commit.
  void CommitPending();

  // Runs the unload handler in the current page, after the new page has
  // committed.
  void SwapOutOldPage(RenderFrameHostImpl* old_render_frame_host);

  // Shutdown all RenderFrameProxyHosts in a SiteInstance. This is called to
  // shutdown frames when all the frames in a SiteInstance are confirmed to be
  // swapped out.
  void ShutdownRenderFrameProxyHostsInSiteInstance(int32 site_instance_id);

  // Helper method to terminate the pending RenderViewHost.
  void CancelPending();

  // Helper method to set the active RenderFrameHost. Returns the old
  // RenderFrameHost and updates counts.
  scoped_ptr<RenderFrameHostImpl> SetRenderFrameHost(
      scoped_ptr<RenderFrameHostImpl> render_frame_host);

  RenderFrameHostImpl* UpdateStateForNavigate(
      const NavigationEntryImpl& entry);

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
  scoped_ptr<WebUIImpl> pending_web_ui_;
  base::WeakPtr<WebUIImpl> pending_and_current_web_ui_;

  // A map of site instance ID to RenderFrameProxyHosts.
  typedef base::hash_map<int32, RenderFrameProxyHost*> RenderFrameProxyHostMap;
  RenderFrameProxyHostMap proxy_hosts_;

  // A map of RenderFrameHosts pending shutdown.
  typedef base::hash_map<int32, linked_ptr<RenderFrameHostImpl> >
      RFHPendingDeleteMap;
  RFHPendingDeleteMap pending_delete_hosts_;

  // The intersitial page currently shown if any, not own by this class
  // (the InterstitialPage is self-owned, it deletes itself when hidden).
  InterstitialPageImpl* interstitial_page_;

  NotificationRegistrar registrar_;

  // PlzNavigate: Owns a navigation request that originated in that frame until
  // it commits.
  scoped_ptr<NavigationRequest> navigation_request_;

  base::WeakPtrFactory<RenderFrameHostManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RenderFrameHostManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_HOST_MANAGER_H_
