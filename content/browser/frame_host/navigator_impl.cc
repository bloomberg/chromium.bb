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
#include "content/common/frame_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"

namespace content {

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

}  // namespace content
