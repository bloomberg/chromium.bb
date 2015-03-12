// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/navigator_impl.h"

#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/time/time.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/navigation_controller_impl.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/frame_host/navigation_request.h"
#include "content/browser/frame_host/navigation_request_info.h"
#include "content/browser/frame_host/navigator_delegate.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/webui/web_ui_controller_factory_registry.h"
#include "content/browser/webui/web_ui_impl.h"
#include "content/common/frame_messages.h"
#include "content/common/navigation_params.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/stream_handle.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/resource_response.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"

namespace content {

namespace {

FrameMsg_Navigate_Type::Value GetNavigationType(
    BrowserContext* browser_context, const NavigationEntryImpl& entry,
    NavigationController::ReloadType reload_type) {
  switch (reload_type) {
    case NavigationControllerImpl::RELOAD:
      return FrameMsg_Navigate_Type::RELOAD;
    case NavigationControllerImpl::RELOAD_IGNORING_CACHE:
      return FrameMsg_Navigate_Type::RELOAD_IGNORING_CACHE;
    case NavigationControllerImpl::RELOAD_ORIGINAL_REQUEST_URL:
      return FrameMsg_Navigate_Type::RELOAD_ORIGINAL_REQUEST_URL;
    case NavigationControllerImpl::NO_RELOAD:
      break;  // Fall through to rest of function.
  }

  // |RenderViewImpl::PopulateStateFromPendingNavigationParams| differentiates
  // between |RESTORE_WITH_POST| and |RESTORE|.
  if (entry.restore_type() ==
      NavigationEntryImpl::RESTORE_LAST_SESSION_EXITED_CLEANLY) {
    if (entry.GetHasPostData())
      return FrameMsg_Navigate_Type::RESTORE_WITH_POST;
    return FrameMsg_Navigate_Type::RESTORE;
  }

  return FrameMsg_Navigate_Type::NORMAL;
}

RenderFrameHostManager* GetRenderManager(RenderFrameHostImpl* rfh) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSitePerProcess))
    return rfh->frame_tree_node()->render_manager();

  return rfh->frame_tree_node()->frame_tree()->root()->render_manager();
}

}  // namespace

struct NavigatorImpl::NavigationMetricsData {
  NavigationMetricsData(base::TimeTicks start_time,
                        GURL url,
                        NavigationEntryImpl::RestoreType restore_type)
      : start_time_(start_time), url_(url) {
    is_restoring_from_last_session_ =
        (restore_type ==
             NavigationEntryImpl::RESTORE_LAST_SESSION_EXITED_CLEANLY ||
         restore_type == NavigationEntryImpl::RESTORE_LAST_SESSION_CRASHED);
  }

  base::TimeTicks start_time_;
  GURL url_;
  bool is_restoring_from_last_session_;
  base::TimeTicks url_job_start_time_;
  base::TimeDelta before_unload_delay_;
};

NavigatorImpl::NavigatorImpl(
    NavigationControllerImpl* navigation_controller,
    NavigatorDelegate* delegate)
    : controller_(navigation_controller),
      delegate_(delegate) {
}

NavigatorImpl::~NavigatorImpl() {}

NavigationController* NavigatorImpl::GetController() {
  return controller_;
}

void NavigatorImpl::DidStartProvisionalLoad(
    RenderFrameHostImpl* render_frame_host,
    const GURL& url,
    bool is_transition_navigation) {
  bool is_error_page = (url.spec() == kUnreachableWebDataURL);
  bool is_iframe_srcdoc = (url.spec() == kAboutSrcDocURL);
  GURL validated_url(url);
  RenderProcessHost* render_process_host = render_frame_host->GetProcess();
  render_process_host->FilterURL(false, &validated_url);

  bool is_main_frame = render_frame_host->frame_tree_node()->IsMainFrame();
  NavigationEntryImpl* pending_entry = controller_->GetPendingEntry();
  if (is_main_frame) {
    // If there is no browser-initiated pending entry for this navigation and it
    // is not for the error URL, create a pending entry using the current
    // SiteInstance, and ensure the address bar updates accordingly.  We don't
    // know the referrer or extra headers at this point, but the referrer will
    // be set properly upon commit.
    bool has_browser_initiated_pending_entry = pending_entry &&
        !pending_entry->is_renderer_initiated();
    if (!has_browser_initiated_pending_entry && !is_error_page) {
      NavigationEntryImpl* entry = NavigationEntryImpl::FromNavigationEntry(
          controller_->CreateNavigationEntry(validated_url,
                                             content::Referrer(),
                                             ui::PAGE_TRANSITION_LINK,
                                             true /* is_renderer_initiated */,
                                             std::string(),
                                             controller_->GetBrowserContext()));
      entry->set_site_instance(render_frame_host->GetSiteInstance());
      // TODO(creis): If there's a pending entry already, find a safe way to
      // update it instead of replacing it and copying over things like this.
      if (pending_entry) {
        entry->set_transferred_global_request_id(
            pending_entry->transferred_global_request_id());
        entry->set_should_replace_entry(pending_entry->should_replace_entry());
        entry->SetRedirectChain(pending_entry->GetRedirectChain());
      }
      controller_->SetPendingEntry(entry);
      if (delegate_)
        delegate_->NotifyChangedNavigationState(content::INVALIDATE_TYPE_URL);
    }

    if (delegate_ && is_transition_navigation)
      delegate_->DidStartNavigationTransition(render_frame_host);
  }

  if (delegate_) {
    // Notify the observer about the start of the provisional load.
    delegate_->DidStartProvisionalLoad(
        render_frame_host, validated_url, is_error_page, is_iframe_srcdoc);
  }
}


