// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/navigator_impl.h"

#include "base/command_line.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/navigation_controller_impl.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/frame_host/navigator_delegate.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/webui/web_ui_controller_factory_registry.h"
#include "content/common/frame_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"

namespace content {

namespace {

ViewMsg_Navigate_Type::Value GetNavigationType(
    BrowserContext* browser_context, const NavigationEntryImpl& entry,
    NavigationController::ReloadType reload_type) {
  switch (reload_type) {
    case NavigationControllerImpl::RELOAD:
      return ViewMsg_Navigate_Type::RELOAD;
    case NavigationControllerImpl::RELOAD_IGNORING_CACHE:
      return ViewMsg_Navigate_Type::RELOAD_IGNORING_CACHE;
    case NavigationControllerImpl::RELOAD_ORIGINAL_REQUEST_URL:
      return ViewMsg_Navigate_Type::RELOAD_ORIGINAL_REQUEST_URL;
    case NavigationControllerImpl::NO_RELOAD:
      break;  // Fall through to rest of function.
  }

  // |RenderViewImpl::PopulateStateFromPendingNavigationParams| differentiates
  // between |RESTORE_WITH_POST| and |RESTORE|.
  if (entry.restore_type() ==
      NavigationEntryImpl::RESTORE_LAST_SESSION_EXITED_CLEANLY) {
    if (entry.GetHasPostData())
      return ViewMsg_Navigate_Type::RESTORE_WITH_POST;
    return ViewMsg_Navigate_Type::RESTORE;
  }

  return ViewMsg_Navigate_Type::NORMAL;
}

void MakeNavigateParams(const NavigationEntryImpl& entry,
                        const NavigationControllerImpl& controller,
                        NavigationController::ReloadType reload_type,
                        ViewMsg_Navigate_Params* params) {
  params->page_id = entry.GetPageID();
  params->should_clear_history_list = entry.should_clear_history_list();
  params->should_replace_current_entry = entry.should_replace_entry();
  if (entry.should_clear_history_list()) {
    // Set the history list related parameters to the same values a
    // NavigationController would return before its first navigation. This will
    // fully clear the RenderView's view of the session history.
    params->pending_history_list_offset = -1;
    params->current_history_list_offset = -1;
    params->current_history_list_length = 0;
  } else {
    params->pending_history_list_offset = controller.GetIndexOfEntry(&entry);
    params->current_history_list_offset =
        controller.GetLastCommittedEntryIndex();
    params->current_history_list_length = controller.GetEntryCount();
  }
  params->url = entry.GetURL();
  if (!entry.GetBaseURLForDataURL().is_empty()) {
    params->base_url_for_data_url = entry.GetBaseURLForDataURL();
    params->history_url_for_data_url = entry.GetVirtualURL();
  }
  params->referrer = entry.GetReferrer();
  params->transition = entry.GetTransitionType();
  params->page_state = entry.GetPageState();
  params->navigation_type =
      GetNavigationType(controller.GetBrowserContext(), entry, reload_type);
  params->request_time = base::Time::Now();
  params->extra_headers = entry.extra_headers();
  params->transferred_request_child_id =
      entry.transferred_global_request_id().child_id;
  params->transferred_request_request_id =
      entry.transferred_global_request_id().request_id;
  params->is_overriding_user_agent = entry.GetIsOverridingUserAgent();
  // Avoid downloading when in view-source mode.
  params->allow_download = !entry.IsViewSourceMode();
  params->is_post = entry.GetHasPostData();
  if (entry.GetBrowserInitiatedPostData()) {
    params->browser_initiated_post_data.assign(
        entry.GetBrowserInitiatedPostData()->front(),
        entry.GetBrowserInitiatedPostData()->front() +
            entry.GetBrowserInitiatedPostData()->size());
  }

  params->redirects = entry.redirect_chain();

  params->can_load_local_resources = entry.GetCanLoadLocalResources();
  params->frame_to_navigate = entry.GetFrameToNavigate();
}

}  // namespace


NavigatorImpl::NavigatorImpl(
    NavigationControllerImpl* navigation_controller,
    NavigatorDelegate* delegate)
    : controller_(navigation_controller),
      delegate_(delegate) {
}

void NavigatorImpl::DidStartProvisionalLoad(
    RenderFrameHostImpl* render_frame_host,
    int64 frame_id,
    int64 parent_frame_id,
    bool is_main_frame,
    const GURL& url) {
  bool is_error_page = (url.spec() == kUnreachableWebDataURL);
  bool is_iframe_srcdoc = (url.spec() == kAboutSrcDocURL);
  GURL validated_url(url);
  RenderProcessHost* render_process_host = render_frame_host->GetProcess();
  render_process_host->FilterURL(false, &validated_url);

  // TODO(creis): This is a hack for now, until we mirror the frame tree and do
  // cross-process subframe navigations in actual subframes.  As a result, we
  // can currently only support a single cross-process subframe per RVH.
  NavigationEntryImpl* pending_entry =
      NavigationEntryImpl::FromNavigationEntry(controller_->GetPendingEntry());
  if (pending_entry &&
      pending_entry->frame_tree_node_id() != -1 &&
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kSitePerProcess))
    is_main_frame = false;

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
                                             content::PAGE_TRANSITION_LINK,
                                             true /* is_renderer_initiated */,
                                             std::string(),
                                             controller_->GetBrowserContext()));
      entry->set_site_instance(
          static_cast<SiteInstanceImpl*>(
              render_frame_host->render_view_host()->GetSiteInstance()));
      // TODO(creis): If there's a pending entry already, find a safe way to
      // update it instead of replacing it and copying over things like this.
      if (pending_entry) {
        entry->set_transferred_global_request_id(
            pending_entry->transferred_global_request_id());
        entry->set_should_replace_entry(pending_entry->should_replace_entry());
        entry->set_redirect_chain(pending_entry->redirect_chain());
      }
      controller_->SetPendingEntry(entry);
      if (delegate_)
        delegate_->NotifyChangedNavigationState(content::INVALIDATE_TYPE_URL);
    }
  }

  if (delegate_) {
    // Notify the observer about the start of the provisional load.
    delegate_->DidStartProvisionalLoad(
        render_frame_host, frame_id, parent_frame_id, is_main_frame,
        validated_url, is_error_page, is_iframe_srcdoc);
  }
}


