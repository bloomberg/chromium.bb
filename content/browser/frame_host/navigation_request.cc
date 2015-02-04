// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/navigation_request.h"

#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/navigation_request_info.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/loader/navigation_url_loader.h"
#include "content/browser/site_instance_impl.h"
#include "content/common/resource_request_body.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/stream_handle.h"
#include "net/url_request/redirect_info.h"

namespace content {

// static
scoped_ptr<NavigationRequest> NavigationRequest::Create(
    FrameTreeNode* frame_tree_node,
    const NavigationEntryImpl& entry,
    FrameMsg_Navigate_Type::Value navigation_type,
    base::TimeTicks navigation_start) {
  FrameMsg_UILoadMetricsReportType::Value report_type =
      FrameMsg_UILoadMetricsReportType::NO_REPORT;
  base::TimeTicks ui_timestamp = base::TimeTicks();
#if defined(OS_ANDROID)
  if (!entry.intent_received_timestamp().is_null())
    report_type = FrameMsg_UILoadMetricsReportType::REPORT_INTENT;
  ui_timestamp = entry.intent_received_timestamp();
#endif

  scoped_ptr<NavigationRequest> navigation_request(new NavigationRequest(
      frame_tree_node,
      CommonNavigationParams(entry.GetURL(), entry.GetReferrer(),
                             entry.GetTransitionType(), navigation_type,
                             !entry.IsViewSourceMode(),ui_timestamp,
                             report_type),
      CommitNavigationParams(entry.GetPageState(),
                             entry.GetIsOverridingUserAgent(),
                             navigation_start),
      &entry));
  return navigation_request.Pass();
}

NavigationRequest::NavigationRequest(
    FrameTreeNode* frame_tree_node,
    const CommonNavigationParams& common_params,
    const CommitNavigationParams& commit_params,
    const NavigationEntryImpl* entry)
    : frame_tree_node_(frame_tree_node),
      common_params_(common_params),
      commit_params_(commit_params),
      state_(NOT_STARTED),
      restore_type_(NavigationEntryImpl::RESTORE_NONE),
      is_view_source_(false),
      bindings_(NavigationEntryImpl::kInvalidBindings) {
  if (entry) {
    source_site_instance_ = entry->source_site_instance();
    dest_site_instance_ = entry->site_instance();
    restore_type_ = entry->restore_type();
    is_view_source_ = entry->IsViewSourceMode();
    bindings_ = entry->bindings();
  }
}

NavigationRequest::~NavigationRequest() {
}

void NavigationRequest::BeginNavigation(
    scoped_ptr<NavigationRequestInfo> info,
    scoped_refptr<ResourceRequestBody> request_body) {
  DCHECK(!loader_);
  DCHECK(state_ == NOT_STARTED || state_ == WAITING_FOR_RENDERER_RESPONSE);
  state_ = STARTED;
  loader_ = NavigationURLLoader::Create(
      frame_tree_node_->navigator()->GetController()->GetBrowserContext(),
      frame_tree_node_->frame_tree_node_id(), common_params_, info.Pass(),
      request_body.get(), this);

  // TODO(davidben): Fire (and add as necessary) observer methods such as
  // DidStartProvisionalLoadForFrame for the navigation.
}

void NavigationRequest::OnRequestRedirected(
    const net::RedirectInfo& redirect_info,
    const scoped_refptr<ResourceResponse>& response) {
  // TODO(davidben): Track other changes from redirects. These are important
  // for, e.g., reloads.
  common_params_.url = redirect_info.new_url;

  // TODO(davidben): This where prerender and navigation_interceptor should be
  // integrated. For now, just always follow all redirects.
  loader_->FollowRedirect();
}

void NavigationRequest::OnResponseStarted(
    const scoped_refptr<ResourceResponse>& response,
    scoped_ptr<StreamHandle> body) {
  DCHECK(state_ == STARTED);
  state_ = RESPONSE_STARTED;
  frame_tree_node_->navigator()->CommitNavigation(frame_tree_node_,
                                                  response.get(), body.Pass());
}

void NavigationRequest::OnRequestFailed(int net_error) {
  DCHECK(state_ == STARTED);
  state_ = FAILED;
  // TODO(davidben): Network failures should display a network error page.
  NOTIMPLEMENTED() << " where net_error=" << net_error;
}

void NavigationRequest::OnRequestStarted(base::TimeTicks timestamp) {
  frame_tree_node_->navigator()->LogResourceRequestTime(timestamp,
                                                        common_params_.url);
}

}  // namespace content