void NavigatorImpl::DidFailProvisionalLoadWithError(
    RenderFrameHostImpl* render_frame_host,
    const FrameHostMsg_DidFailProvisionalLoadWithError_Params& params) {
  VLOG(1) << "Failed Provisional Load: " << params.url.possibly_invalid_spec()
          << ", error_code: " << params.error_code
          << ", error_description: " << params.error_description
          << ", showing_repost_interstitial: " <<
            params.showing_repost_interstitial
          << ", frame_id: " << render_frame_host->GetRoutingID();
  GURL validated_url(params.url);
  RenderProcessHost* render_process_host = render_frame_host->GetProcess();
  render_process_host->FilterURL(false, &validated_url);

  if (net::ERR_ABORTED == params.error_code) {
    // EVIL HACK ALERT! Ignore failed loads when we're showing interstitials.
    // This means that the interstitial won't be torn down properly, which is
    // bad. But if we have an interstitial, go back to another tab type, and
    // then load the same interstitial again, we could end up getting the first
    // interstitial's "failed" message (as a result of the cancel) when we're on
    // the second one. We can't tell this apart, so we think we're tearing down
    // the current page which will cause a crash later on.
    //
    // http://code.google.com/p/chromium/issues/detail?id=2855
    // Because this will not tear down the interstitial properly, if "back" is
    // back to another tab type, the interstitial will still be somewhat alive
    // in the previous tab type. If you navigate somewhere that activates the
    // tab with the interstitial again, you'll see a flash before the new load
    // commits of the interstitial page.
    FrameTreeNode* root =
        render_frame_host->frame_tree_node()->frame_tree()->root();
    if (root->render_manager()->interstitial_page() != NULL) {
      LOG(WARNING) << "Discarding message during interstitial.";
      return;
    }

    // We used to cancel the pending renderer here for cross-site downloads.
    // However, it's not safe to do that because the download logic repeatedly
    // looks for this WebContents based on a render ID.  Instead, we just
    // leave the pending renderer around until the next navigation event
    // (Navigate, DidNavigate, etc), which will clean it up properly.
    //
    // TODO(creis): Find a way to cancel any pending RFH here.
  }

  // We usually clear the pending entry when it fails, so that an arbitrary URL
  // isn't left visible above a committed page.  This must be enforced when
  // the pending entry isn't visible (e.g., renderer-initiated navigations) to
  // prevent URL spoofs for in-page navigations that don't go through
  // DidStartProvisionalLoadForFrame.
  //
  // However, we do preserve the pending entry in some cases, such as on the
  // initial navigation of an unmodified blank tab.  We also allow the delegate
  // to say when it's safe to leave aborted URLs in the omnibox, to let the user
  // edit the URL and try again.  This may be useful in cases that the committed
  // page cannot be attacker-controlled.  In these cases, we still allow the
  // view to clear the pending entry and typed URL if the user requests
  // (e.g., hitting Escape with focus in the address bar).
  //
  // Note: don't touch the transient entry, since an interstitial may exist.
  bool should_preserve_entry = controller_->IsUnmodifiedBlankTab() ||
      delegate_->ShouldPreserveAbortedURLs();
  if (controller_->GetPendingEntry() != controller_->GetVisibleEntry() ||
      !should_preserve_entry) {
    controller_->DiscardPendingEntry();

    // Also force the UI to refresh.
    controller_->delegate()->NotifyNavigationStateChanged(INVALIDATE_TYPE_URL);
  }

  if (delegate_)
    delegate_->DidFailProvisionalLoadWithError(render_frame_host, params);
}