void NavigatorImpl::DidFailProvisionalLoadWithError(
    RenderFrameHostImpl* render_frame_host,
    const FrameHostMsg_DidFailProvisionalLoadWithError_Params& params) {
  VLOG(1) << "Failed Provisional Load: " << params.url.possibly_invalid_spec()
          << ", error_code: " << params.error_code
          << ", error_description: " << params.error_description
          << ", is_main_frame: " << params.is_main_frame
          << ", showing_repost_interstitial: " <<
            params.showing_repost_interstitial
          << ", frame_id: " << params.frame_id;
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

  // Do not usually clear the pending entry if one exists, so that the user's
  // typed URL is not lost when a navigation fails or is aborted.  However, in
  // cases that we don't show the pending entry (e.g., renderer-initiated
  // navigations in an existing tab), we don't keep it around.  That prevents
  // spoofs on in-page navigations that don't go through
  // DidStartProvisionalLoadForFrame.
  // In general, we allow the view to clear the pending entry and typed URL if
  // the user requests (e.g., hitting Escape with focus in the address bar).
  // Note: don't touch the transient entry, since an interstitial may exist.
  if (controller_->GetPendingEntry() != controller_->GetVisibleEntry())
    controller_->DiscardPendingEntry();

  delegate_->DidFailProvisionalLoadWithError(render_frame_host, params);
}

void NavigatorImpl::DidRedirectProvisionalLoad(
    RenderFrameHostImpl* render_frame_host,
    int32 page_id,
    const GURL& source_url,
    const GURL& target_url) {
  // TODO(creis): Remove this method and have the pre-rendering code listen to
  // WebContentsObserver::DidGetRedirectForResourceRequest instead.
  // See http://crbug.com/78512.
  GURL validated_source_url(source_url);
  GURL validated_target_url(target_url);
  RenderProcessHost* render_process_host = render_frame_host->GetProcess();
  render_process_host->FilterURL(false, &validated_source_url);
  render_process_host->FilterURL(false, &validated_target_url);
  NavigationEntry* entry;
  if (page_id == -1) {
    entry = controller_->GetPendingEntry();
  } else {
    entry = controller_->GetEntryWithPageID(
        render_frame_host->GetSiteInstance(), page_id);
  }
  if (!entry || entry->GetURL() != validated_source_url)
    return;

  delegate_->DidRedirectProvisionalLoad(
      render_frame_host, validated_target_url);
}

