// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/navigator_impl.h"

#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/navigation_controller_impl.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/frame_host/navigator_delegate.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_view_host.h"
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
  RenderViewHost::FilterURL(render_process_host, false, &validated_url);

  if (is_main_frame) {
    // If there is no browser-initiated pending entry for this navigation and it
    // is not for the error URL, create a pending entry using the current
    // SiteInstance, and ensure the address bar updates accordingly.  We don't
    // know the referrer or extra headers at this point, but the referrer will
    // be set properly upon commit.
    NavigationEntryImpl* pending_entry =
        NavigationEntryImpl::FromNavigationEntry(
            controller_->GetPendingEntry());
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

}  // namespace content