void NavigatorImpl::DidFailLoadWithError(
    RenderFrameHostImpl* render_frame_host,
    const GURL& url,
    int error_code,
    const base::string16& error_description) {
  if (delegate_) {
    delegate_->DidFailLoadWithError(
        render_frame_host, url, error_code,
        error_description);
  }
}

bool NavigatorImpl::NavigateToEntry(
    FrameTreeNode* frame_tree_node,
    const NavigationEntryImpl& entry,
    NavigationController::ReloadType reload_type) {
  TRACE_EVENT0("browser,navigation", "NavigatorImpl::NavigateToEntry");

  // The renderer will reject IPC messages with URLs longer than
  // this limit, so don't attempt to navigate with a longer URL.
  if (entry.GetURL().spec().size() > GetMaxURLChars()) {
    LOG(WARNING) << "Refusing to load URL as it exceeds " << GetMaxURLChars()
                 << " characters.";
    return false;
  }

  // This will be used to set the Navigation Timing API navigationStart
  // parameter for browser navigations in new tabs (intents, tabs opened through
  // "Open link in new tab"). We need to keep it above RFHM::Navigate() call to
  // capture the time needed for the RenderFrameHost initialization.
  base::TimeTicks navigation_start = base::TimeTicks::Now();

  RenderFrameHostManager* manager = frame_tree_node->render_manager();

  // PlzNavigate: the RenderFrameHosts are no longer asked to navigate.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableBrowserSideNavigation)) {
    navigation_data_.reset(new NavigationMetricsData(
        navigation_start, entry.GetURL(), entry.restore_type()));
    RequestNavigation(frame_tree_node, entry, reload_type, navigation_start);
    return true;
  }

  RenderFrameHostImpl* dest_render_frame_host = manager->Navigate(entry);
  if (!dest_render_frame_host)
    return false;  // Unable to create the desired RenderFrameHost.

  // Make sure no code called via RFHM::Navigate clears the pending entry.
  CHECK_EQ(controller_->GetPendingEntry(), &entry);

  // For security, we should never send non-Web-UI URLs to a Web UI renderer.
  // Double check that here.
  CheckWebUIRendererDoesNotDisplayNormalURL(
      dest_render_frame_host, entry.GetURL());

  // Notify observers that we will navigate in this RenderFrame.
  if (delegate_) {
    delegate_->AboutToNavigateRenderFrame(frame_tree_node->current_frame_host(),
                                          dest_render_frame_host);
  }

  // Navigate in the desired RenderFrameHost.
  // We can skip this step in the rare case that this is a transfer navigation
  // which began in the chosen RenderFrameHost, since the request has already
  // been issued.  In that case, simply resume the response.
  bool is_transfer_to_same =
      entry.transferred_global_request_id().child_id != -1 &&
      entry.transferred_global_request_id().child_id ==
          dest_render_frame_host->GetProcess()->GetID();
  if (!is_transfer_to_same) {
    navigation_data_.reset(new NavigationMetricsData(
        navigation_start, entry.GetURL(), entry.restore_type()));
    // Create the navigation parameters.
    // TODO(vitalybuka): Move this before AboutToNavigateRenderFrame once
    // http://crbug.com/408684 is fixed.
    FrameMsg_Navigate_Type::Value navigation_type =
        GetNavigationType(controller_->GetBrowserContext(), entry, reload_type);
    dest_render_frame_host->Navigate(
        entry.ConstructCommonNavigationParams(navigation_type),
        entry.ConstructStartNavigationParams(),
        entry.ConstructCommitNavigationParams(navigation_start),
        entry.ConstructHistoryNavigationParams(controller_));
  } else {
    // No need to navigate again.  Just resume the deferred request.
    dest_render_frame_host->GetProcess()->ResumeDeferredNavigation(
        entry.transferred_global_request_id());
  }

  // Make sure no code called via RFH::Navigate clears the pending entry.
  CHECK_EQ(controller_->GetPendingEntry(), &entry);

  if (entry.GetPageID() == -1) {
    // HACK!!  This code suppresses javascript: URLs from being added to
    // session history, which is what we want to do for javascript: URLs that
    // do not generate content.  What we really need is a message from the
    // renderer telling us that a new page was not created.  The same message
    // could be used for mailto: URLs and the like.
    if (entry.GetURL().SchemeIs(url::kJavaScriptScheme))
      return false;
  }

  // Notify observers about navigation.
  if (delegate_)
    delegate_->DidStartNavigationToPendingEntry(entry.GetURL(), reload_type);

  return true;
}