bool NavigatorImpl::NavigateToEntry(
    RenderFrameHostImpl* render_frame_host,
    const NavigationEntryImpl& entry,
    NavigationController::ReloadType reload_type) {
  TRACE_EVENT0("browser", "NavigatorImpl::NavigateToEntry");

  // The renderer will reject IPC messages with URLs longer than
  // this limit, so don't attempt to navigate with a longer URL.
  if (entry.GetURL().spec().size() > GetMaxURLChars()) {
    LOG(WARNING) << "Refusing to load URL as it exceeds " << GetMaxURLChars()
                 << " characters.";
    return false;
  }

  // Use entry->frame_tree_node_id() to pick which RenderFrameHostManager to
  // use when --site-per-process is used.
  RenderFrameHostManager* manager =
      render_frame_host->frame_tree_node()->render_manager();
  if (entry.frame_tree_node_id() != -1 &&
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kSitePerProcess)) {
    int64 frame_tree_node_id = entry.frame_tree_node_id();
    manager = render_frame_host->frame_tree_node()->frame_tree()->FindByID(
        frame_tree_node_id)->render_manager();
  }

  RenderFrameHostImpl* dest_render_frame_host = manager->Navigate(entry);
  if (!dest_render_frame_host)
    return false;  // Unable to create the desired RenderFrameHost.

  // For security, we should never send non-Web-UI URLs to a Web UI renderer.
  // Double check that here.
  int enabled_bindings =
      dest_render_frame_host->render_view_host()->GetEnabledBindings();
  bool is_allowed_in_web_ui_renderer =
      WebUIControllerFactoryRegistry::GetInstance()->IsURLAcceptableForWebUI(
          controller_->GetBrowserContext(), entry.GetURL());
  if ((enabled_bindings & BINDINGS_POLICY_WEB_UI) &&
      !is_allowed_in_web_ui_renderer) {
    // Log the URL to help us diagnose any future failures of this CHECK.
    GetContentClient()->SetActiveURL(entry.GetURL());
    CHECK(0);
  }

  // Notify observers that we will navigate in this RenderFrame.
  if (delegate_)
    delegate_->AboutToNavigateRenderFrame(dest_render_frame_host);

  // Used for page load time metrics.
  current_load_start_ = base::TimeTicks::Now();

  // Navigate in the desired RenderViewHost.
  // TODO(creis): As a temporary hack, we currently do cross-process subframe
  // navigations in a top-level frame of the new process.  Thus, we don't yet
  // need to store the correct frame ID in ViewMsg_Navigate_Params.
  ViewMsg_Navigate_Params navigate_params;
  MakeNavigateParams(entry, *controller_, reload_type, &navigate_params);
  dest_render_frame_host->render_view_host()->Navigate(navigate_params);

  if (entry.GetPageID() == -1) {
    // HACK!!  This code suppresses javascript: URLs from being added to
    // session history, which is what we want to do for javascript: URLs that
    // do not generate content.  What we really need is a message from the
    // renderer telling us that a new page was not created.  The same message
    // could be used for mailto: URLs and the like.
    if (entry.GetURL().SchemeIs(kJavaScriptScheme))
      return false;
  }

  // Notify observers about navigation.
  if (delegate_) {
    delegate_->DidStartNavigationToPendingEntry(render_frame_host,
                                                entry.GetURL(),
                                                reload_type);
  }

  return true;
}

bool NavigatorImpl::NavigateToPendingEntry(
    RenderFrameHostImpl* render_frame_host,
    NavigationController::ReloadType reload_type) {
  return NavigateToEntry(
      render_frame_host,
      *NavigationEntryImpl::FromNavigationEntry(controller_->GetPendingEntry()),
      reload_type);
}

base::TimeTicks NavigatorImpl::GetCurrentLoadStart() {
  return current_load_start_;
}

}  // namespace content
