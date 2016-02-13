// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/navigator_impl.h"

#include <utility>

#include "base/metrics/histogram.h"
#include "base/time/time.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/navigation_controller_impl.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/frame_host/navigation_handle_impl.h"
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
#include "content/common/site_isolation_policy.h"
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
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/resource_response.h"
#include "content/public/common/url_constants.h"
#include "net/base/net_errors.h"

namespace content {

namespace {

FrameMsg_Navigate_Type::Value GetNavigationType(
    BrowserContext* browser_context, const NavigationEntryImpl& entry,
    NavigationController::ReloadType reload_type) {
  switch (reload_type) {
    case NavigationControllerImpl::RELOAD:
      return FrameMsg_Navigate_Type::RELOAD;
    case NavigationControllerImpl::RELOAD_IGNORING_CACHE:
    case NavigationControllerImpl::RELOAD_DISABLE_LOFI_MODE:
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

NavigatorDelegate* NavigatorImpl::GetDelegate() {
  return delegate_;
}

NavigationController* NavigatorImpl::GetController() {
  return controller_;
}

void NavigatorImpl::DidStartProvisionalLoad(
    RenderFrameHostImpl* render_frame_host,
    const GURL& url,
    const base::TimeTicks& navigation_start) {
  bool is_main_frame = render_frame_host->frame_tree_node()->IsMainFrame();
  bool is_error_page = (url.spec() == kUnreachableWebDataURL);
  bool is_iframe_srcdoc = (url.spec() == kAboutSrcDocURL);
  GURL validated_url(url);
  RenderProcessHost* render_process_host = render_frame_host->GetProcess();
  render_process_host->FilterURL(false, &validated_url);

  if (is_main_frame && !is_error_page) {
    DidStartMainFrameNavigation(validated_url,
                                render_frame_host->GetSiteInstance());
  }

  if (delegate_) {
    // Notify the observer about the start of the provisional load.
    delegate_->DidStartProvisionalLoad(render_frame_host, validated_url,
                                       is_error_page, is_iframe_srcdoc);
  }

  if (is_error_page || IsBrowserSideNavigationEnabled())
    return;

  if (render_frame_host->navigation_handle()) {
    if (render_frame_host->navigation_handle()->is_transferring()) {
      // If the navigation is completing a transfer, this
      // DidStartProvisionalLoad should not correspond to a new navigation.
      DCHECK_EQ(url, render_frame_host->navigation_handle()->GetURL());
      render_frame_host->navigation_handle()->set_is_transferring(false);
      return;
    }

    // This ensures that notifications about the end of the previous
    // navigation are sent before notifications about the start of the
    // new navigation.
    render_frame_host->SetNavigationHandle(scoped_ptr<NavigationHandleImpl>());
  }

  render_frame_host->SetNavigationHandle(NavigationHandleImpl::Create(
      validated_url, render_frame_host->frame_tree_node(),
      false,             // is_synchronous
      is_iframe_srcdoc,  // is_srcdoc
      navigation_start));
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
    controller_->DiscardPendingEntry(true);

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
    const base::string16& error_description,
    bool was_ignored_by_handler) {
  if (delegate_) {
    delegate_->DidFailLoadWithError(
        render_frame_host, url, error_code,
        error_description, was_ignored_by_handler);
  }
}

bool NavigatorImpl::NavigateToEntry(
    FrameTreeNode* frame_tree_node,
    const FrameNavigationEntry& frame_entry,
    const NavigationEntryImpl& entry,
    NavigationController::ReloadType reload_type,
    bool is_same_document_history_load,
    bool is_pending_entry) {
  TRACE_EVENT0("browser,navigation", "NavigatorImpl::NavigateToEntry");

  GURL dest_url = frame_entry.url();
  Referrer dest_referrer = frame_entry.referrer();
  if (reload_type ==
          NavigationController::ReloadType::RELOAD_ORIGINAL_REQUEST_URL &&
      entry.GetOriginalRequestURL().is_valid() && !entry.GetHasPostData()) {
    // We may have been redirected when navigating to the current URL.
    // Use the URL the user originally intended to visit, if it's valid and if a
    // POST wasn't involved; the latter case avoids issues with sending data to
    // the wrong page.
    dest_url = entry.GetOriginalRequestURL();
    dest_referrer = Referrer();
  }

  // Don't attempt to navigate to non-empty invalid URLs.
  if (!dest_url.is_valid() && !dest_url.is_empty()) {
    LOG(WARNING) << "Refusing to load invalid URL: "
                 << dest_url.possibly_invalid_spec();
    return false;
  }

  // The renderer will reject IPC messages with URLs longer than
  // this limit, so don't attempt to navigate with a longer URL.
  if (dest_url.spec().size() > kMaxURLChars) {
    LOG(WARNING) << "Refusing to load URL as it exceeds " << kMaxURLChars
                 << " characters.";
    return false;
  }

  // This will be used to set the Navigation Timing API navigationStart
  // parameter for browser navigations in new tabs (intents, tabs opened through
  // "Open link in new tab"). We need to keep it above RFHM::Navigate() call to
  // capture the time needed for the RenderFrameHost initialization.
  base::TimeTicks navigation_start = base::TimeTicks::Now();
  TRACE_EVENT_INSTANT_WITH_TIMESTAMP0(
      "navigation", "NavigationTiming navigationStart",
      TRACE_EVENT_SCOPE_GLOBAL, navigation_start.ToInternalValue());

  LoFiState lofi_state =
      (reload_type ==
       NavigationController::ReloadType::RELOAD_DISABLE_LOFI_MODE)
          ? LOFI_OFF
          : LOFI_UNSPECIFIED;

  // PlzNavigate: the RenderFrameHosts are no longer asked to navigate.
  if (IsBrowserSideNavigationEnabled()) {
    navigation_data_.reset(new NavigationMetricsData(navigation_start, dest_url,
                                                     entry.restore_type()));
    RequestNavigation(frame_tree_node, dest_url, dest_referrer, frame_entry,
                      entry, reload_type, lofi_state,
                      is_same_document_history_load, navigation_start);
    if (frame_tree_node->IsMainFrame() &&
        frame_tree_node->navigation_request()) {
      // TODO(carlosk): extend these traces to support subframes and
      // non-PlzNavigate navigations.
      // For these traces below we're using the navigation handle as the async
      // trace id, |navigation_start| as the timestamp and reporting the
      // FrameTreeNode id as a parameter. For navigations where no network
      // request is made (data URLs, JavaScript URLs, etc) there is no handle
      // and so no tracing is done.
      TRACE_EVENT_ASYNC_BEGIN_WITH_TIMESTAMP1(
          "navigation", "Navigation timeToNetworkStack",
          frame_tree_node->navigation_request()->navigation_handle(),
          navigation_start.ToInternalValue(),
          "FrameTreeNode id", frame_tree_node->frame_tree_node_id());
      TRACE_EVENT_ASYNC_BEGIN_WITH_TIMESTAMP1(
          "navigation", "Navigation timeToCommit",
          frame_tree_node->navigation_request()->navigation_handle(),
          navigation_start.ToInternalValue(),
          "FrameTreeNode id", frame_tree_node->frame_tree_node_id());
    }

    // Notify observers about navigation if this is for the pending entry.
    if (delegate_ && is_pending_entry)
      delegate_->DidStartNavigationToPendingEntry(dest_url, reload_type);

    return true;
  }

  RenderFrameHostImpl* dest_render_frame_host =
      frame_tree_node->render_manager()->Navigate(dest_url, frame_entry, entry);
  if (!dest_render_frame_host)
    return false;  // Unable to create the desired RenderFrameHost.

  // Make sure no code called via RFHM::Navigate clears the pending entry.
  if (is_pending_entry)
    CHECK_EQ(controller_->GetPendingEntry(), &entry);

  // For security, we should never send non-Web-UI URLs to a Web UI renderer.
  // Double check that here.
  CheckWebUIRendererDoesNotDisplayNormalURL(dest_render_frame_host, dest_url);

  // Navigate in the desired RenderFrameHost.
  // We can skip this step in the rare case that this is a transfer navigation
  // which began in the chosen RenderFrameHost, since the request has already
  // been issued.  In that case, simply resume the response.
  bool is_transfer_to_same =
      entry.transferred_global_request_id().child_id != -1 &&
      entry.transferred_global_request_id().child_id ==
          dest_render_frame_host->GetProcess()->GetID();
  if (!is_transfer_to_same) {
    navigation_data_.reset(new NavigationMetricsData(navigation_start, dest_url,
                                                     entry.restore_type()));
    // Create the navigation parameters.
    FrameMsg_Navigate_Type::Value navigation_type =
        GetNavigationType(controller_->GetBrowserContext(), entry, reload_type);
    dest_render_frame_host->Navigate(
        entry.ConstructCommonNavigationParams(dest_url, dest_referrer,
                                              navigation_type, lofi_state,
                                              navigation_start),
        entry.ConstructStartNavigationParams(),
        entry.ConstructRequestNavigationParams(
            frame_entry, is_same_document_history_load,
            frame_tree_node->has_committed_real_load(),
            controller_->GetPendingEntryIndex() == -1,
            controller_->GetIndexOfEntry(&entry),
            controller_->GetLastCommittedEntryIndex(),
            controller_->GetEntryCount()));
  } else {
    // No need to navigate again.  Just resume the deferred request.
    dest_render_frame_host->GetProcess()->ResumeDeferredNavigation(
        entry.transferred_global_request_id());
  }

  // Make sure no code called via RFH::Navigate clears the pending entry.
  if (is_pending_entry)
    CHECK_EQ(controller_->GetPendingEntry(), &entry);

  if (controller_->GetPendingEntryIndex() == -1 &&
      dest_url.SchemeIs(url::kJavaScriptScheme)) {
    // If the pending entry index is -1 (which means a new navigation rather
    // than a history one), and the user typed in a javascript: URL, don't add
    // it to the session history.
    //
    // This is a hack. What we really want is to avoid adding to the history any
    // URL that doesn't generate content, and what would be great would be if we
    // had a message from the renderer telling us that a new page was not
    // created. The same message could be used for mailto: URLs and the like.
    return false;
  }

  // Notify observers about navigation.
  if (delegate_ && is_pending_entry)
    delegate_->DidStartNavigationToPendingEntry(dest_url, reload_type);

  return true;
}

bool NavigatorImpl::NavigateToPendingEntry(
    FrameTreeNode* frame_tree_node,
    const FrameNavigationEntry& frame_entry,
    NavigationController::ReloadType reload_type,
    bool is_same_document_history_load) {
  return NavigateToEntry(frame_tree_node, frame_entry,
                         *controller_->GetPendingEntry(), reload_type,
                         is_same_document_history_load, true);
}

bool NavigatorImpl::NavigateNewChildFrame(
    RenderFrameHostImpl* render_frame_host,
    const std::string& unique_name) {
  NavigationEntryImpl* entry =
      controller_->GetEntryWithUniqueID(render_frame_host->nav_entry_id());
  if (!entry)
    return false;

  FrameNavigationEntry* frame_entry =
      entry->GetFrameEntryByUniqueName(unique_name);
  if (!frame_entry)
    return false;

  // Update the FrameNavigationEntry's FrameTreeNode ID (which is currently the
  // ID of the old FrameTreeNode that no longer exists) to be the ID of the
  // newly created frame.
  frame_entry->set_frame_tree_node_id(
      render_frame_host->frame_tree_node()->frame_tree_node_id());

  return NavigateToEntry(render_frame_host->frame_tree_node(), *frame_entry,
                         *entry, NavigationControllerImpl::NO_RELOAD, false,
                         false);
}

void NavigatorImpl::DidNavigate(
    RenderFrameHostImpl* render_frame_host,
    const FrameHostMsg_DidCommitProvisionalLoad_Params& params) {
  FrameTree* frame_tree = render_frame_host->frame_tree_node()->frame_tree();
  bool oopifs_possible = SiteIsolationPolicy::AreCrossProcessFramesPossible();

  bool is_navigation_within_page = controller_->IsURLInPageNavigation(
      params.url, params.was_within_same_page, render_frame_host);
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
      delegate_->DidNavigateMainFramePreCommit(is_navigation_within_page);
    }

    if (!oopifs_possible)
      frame_tree->root()->render_manager()->DidNavigateFrame(
          render_frame_host, params.gesture == NavigationGestureUser);
  }

  // Save the origin of the new page.  Do this before calling
  // DidNavigateFrame(), because the origin needs to be included in the SwapOut
  // message, which is sent inside DidNavigateFrame().  SwapOut needs the
  // origin because it creates a RenderFrameProxy that needs this to initialize
  // its security context. This origin will also be sent to RenderFrameProxies
  // created via ViewMsg_New and FrameMsg_NewFrameProxy.
  render_frame_host->frame_tree_node()->SetCurrentOrigin(params.origin);

  render_frame_host->frame_tree_node()->SetEnforceStrictMixedContentChecking(
      params.should_enforce_strict_mixed_content_checking);

  // When using --site-per-process, we notify the RFHM for all navigations,
  // not just main frame navigations.
  if (oopifs_possible) {
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

  // Keep track of each frame's URL in its FrameTreeNode.
  render_frame_host->frame_tree_node()->SetCurrentURL(params.url);

  if (did_navigate && render_frame_host->frame_tree_node()->IsMainFrame() &&
      IsBrowserSideNavigationEnabled()) {
    TRACE_EVENT_ASYNC_END0("navigation", "Navigation timeToCommit",
                           render_frame_host->navigation_handle());
  }

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
    render_frame_host->navigation_handle()->DidCommitNavigation(
        params, is_navigation_within_page, render_frame_host);
    render_frame_host->SetNavigationHandle(nullptr);
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
  // This call only makes sense for subframes if OOPIFs are possible.
  DCHECK(!render_frame_host->GetParent() ||
         SiteIsolationPolicy::AreCrossProcessFramesPossible());

  // If this came from a swapped out RenderFrameHost, we only allow the request
  // if we are still in the same BrowsingInstance.
  // TODO(creis): Move this to RenderFrameProxyHost::OpenURL.
  SiteInstance* current_site_instance = render_frame_host->frame_tree_node()
                                            ->current_frame_host()
                                            ->GetSiteInstance();
  if (render_frame_host->is_swapped_out() &&
      !render_frame_host->GetSiteInstance()->IsRelatedSiteInstance(
          current_site_instance)) {
    return;
  }

  // TODO(creis): Pass the redirect_chain into this method to support client
  // redirects.  http://crbug.com/311721.
  std::vector<GURL> redirect_chain;

  GURL dest_url(url);
  if (!GetContentClient()->browser()->ShouldAllowOpenURL(
          current_site_instance, url)) {
    dest_url = GURL(url::kAboutBlankURL);
  }

  int frame_tree_node_id = -1;

  // Send the navigation to the current FrameTreeNode if it's destined for a
  // subframe in the current tab.  We'll assume it's for the main frame
  // (possibly of a new or different WebContents) otherwise.
  if (SiteIsolationPolicy::AreCrossProcessFramesPossible() &&
      disposition == CURRENT_TAB && render_frame_host->GetParent()) {
    frame_tree_node_id =
        render_frame_host->frame_tree_node()->frame_tree_node_id();
  }

  OpenURLParams params(dest_url, referrer, frame_tree_node_id, disposition,
                       ui::PAGE_TRANSITION_LINK,
                       true /* is_renderer_initiated */);
  params.source_site_instance = source_site_instance;
  if (redirect_chain.size() > 0)
    params.redirect_chain = redirect_chain;
  params.should_replace_current_entry = should_replace_current_entry;
  params.user_gesture = user_gesture;

  if (render_frame_host->web_ui()) {
    // Web UI pages sometimes want to override the page transition type for
    // link clicks (e.g., so the new tab page can specify AUTO_BOOKMARK for
    // automatically generated suggestions).  We don't override other types
    // like TYPED because they have different implications (e.g., autocomplete).
    if (ui::PageTransitionCoreTypeIs(
        params.transition, ui::PAGE_TRANSITION_LINK))
      params.transition = render_frame_host->web_ui()->GetLinkTransitionType();

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

void NavigatorImpl::RequestTransferURL(
    RenderFrameHostImpl* render_frame_host,
    const GURL& url,
    const std::vector<GURL>& redirect_chain,
    const Referrer& referrer,
    ui::PageTransition page_transition,
    const GlobalRequestID& transferred_global_request_id,
    bool should_replace_current_entry) {
  // This call only makes sense for subframes if OOPIFs are possible.
  DCHECK(!render_frame_host->GetParent() ||
         SiteIsolationPolicy::AreCrossProcessFramesPossible());

  // Allow the delegate to cancel the transfer.
  if (!delegate_->ShouldTransferNavigation())
    return;

  GURL dest_url(url);
  Referrer referrer_to_use(referrer);
  FrameTreeNode* node = render_frame_host->frame_tree_node();
  SiteInstance* current_site_instance = render_frame_host->GetSiteInstance();
  if (!GetContentClient()->browser()->ShouldAllowOpenURL(current_site_instance,
                                                         url)) {
    dest_url = GURL(url::kAboutBlankURL);
  }

  // TODO(creis): Determine if this transfer started as a browser-initiated
  // navigation.  See https://crbug.com/495161.
  bool is_renderer_initiated = true;
  if (render_frame_host->web_ui()) {
    // Web UI pages sometimes want to override the page transition type for
    // link clicks (e.g., so the new tab page can specify AUTO_BOOKMARK for
    // automatically generated suggestions).  We don't override other types
    // like TYPED because they have different implications (e.g., autocomplete).
    if (ui::PageTransitionCoreTypeIs(page_transition, ui::PAGE_TRANSITION_LINK))
      page_transition = render_frame_host->web_ui()->GetLinkTransitionType();

    // Note also that we hide the referrer for Web UI pages. We don't really
    // want web sites to see a referrer of "chrome://blah" (and some
    // chrome: URLs might have search terms or other stuff we don't want to
    // send to the site), so we send no referrer.
    referrer_to_use = Referrer();

    // Navigations in Web UI pages count as browser-initiated navigations.
    is_renderer_initiated = false;
  }

  NavigationController::LoadURLParams load_url_params(dest_url);
  // The source_site_instance only matters for navigations via RenderFrameProxy,
  // which go through RequestOpenURL.
  load_url_params.source_site_instance = nullptr;
  load_url_params.transition_type = page_transition;
  load_url_params.frame_tree_node_id = node->frame_tree_node_id();
  load_url_params.referrer = referrer_to_use;
  load_url_params.redirect_chain = redirect_chain;
  load_url_params.is_renderer_initiated = is_renderer_initiated;
  load_url_params.transferred_global_request_id = transferred_global_request_id;
  load_url_params.should_replace_current_entry = should_replace_current_entry;

  controller_->LoadURLWithParams(load_url_params);
}

// PlzNavigate
void NavigatorImpl::OnBeforeUnloadACK(FrameTreeNode* frame_tree_node,
                                      bool proceed) {
  CHECK(IsBrowserSideNavigationEnabled());
  DCHECK(frame_tree_node);

  NavigationRequest* navigation_request = frame_tree_node->navigation_request();

  // The NavigationRequest may have been canceled while the renderer was
  // executing the BeforeUnload event.
  if (!navigation_request)
    return;

  DCHECK_EQ(NavigationRequest::WAITING_FOR_RENDERER_RESPONSE,
            navigation_request->state());

  // If the navigation is allowed to proceed, send the request to the IO thread.
  if (proceed)
    navigation_request->BeginNavigation();
  else
    CancelNavigation(frame_tree_node);
}

// PlzNavigate
void NavigatorImpl::OnBeginNavigation(
    FrameTreeNode* frame_tree_node,
    const CommonNavigationParams& common_params,
    const BeginNavigationParams& begin_params,
    scoped_refptr<ResourceRequestBody> body) {
  // TODO(clamy): the url sent by the renderer should be validated with
  // FilterURL.
  // This is a renderer-initiated navigation.
  CHECK(IsBrowserSideNavigationEnabled());
  DCHECK(frame_tree_node);

  NavigationRequest* ongoing_navigation_request =
      frame_tree_node->navigation_request();

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
  // NavigationRequest is created for the node.
  frame_tree_node->CreatedNavigationRequest(
      NavigationRequest::CreateRendererInitiated(
          frame_tree_node, common_params, begin_params, body,
          controller_->GetLastCommittedEntryIndex(),
          controller_->GetEntryCount()));
  NavigationRequest* navigation_request = frame_tree_node->navigation_request();
  navigation_request->CreateNavigationHandle();

  if (frame_tree_node->IsMainFrame()) {
    // Renderer-initiated main-frame navigations that need to swap processes
    // will go to the browser via a OpenURL call, and then be handled by the
    // same code path as browser-initiated navigations. For renderer-initiated
    // main frame navigation that start via a BeginNavigation IPC, the
    // RenderFrameHost will not be swapped. Therefore it is safe to call
    // DidStartMainFrameNavigation with the SiteInstance from the current
    // RenderFrameHost.
    DidStartMainFrameNavigation(
        common_params.url,
        frame_tree_node->current_frame_host()->GetSiteInstance());
    navigation_data_.reset();
  }

  navigation_request->BeginNavigation();
}

// PlzNavigate
void NavigatorImpl::CommitNavigation(FrameTreeNode* frame_tree_node,
                                     ResourceResponse* response,
                                     scoped_ptr<StreamHandle> body) {
  CHECK(IsBrowserSideNavigationEnabled());

  NavigationRequest* navigation_request = frame_tree_node->navigation_request();
  DCHECK(navigation_request);
  DCHECK(response ||
         !ShouldMakeNetworkRequestForURL(
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

  // The renderer can exit view source mode when any error or cancellation
  // happen. When reusing the same renderer, overwrite to recover the mode.
  if (navigation_request->is_view_source() &&
      render_frame_host ==
          frame_tree_node->render_manager()->current_frame_host()) {
    DCHECK(!render_frame_host->GetParent());
    render_frame_host->render_view_host()->Send(
        new ViewMsg_EnableViewSourceMode(
            render_frame_host->render_view_host()->GetRoutingID()));
  }

  CheckWebUIRendererDoesNotDisplayNormalURL(
      render_frame_host, navigation_request->common_params().url);

  navigation_request->TransferNavigationHandleOwnership(render_frame_host);
  render_frame_host->navigation_handle()->ReadyToCommitNavigation(
      render_frame_host, response ? response->head.headers : nullptr);
  render_frame_host->CommitNavigation(response, std::move(body),
                                      navigation_request->common_params(),
                                      navigation_request->request_params());
}

// PlzNavigate
void NavigatorImpl::FailedNavigation(FrameTreeNode* frame_tree_node,
                                     bool has_stale_copy_in_cache,
                                     int error_code) {
  CHECK(IsBrowserSideNavigationEnabled());

  NavigationRequest* navigation_request = frame_tree_node->navigation_request();
  DCHECK(navigation_request);

  // If the request was canceled by the user do not show an error page.
  if (error_code == net::ERR_ABORTED) {
    frame_tree_node->ResetNavigationRequest(false);
    return;
  }

  // Select an appropriate renderer to show the error page.
  RenderFrameHostImpl* render_frame_host =
      frame_tree_node->render_manager()->GetFrameHostForNavigation(
          *navigation_request);
  CheckWebUIRendererDoesNotDisplayNormalURL(
      render_frame_host, navigation_request->common_params().url);

  navigation_request->TransferNavigationHandleOwnership(render_frame_host);
  render_frame_host->navigation_handle()->ReadyToCommitNavigation(
      render_frame_host, scoped_refptr<net::HttpResponseHeaders>());
  render_frame_host->FailedNavigation(navigation_request->common_params(),
                                      navigation_request->request_params(),
                                      has_stale_copy_in_cache, error_code);
}

// PlzNavigate
void NavigatorImpl::CancelNavigation(FrameTreeNode* frame_tree_node) {
  CHECK(IsBrowserSideNavigationEnabled());
  frame_tree_node->ResetNavigationRequest(false);
  if (frame_tree_node->IsMainFrame())
    navigation_data_.reset();
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
    const GURL& dest_url,
    const Referrer& dest_referrer,
    const FrameNavigationEntry& frame_entry,
    const NavigationEntryImpl& entry,
    NavigationController::ReloadType reload_type,
    LoFiState lofi_state,
    bool is_same_document_history_load,
    base::TimeTicks navigation_start) {
  CHECK(IsBrowserSideNavigationEnabled());
  DCHECK(frame_tree_node);

  // This value must be set here because creating a NavigationRequest might
  // change the renderer live/non-live status and change this result.
  bool should_dispatch_beforeunload =
      frame_tree_node->current_frame_host()->ShouldDispatchBeforeUnload();
  FrameMsg_Navigate_Type::Value navigation_type =
      GetNavigationType(controller_->GetBrowserContext(), entry, reload_type);
  frame_tree_node->CreatedNavigationRequest(
      NavigationRequest::CreateBrowserInitiated(
          frame_tree_node, dest_url, dest_referrer, frame_entry, entry,
          navigation_type, lofi_state, is_same_document_history_load,
          navigation_start, controller_));
  NavigationRequest* navigation_request = frame_tree_node->navigation_request();
  navigation_request->CreateNavigationHandle();

  // Have the current renderer execute its beforeunload event if needed. If it
  // is not needed (when beforeunload dispatch is not needed or this navigation
  // is synchronous and same-site) then NavigationRequest::BeginNavigation
  // should be directly called instead.
  if (should_dispatch_beforeunload &&
      ShouldMakeNetworkRequestForURL(
          navigation_request->common_params().url)) {
    navigation_request->SetWaitingForRendererResponse();
    frame_tree_node->current_frame_host()->DispatchBeforeUnload(true);
  } else {
    navigation_request->BeginNavigation();
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

void NavigatorImpl::DidStartMainFrameNavigation(
    const GURL& url,
    SiteInstanceImpl* site_instance) {
  // If there is no browser-initiated pending entry for this navigation and it
  // is not for the error URL, create a pending entry using the current
  // SiteInstance, and ensure the address bar updates accordingly.  We don't
  // know the referrer or extra headers at this point, but the referrer will
  // be set properly upon commit.
  NavigationEntryImpl* pending_entry = controller_->GetPendingEntry();
  bool has_browser_initiated_pending_entry =
      pending_entry && !pending_entry->is_renderer_initiated();
  if (!has_browser_initiated_pending_entry) {
    scoped_ptr<NavigationEntryImpl> entry =
        NavigationEntryImpl::FromNavigationEntry(
            controller_->CreateNavigationEntry(
                url, content::Referrer(), ui::PAGE_TRANSITION_LINK,
                true /* is_renderer_initiated */, std::string(),
                controller_->GetBrowserContext()));
    entry->set_site_instance(site_instance);
    // TODO(creis): If there's a pending entry already, find a safe way to
    // update it instead of replacing it and copying over things like this.
    if (pending_entry) {
      entry->set_transferred_global_request_id(
          pending_entry->transferred_global_request_id());
      entry->set_should_replace_entry(pending_entry->should_replace_entry());
      entry->SetRedirectChain(pending_entry->GetRedirectChain());
    }
    controller_->SetPendingEntry(std::move(entry));
    if (delegate_)
      delegate_->NotifyChangedNavigationState(content::INVALIDATE_TYPE_URL);
  }
}

}  // namespace content