bool NavigatorImpl::NavigateToPendingEntry(
    FrameTreeNode* frame_tree_node,
    NavigationController::ReloadType reload_type) {
  return NavigateToEntry(frame_tree_node, *controller_->GetPendingEntry(),
                         reload_type);
}

void NavigatorImpl::DidNavigate(
    RenderFrameHostImpl* render_frame_host,
    const FrameHostMsg_DidCommitProvisionalLoad_Params& input_params) {
  // PlzNavigate
  // The navigation request has been committed so the browser process doesn't
  // need to care about it anymore.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableBrowserSideNavigation)) {
    navigation_request_map_.erase(
        render_frame_host->frame_tree_node()->frame_tree_node_id());
  }

  FrameHostMsg_DidCommitProvisionalLoad_Params params(input_params);
  FrameTree* frame_tree = render_frame_host->frame_tree_node()->frame_tree();
  bool use_site_per_process = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kSitePerProcess);

  if (ui::PageTransitionIsMainFrame(params.transition)) {
    if (delegate_) {
      // When overscroll navigation gesture is enabled, a screenshot of the page
      // in its current state is taken so that it can be used during the
      // nav-gesture. It is necessary to take the screenshot here, before
      // calling RenderFrameHostManager::DidNavigateMainFrame, because that can
      // change WebContents::GetRenderViewHost to return the new host, instead
      // of the one that may have just been swapped out.
      if (delegate_->CanOverscrollContent()) {
        // Don't take screenshots if we are staying on the same page. We want
        // in-page navigations to be super fast, and taking a screenshot
        // currently blocks GPU for a longer time than we are willing to
        // tolerate in this use case.
        if (!params.was_within_same_page)
          controller_->TakeScreenshot();
      }

      // Run tasks that must execute just before the commit.
      bool is_navigation_within_page = controller_->IsURLInPageNavigation(
          params.url, params.was_within_same_page, render_frame_host);
      delegate_->DidNavigateMainFramePreCommit(is_navigation_within_page);
    }

    if (!use_site_per_process)
      frame_tree->root()->render_manager()->DidNavigateFrame(
          render_frame_host, params.gesture == NavigationGestureUser);
  }

  // Save the origin of the new page.  Do this before calling
  // DidNavigateFrame(), because the origin needs to be included in the SwapOut
  // message, which is sent inside DidNavigateFrame().  SwapOut needs the
  // origin because it creates a RenderFrameProxy that needs this to initialize
  // its security context. This origin will also be sent to RenderFrameProxies
  // created via ViewMsg_New and FrameMsg_NewFrameProxy.
  render_frame_host->frame_tree_node()->set_current_origin(params.origin);

  // When using --site-per-process, we notify the RFHM for all navigations,
  // not just main frame navigations.
  if (use_site_per_process) {
    FrameTreeNode* frame = render_frame_host->frame_tree_node();
    frame->render_manager()->DidNavigateFrame(
        render_frame_host, params.gesture == NavigationGestureUser);
  }

  // Update the site of the SiteInstance if it doesn't have one yet, unless
  // assigning a site is not necessary for this URL.  In that case, the
  // SiteInstance can still be considered unused until a navigation to a real
  // page.
  SiteInstanceImpl* site_instance = render_frame_host->GetSiteInstance();
  if (!site_instance->HasSite() &&
      ShouldAssignSiteForURL(params.url)) {
    site_instance->SetSite(params.url);
  }

  // Need to update MIME type here because it's referred to in
  // UpdateNavigationCommands() called by RendererDidNavigate() to
  // determine whether or not to enable the encoding menu.
  // It's updated only for the main frame. For a subframe,
  // RenderView::UpdateURL does not set params.contents_mime_type.
  // (see http://code.google.com/p/chromium/issues/detail?id=2929 )
  // TODO(jungshik): Add a test for the encoding menu to avoid
  // regressing it again.
  // TODO(nasko): Verify the correctness of the above comment, since some of the
  // code doesn't exist anymore. Also, move this code in the
  // PageTransitionIsMainFrame code block above.
  if (ui::PageTransitionIsMainFrame(params.transition) && delegate_)
    delegate_->SetMainFrameMimeType(params.contents_mime_type);

  LoadCommittedDetails details;
  bool did_navigate = controller_->RendererDidNavigate(render_frame_host,
                                                       params, &details);

  // For now, keep track of each frame's URL in its FrameTreeNode.  This lets
  // us estimate our process count for implementing OOP iframes.
  // TODO(creis): Remove this when we track which pages commit in each frame.
  render_frame_host->frame_tree_node()->set_current_url(params.url);

  // Send notification about committed provisional loads. This notification is
  // different from the NAV_ENTRY_COMMITTED notification which doesn't include
  // the actual URL navigated to and isn't sent for AUTO_SUBFRAME navigations.
  if (details.type != NAVIGATION_TYPE_NAV_IGNORE && delegate_) {
    DCHECK_EQ(!render_frame_host->GetParent(),
              did_navigate ? details.is_main_frame : false);
    ui::PageTransition transition_type = params.transition;
    // Whether or not a page transition was triggered by going backward or
    // forward in the history is only stored in the navigation controller's
    // entry list.
    if (did_navigate &&
        (controller_->GetLastCommittedEntry()->GetTransitionType() &
            ui::PAGE_TRANSITION_FORWARD_BACK)) {
      transition_type = ui::PageTransitionFromInt(
          params.transition | ui::PAGE_TRANSITION_FORWARD_BACK);
    }

    delegate_->DidCommitProvisionalLoad(render_frame_host,
                                        params.url,
                                        transition_type);
  }

  if (!did_navigate)
    return;  // No navigation happened.

  // DO NOT ADD MORE STUFF TO THIS FUNCTION! Your component should either listen
  // for the appropriate notification (best) or you can add it to
  // DidNavigateMainFramePostCommit / DidNavigateAnyFramePostCommit (only if
  // necessary, please).

  // TODO(carlosk): Move this out when PlzNavigate implementation properly calls
  // the observer methods.
  RecordNavigationMetrics(details, params, site_instance);

  // Run post-commit tasks.
  if (delegate_) {
    if (details.is_main_frame) {
      delegate_->DidNavigateMainFramePostCommit(render_frame_host,
                                                details, params);
    }

    delegate_->DidNavigateAnyFramePostCommit(
        render_frame_host, details, params);
  }
}

bool NavigatorImpl::ShouldAssignSiteForURL(const GURL& url) {
  // about:blank should not "use up" a new SiteInstance.  The SiteInstance can
  // still be used for a normal web site.
  if (url == GURL(url::kAboutBlankURL))
    return false;

  // The embedder will then have the opportunity to determine if the URL
  // should "use up" the SiteInstance.
  return GetContentClient()->browser()->ShouldAssignSiteForURL(url);
}

void NavigatorImpl::RequestOpenURL(RenderFrameHostImpl* render_frame_host,
                                   const GURL& url,
                                   SiteInstance* source_site_instance,
                                   const Referrer& referrer,
                                   WindowOpenDisposition disposition,
                                   bool should_replace_current_entry,
                                   bool user_gesture) {
  SiteInstance* current_site_instance =
      GetRenderManager(render_frame_host)->current_frame_host()->
          GetSiteInstance();
  // If this came from a swapped out RenderFrameHost, we only allow the request
  // if we are still in the same BrowsingInstance.
  // TODO(creis): Move this to RenderFrameProxyHost::OpenURL.
  if (render_frame_host->is_swapped_out() &&
      !render_frame_host->GetSiteInstance()->IsRelatedSiteInstance(
          current_site_instance)) {
    return;
  }

  // Delegate to RequestTransferURL because this is just the generic
  // case where |old_request_id| is empty.
  // TODO(creis): Pass the redirect_chain into this method to support client
  // redirects.  http://crbug.com/311721.
  std::vector<GURL> redirect_chain;
  RequestTransferURL(render_frame_host, url, source_site_instance,
                     redirect_chain, referrer, ui::PAGE_TRANSITION_LINK,
                     disposition, GlobalRequestID(),
                     should_replace_current_entry, user_gesture);
}

void NavigatorImpl::RequestTransferURL(
    RenderFrameHostImpl* render_frame_host,
    const GURL& url,
    SiteInstance* source_site_instance,
    const std::vector<GURL>& redirect_chain,
    const Referrer& referrer,
    ui::PageTransition page_transition,
    WindowOpenDisposition disposition,
    const GlobalRequestID& transferred_global_request_id,
    bool should_replace_current_entry,
    bool user_gesture) {
  GURL dest_url(url);
  SiteInstance* current_site_instance =
      GetRenderManager(render_frame_host)->current_frame_host()->
          GetSiteInstance();
  if (!GetContentClient()->browser()->ShouldAllowOpenURL(
          current_site_instance, url)) {
    dest_url = GURL(url::kAboutBlankURL);
  }

  int64 frame_tree_node_id = -1;

  // Send the navigation to the current FrameTreeNode if it's destined for a
  // subframe in the current tab.  We'll assume it's for the main frame
  // (possibly of a new or different WebContents) otherwise.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSitePerProcess) &&
      disposition == CURRENT_TAB &&
      render_frame_host->GetParent()) {
    frame_tree_node_id =
        render_frame_host->frame_tree_node()->frame_tree_node_id();
  }

  OpenURLParams params(
      dest_url, referrer, frame_tree_node_id, disposition, page_transition,
      true /* is_renderer_initiated */);
  params.source_site_instance = source_site_instance;
  if (redirect_chain.size() > 0)
    params.redirect_chain = redirect_chain;
  params.transferred_global_request_id = transferred_global_request_id;
  params.should_replace_current_entry = should_replace_current_entry;
  params.user_gesture = user_gesture;

  if (GetRenderManager(render_frame_host)->web_ui()) {
    // Web UI pages sometimes want to override the page transition type for
    // link clicks (e.g., so the new tab page can specify AUTO_BOOKMARK for
    // automatically generated suggestions).  We don't override other types
    // like TYPED because they have different implications (e.g., autocomplete).
    if (ui::PageTransitionCoreTypeIs(
        params.transition, ui::PAGE_TRANSITION_LINK))
      params.transition =
          GetRenderManager(render_frame_host)->web_ui()->
              GetLinkTransitionType();

    // Note also that we hide the referrer for Web UI pages. We don't really
    // want web sites to see a referrer of "chrome://blah" (and some
    // chrome: URLs might have search terms or other stuff we don't want to
    // send to the site), so we send no referrer.
    params.referrer = Referrer();

    // Navigations in Web UI pages count as browser-initiated navigations.
    params.is_renderer_initiated = false;
  }

  if (delegate_)
    delegate_->RequestOpenURL(render_frame_host, params);
}

// PlzNavigate
void NavigatorImpl::OnBeforeUnloadACK(FrameTreeNode* frame_tree_node,
                                      bool proceed) {
  CHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableBrowserSideNavigation));
  DCHECK(frame_tree_node);

  NavigationRequest* navigation_request =
      navigation_request_map_.get(frame_tree_node->frame_tree_node_id());

  // The NavigationRequest may have been canceled while the renderer was
  // executing the BeforeUnload event.
  if (!navigation_request)
    return;

  DCHECK_EQ(NavigationRequest::WAITING_FOR_RENDERER_RESPONSE,
            navigation_request->state());

  if (proceed)
    BeginNavigation(frame_tree_node);
  else
    CancelNavigation(frame_tree_node);
}

// PlzNavigate
void NavigatorImpl::OnBeginNavigation(
    FrameTreeNode* frame_tree_node,
    const CommonNavigationParams& common_params,
    const BeginNavigationParams& begin_params,
    scoped_refptr<ResourceRequestBody> body) {
  // This is a renderer-initiated navigation.
  CHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableBrowserSideNavigation));
  DCHECK(frame_tree_node);

  NavigationRequest* ongoing_navigation_request =
      navigation_request_map_.get(frame_tree_node->frame_tree_node_id());

  // The renderer-initiated navigation request is ignored iff a) there is an
  // ongoing request b) which is browser or user-initiated and c) the renderer
  // request is not user-initiated.
  if (ongoing_navigation_request &&
      (ongoing_navigation_request->browser_initiated() ||
       ongoing_navigation_request->begin_params().has_user_gesture) &&
      !begin_params.has_user_gesture) {
    return;
  }

  // In all other cases the current navigation, if any, is canceled and a new
  // NavigationRequest is created and stored in the map. Actual cancellation
  // happens when the existing request map entry is replaced and destroyed.
  scoped_ptr<NavigationRequest> navigation_request =
      NavigationRequest::CreateRendererInitiated(
          frame_tree_node, common_params, begin_params, body,
          controller_->GetLastCommittedEntryIndex(),
          controller_->GetEntryCount());
  navigation_request_map_.set(
      frame_tree_node->frame_tree_node_id(), navigation_request.Pass());

  if (frame_tree_node->IsMainFrame())
    navigation_data_.reset();

  BeginNavigation(frame_tree_node);
}

// PlzNavigate
void NavigatorImpl::CommitNavigation(FrameTreeNode* frame_tree_node,
                                     ResourceResponse* response,
                                     scoped_ptr<StreamHandle> body) {
  CHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableBrowserSideNavigation));

  NavigationRequest* navigation_request =
      navigation_request_map_.get(frame_tree_node->frame_tree_node_id());
  DCHECK(navigation_request);
  DCHECK(response ||
         !NavigationRequest::ShouldMakeNetworkRequest(
             navigation_request->common_params().url));

  // HTTP 204 (No Content) and HTTP 205 (Reset Content) responses should not
  // commit; they leave the frame showing the previous page.
  if (response && response->head.headers.get() &&
      (response->head.headers->response_code() == 204 ||
       response->head.headers->response_code() == 205)) {
    CancelNavigation(frame_tree_node);
    return;
  }

  // Select an appropriate renderer to commit the navigation.
  RenderFrameHostImpl* render_frame_host =
      frame_tree_node->render_manager()->GetFrameHostForNavigation(
          *navigation_request);
  CheckWebUIRendererDoesNotDisplayNormalURL(
      render_frame_host, navigation_request->common_params().url);

  render_frame_host->CommitNavigation(response, body.Pass(),
                                      navigation_request->common_params(),
                                      navigation_request->commit_params(),
                                      navigation_request->history_params());
}

// PlzNavigate
void NavigatorImpl::CancelNavigation(FrameTreeNode* frame_tree_node) {
  CHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableBrowserSideNavigation));
  navigation_request_map_.erase(frame_tree_node->frame_tree_node_id());
  if (frame_tree_node->IsMainFrame())
    navigation_data_.reset();
  // TODO(carlosk): move this cleanup into the NavigationRequest destructor once
  // we properly cancel ongoing navigations.
  frame_tree_node->render_manager()->CleanUpNavigation();
}

// PlzNavigate
NavigationRequest* NavigatorImpl::GetNavigationRequestForNodeForTesting(
    FrameTreeNode* frame_tree_node) {
  return navigation_request_map_.get(frame_tree_node->frame_tree_node_id());
}

bool NavigatorImpl::IsWaitingForBeforeUnloadACK(
    FrameTreeNode* frame_tree_node) {
  CHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableBrowserSideNavigation));
  NavigationRequest* request =
      navigation_request_map_.get(frame_tree_node->frame_tree_node_id());
  if (!request)
    return false;
  return request->state() == NavigationRequest::WAITING_FOR_RENDERER_RESPONSE;
}

void NavigatorImpl::LogResourceRequestTime(
    base::TimeTicks timestamp, const GURL& url) {
  if (navigation_data_ && navigation_data_->url_ == url) {
    navigation_data_->url_job_start_time_ = timestamp;
    UMA_HISTOGRAM_TIMES(
        "Navigation.TimeToURLJobStart",
        navigation_data_->url_job_start_time_ - navigation_data_->start_time_);
  }
}

void NavigatorImpl::LogBeforeUnloadTime(
    const base::TimeTicks& renderer_before_unload_start_time,
    const base::TimeTicks& renderer_before_unload_end_time) {
  // Only stores the beforeunload delay if we're tracking a browser initiated
  // navigation and it happened later than the navigation request.
  if (navigation_data_ &&
      renderer_before_unload_start_time > navigation_data_->start_time_) {
    navigation_data_->before_unload_delay_ =
        renderer_before_unload_end_time - renderer_before_unload_start_time;
  }
}

void NavigatorImpl::CheckWebUIRendererDoesNotDisplayNormalURL(
    RenderFrameHostImpl* render_frame_host,
    const GURL& url) {
  int enabled_bindings =
      render_frame_host->render_view_host()->GetEnabledBindings();
  bool is_allowed_in_web_ui_renderer =
      WebUIControllerFactoryRegistry::GetInstance()->IsURLAcceptableForWebUI(
          controller_->GetBrowserContext(), url);
  if ((enabled_bindings & BINDINGS_POLICY_WEB_UI) &&
      !is_allowed_in_web_ui_renderer) {
    // Log the URL to help us diagnose any future failures of this CHECK.
    GetContentClient()->SetActiveURL(url);
    CHECK(0);
  }
}

// PlzNavigate
void NavigatorImpl::RequestNavigation(
    FrameTreeNode* frame_tree_node,
    const NavigationEntryImpl& entry,
    NavigationController::ReloadType reload_type,
    base::TimeTicks navigation_start) {
  CHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableBrowserSideNavigation));
  DCHECK(frame_tree_node);
  int64 frame_tree_node_id = frame_tree_node->frame_tree_node_id();
  FrameMsg_Navigate_Type::Value navigation_type =
      GetNavigationType(controller_->GetBrowserContext(), entry, reload_type);
  scoped_ptr<NavigationRequest> navigation_request =
      NavigationRequest::CreateBrowserInitiated(frame_tree_node, entry,
                                                navigation_type,
                                                navigation_start, controller_);
  // TODO(clamy): Check if navigations are blocked and if so store the
  // parameters.

  // If there is an ongoing request, replace it.
  navigation_request_map_.set(frame_tree_node_id, navigation_request.Pass());

  // Have the current renderer execute its beforeUnload event if needed. If it
  // is not needed (eg. the renderer is not live), BeginNavigation should get
  // called.
  NavigationRequest* request_to_send =
      navigation_request_map_.get(frame_tree_node_id);
  request_to_send->SetWaitingForRendererResponse();
  frame_tree_node->current_frame_host()->DispatchBeforeUnload(true);
}

void NavigatorImpl::BeginNavigation(FrameTreeNode* frame_tree_node) {
  NavigationRequest* navigation_request =
      navigation_request_map_.get(frame_tree_node->frame_tree_node_id());

  // A browser-initiated navigation could have been cancelled while it was
  // waiting for the BeforeUnload event to execute.
  if (!navigation_request)
    return;

  // Start the request.
   if (navigation_request->BeginNavigation()) {
    // If the request was sent to the IO thread, notify the
    // RenderFrameHostManager so it can speculatively create a RenderFrameHost
    // (and potentially a new renderer process) in parallel.
    frame_tree_node->render_manager()->BeginNavigation(*navigation_request);
  }
}

void NavigatorImpl::RecordNavigationMetrics(
    const LoadCommittedDetails& details,
    const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
    SiteInstance* site_instance) {
  DCHECK(site_instance->HasProcess());

  if (!details.is_in_page)
    RecordAction(base::UserMetricsAction("FrameLoad"));

  if (!details.is_main_frame || !navigation_data_ ||
      navigation_data_->url_job_start_time_.is_null() ||
      navigation_data_->url_ != params.original_request_url) {
    return;
  }

  base::TimeDelta time_to_commit =
      base::TimeTicks::Now() - navigation_data_->start_time_;
  UMA_HISTOGRAM_TIMES("Navigation.TimeToCommit", time_to_commit);

  time_to_commit -= navigation_data_->before_unload_delay_;
  base::TimeDelta time_to_network = navigation_data_->url_job_start_time_ -
                                    navigation_data_->start_time_ -
                                    navigation_data_->before_unload_delay_;
  if (navigation_data_->is_restoring_from_last_session_) {
    UMA_HISTOGRAM_TIMES(
        "Navigation.TimeToCommit_SessionRestored_BeforeUnloadDiscounted",
        time_to_commit);
    UMA_HISTOGRAM_TIMES(
        "Navigation.TimeToURLJobStart_SessionRestored_BeforeUnloadDiscounted",
        time_to_network);
    navigation_data_.reset();
    return;
  }
  bool navigation_created_new_renderer_process =
      site_instance->GetProcess()->GetInitTimeForNavigationMetrics() >
      navigation_data_->start_time_;
  if (navigation_created_new_renderer_process) {
    UMA_HISTOGRAM_TIMES(
        "Navigation.TimeToCommit_NewRenderer_BeforeUnloadDiscounted",
        time_to_commit);
    UMA_HISTOGRAM_TIMES(
        "Navigation.TimeToURLJobStart_NewRenderer_BeforeUnloadDiscounted",
        time_to_network);
  } else {
    UMA_HISTOGRAM_TIMES(
        "Navigation.TimeToCommit_ExistingRenderer_BeforeUnloadDiscounted",
        time_to_commit);
    UMA_HISTOGRAM_TIMES(
        "Navigation.TimeToURLJobStart_ExistingRenderer_BeforeUnloadDiscounted",
        time_to_network);
  }
  navigation_data_.reset();
}

}  // namespace content
