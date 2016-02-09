// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/navigation_request.h"

#include <utility>

#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/navigation_controller_impl.h"
#include "content/browser/frame_host/navigation_handle_impl.h"
#include "content/browser/frame_host/navigation_request_info.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/loader/navigation_url_loader.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_navigation_handle.h"
#include "content/browser/site_instance_impl.h"
#include "content/common/resource_request_body.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/stream_handle.h"
#include "content/public/common/content_client.h"
#include "content/public/common/resource_response.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/redirect_info.h"
#include "third_party/WebKit/public/web/WebSandboxFlags.h"

namespace content {

namespace {

// Returns the net load flags to use based on the navigation type.
// TODO(clamy): unify the code with what is happening on the renderer side.
int LoadFlagFromNavigationType(FrameMsg_Navigate_Type::Value navigation_type) {
  int load_flags = net::LOAD_NORMAL;
  switch (navigation_type) {
    case FrameMsg_Navigate_Type::RELOAD:
    case FrameMsg_Navigate_Type::RELOAD_ORIGINAL_REQUEST_URL:
      load_flags |= net::LOAD_VALIDATE_CACHE;
      break;
    case FrameMsg_Navigate_Type::RELOAD_IGNORING_CACHE:
      load_flags |= net::LOAD_BYPASS_CACHE;
      break;
    case FrameMsg_Navigate_Type::RESTORE:
      load_flags |= net::LOAD_PREFERRING_CACHE;
      break;
    case FrameMsg_Navigate_Type::RESTORE_WITH_POST:
      load_flags |= net::LOAD_ONLY_FROM_CACHE;
      break;
    case FrameMsg_Navigate_Type::NORMAL:
    default:
      break;
  }
  return load_flags;
}

}  // namespace

// static
scoped_ptr<NavigationRequest> NavigationRequest::CreateBrowserInitiated(
    FrameTreeNode* frame_tree_node,
    const GURL& dest_url,
    const Referrer& dest_referrer,
    const FrameNavigationEntry& frame_entry,
    const NavigationEntryImpl& entry,
    FrameMsg_Navigate_Type::Value navigation_type,
    bool is_same_document_history_load,
    const base::TimeTicks& navigation_start,
    NavigationControllerImpl* controller) {
  std::string method = entry.GetHasPostData() ? "POST" : "GET";

  // Copy existing headers and add necessary headers that may not be present
  // in the RequestNavigationParams.
  net::HttpRequestHeaders headers;
  headers.AddHeadersFromString(entry.extra_headers());
  headers.SetHeaderIfMissing(net::HttpRequestHeaders::kUserAgent,
                             GetContentClient()->GetUserAgent());
  // TODO(clamy): match what blink is doing with accept headers.
  headers.SetHeaderIfMissing("Accept", "*/*");

  // Fill POST data from the browser in the request body.
  scoped_refptr<ResourceRequestBody> request_body;
  if (entry.GetHasPostData()) {
    request_body = new ResourceRequestBody();
    request_body->AppendBytes(
        reinterpret_cast<const char *>(
            entry.GetBrowserInitiatedPostData()->front()),
        entry.GetBrowserInitiatedPostData()->size());
  }

  scoped_ptr<NavigationRequest> navigation_request(new NavigationRequest(
      frame_tree_node, entry.ConstructCommonNavigationParams(
                           dest_url, dest_referrer, navigation_type,
                           LOFI_UNSPECIFIED, navigation_start),
      BeginNavigationParams(method, headers.ToString(),
                            LoadFlagFromNavigationType(navigation_type),
                            false,  // has_user_gestures
                            false,  // skip_service_worker
                            REQUEST_CONTEXT_TYPE_LOCATION),
      entry.ConstructRequestNavigationParams(
          frame_entry, is_same_document_history_load,
          frame_tree_node->has_committed_real_load(),
          controller->GetPendingEntryIndex() == -1,
          controller->GetIndexOfEntry(&entry),
          controller->GetLastCommittedEntryIndex(),
          controller->GetEntryCount()),
      request_body, true, &frame_entry, &entry));
  return navigation_request;
}

// static
scoped_ptr<NavigationRequest> NavigationRequest::CreateRendererInitiated(
    FrameTreeNode* frame_tree_node,
    const CommonNavigationParams& common_params,
    const BeginNavigationParams& begin_params,
    scoped_refptr<ResourceRequestBody> body,
    int current_history_list_offset,
    int current_history_list_length) {
  // TODO(clamy): Check if some PageState should be provided here.
  // TODO(clamy): See how we should handle override of the user agent when the
  // navigation may start in a renderer and commit in another one.
  // TODO(clamy): See if the navigation start time should be measured in the
  // renderer and sent to the browser instead of being measured here.
  // TODO(clamy): The pending history list offset should be properly set.
  // TODO(clamy): Set has_committed_real_load.
  RequestNavigationParams request_params(
      false,                   // is_overriding_user_agent
      std::vector<GURL>(),     // redirects
      false,                   // can_load_local_resources
      base::Time::Now(),       // request_time
      PageState(),             // page_state
      -1,                      // page_id
      0,                       // nav_entry_id
      false,                   // is_same_document_history_load
      false,                   // has_committed_real_load
      false,                   // intended_as_new_entry
      -1,                      // pending_history_list_offset
      current_history_list_offset, current_history_list_length,
      false,                   // is_view_source
      false);                  // should_clear_history_list
  scoped_ptr<NavigationRequest> navigation_request(
      new NavigationRequest(frame_tree_node, common_params, begin_params,
                            request_params, body, false, nullptr, nullptr));
  return navigation_request;
}

NavigationRequest::NavigationRequest(
    FrameTreeNode* frame_tree_node,
    const CommonNavigationParams& common_params,
    const BeginNavigationParams& begin_params,
    const RequestNavigationParams& request_params,
    scoped_refptr<ResourceRequestBody> body,
    bool browser_initiated,
    const FrameNavigationEntry* frame_entry,
    const NavigationEntryImpl* entry)
    : frame_tree_node_(frame_tree_node),
      common_params_(common_params),
      begin_params_(begin_params),
      request_params_(request_params),
      browser_initiated_(browser_initiated),
      state_(NOT_STARTED),
      restore_type_(NavigationEntryImpl::RESTORE_NONE),
      is_view_source_(false),
      bindings_(NavigationEntryImpl::kInvalidBindings) {
  DCHECK(!browser_initiated || (entry != nullptr && frame_entry != nullptr));
  if (browser_initiated) {
    // TODO(clamy): use the FrameNavigationEntry for the source SiteInstance
    // once it has been moved from the NavigationEntry.
    source_site_instance_ = entry->source_site_instance();
    dest_site_instance_ = frame_entry->site_instance();
    restore_type_ = entry->restore_type();
    is_view_source_ = entry->IsViewSourceMode();
    bindings_ = entry->bindings();
  } else {
    // This is needed to have about:blank and data URLs commit in the same
    // SiteInstance as the initiating renderer.
    source_site_instance_ =
        frame_tree_node->current_frame_host()->GetSiteInstance();
  }

  // TODO(mkwst): This is incorrect. It ought to use the definition from
  // 'Document::firstPartyForCookies()' in Blink, which walks the ancestor tree
  // and verifies that all origins are PSL-matches (and special-cases extension
  // URLs).
  const GURL& first_party_for_cookies =
      frame_tree_node->IsMainFrame()
          ? common_params.url
          : frame_tree_node->frame_tree()->root()->current_url();
  bool parent_is_main_frame = !frame_tree_node->parent() ?
      false : frame_tree_node->parent()->IsMainFrame();
  info_.reset(new NavigationRequestInfo(
      common_params, begin_params, first_party_for_cookies,
      frame_tree_node->frame_origin(), frame_tree_node->IsMainFrame(),
      parent_is_main_frame, frame_tree_node->frame_tree_node_id(), body));
}

NavigationRequest::~NavigationRequest() {
}

void NavigationRequest::BeginNavigation() {
  DCHECK(!loader_);
  DCHECK(state_ == NOT_STARTED || state_ == WAITING_FOR_RENDERER_RESPONSE);
  state_ = STARTED;

  if (ShouldMakeNetworkRequestForURL(common_params_.url)) {
    // It's safe to use base::Unretained because this NavigationRequest owns
    // the NavigationHandle where the callback will be stored.
    // TODO(clamy): pass the real value for |is_external_protocol| if needed.
    navigation_handle_->WillStartRequest(
        begin_params_.method == "POST",
        Referrer::SanitizeForRequest(common_params_.url,
                                     common_params_.referrer),
        begin_params_.has_user_gesture, common_params_.transition, false,
        base::Bind(&NavigationRequest::OnStartChecksComplete,
                   base::Unretained(this)));
    return;
  }

  // There is no need to make a network request for this navigation, so commit
  // it immediately.
  state_ = RESPONSE_STARTED;
  frame_tree_node_->navigator()->CommitNavigation(
      frame_tree_node_, nullptr, scoped_ptr<StreamHandle>());
}

void NavigationRequest::CreateNavigationHandle() {
  // TODO(nasko): Update the NavigationHandle creation to ensure that the
  // proper values are specified for is_synchronous and is_srcdoc.
  navigation_handle_ =
      NavigationHandleImpl::Create(common_params_.url, frame_tree_node_,
                                   false,  // is_synchronous
                                   false,  // is_srcdoc
                                   common_params_.navigation_start);
}

void NavigationRequest::TransferNavigationHandleOwnership(
    RenderFrameHostImpl* render_frame_host) {
  render_frame_host->SetNavigationHandle(std::move(navigation_handle_));
}

void NavigationRequest::OnRequestRedirected(
    const net::RedirectInfo& redirect_info,
    const scoped_refptr<ResourceResponse>& response) {
  common_params_.url = redirect_info.new_url;
  begin_params_.method = redirect_info.new_method;
  common_params_.referrer.url = GURL(redirect_info.new_referrer);

  // TODO(clamy): Have CSP + security upgrade checks here.
  // TODO(clamy): Kill the renderer if FilterURL fails?

  // It's safe to use base::Unretained because this NavigationRequest owns the
  // NavigationHandle where the callback will be stored.
  // TODO(clamy): pass the real value for |is_external_protocol| if needed.
  navigation_handle_->WillRedirectRequest(
      common_params_.url, begin_params_.method == "POST",
      common_params_.referrer.url, false, response->head.headers,
      base::Bind(&NavigationRequest::OnRedirectChecksComplete,
                 base::Unretained(this)));
}

void NavigationRequest::OnResponseStarted(
    const scoped_refptr<ResourceResponse>& response,
    scoped_ptr<StreamHandle> body) {
  DCHECK(state_ == STARTED);
  state_ = RESPONSE_STARTED;

  // Update the service worker params of the request params.
  request_params_.should_create_service_worker =
      (frame_tree_node_->pending_sandbox_flags() &
       blink::WebSandboxFlags::Origin) != blink::WebSandboxFlags::Origin;
  if (navigation_handle_->service_worker_handle()) {
    request_params_.service_worker_provider_id =
        navigation_handle_->service_worker_handle()
            ->service_worker_provider_host_id();
  }

  frame_tree_node_->navigator()->CommitNavigation(
      frame_tree_node_, response.get(), std::move(body));
}

void NavigationRequest::OnRequestFailed(bool has_stale_copy_in_cache,
                                        int net_error) {
  DCHECK(state_ == STARTED);
  state_ = FAILED;
  navigation_handle_->set_net_error_code(static_cast<net::Error>(net_error));
  frame_tree_node_->navigator()->FailedNavigation(
      frame_tree_node_, has_stale_copy_in_cache, net_error);
}

void NavigationRequest::OnRequestStarted(base::TimeTicks timestamp) {
  if (frame_tree_node_->IsMainFrame()) {
    TRACE_EVENT_ASYNC_END_WITH_TIMESTAMP0(
        "navigation", "Navigation timeToNetworkStack", navigation_handle_.get(),
        timestamp.ToInternalValue());
  }

  frame_tree_node_->navigator()->LogResourceRequestTime(timestamp,
                                                        common_params_.url);
}

void NavigationRequest::OnStartChecksComplete(
    NavigationThrottle::ThrottleCheckResult result) {
  CHECK(result != NavigationThrottle::DEFER);

  // Abort the request if needed. This will destroy the NavigationRequest.
  if (result == NavigationThrottle::CANCEL_AND_IGNORE ||
      result == NavigationThrottle::CANCEL) {
    // TODO(clamy): distinguish between CANCEL and CANCEL_AND_IGNORE.
    frame_tree_node_->ResetNavigationRequest(false);
    return;
  }

  InitializeServiceWorkerHandleIfNeeded();
  loader_ = NavigationURLLoader::Create(
      frame_tree_node_->navigator()->GetController()->GetBrowserContext(),
      std::move(info_), navigation_handle_->service_worker_handle(), this);
}

void NavigationRequest::OnRedirectChecksComplete(
    NavigationThrottle::ThrottleCheckResult result) {
  CHECK(result != NavigationThrottle::DEFER);

  // Abort the request if needed. This will destroy the NavigationRequest.
  if (result == NavigationThrottle::CANCEL_AND_IGNORE ||
      result == NavigationThrottle::CANCEL) {
    // TODO(clamy): distinguish between CANCEL and CANCEL_AND_IGNORE.
    frame_tree_node_->ResetNavigationRequest(false);
    return;
  }

  loader_->FollowRedirect();
}

void NavigationRequest::InitializeServiceWorkerHandleIfNeeded() {
  // Only initialize the ServiceWorkerNavigationHandle if it can be created for
  // this frame.
  bool can_create_service_worker =
      (frame_tree_node_->pending_sandbox_flags() &
       blink::WebSandboxFlags::Origin) != blink::WebSandboxFlags::Origin;
  if (!can_create_service_worker)
    return;

  // Use the SiteInstance of the navigating RenderFrameHost to get access to
  // the StoragePartition. Using the url of the navigation will result in a
  // wrong StoragePartition being picked when a WebView is navigating.
  RenderFrameHostImpl* navigating_frame_host =
      frame_tree_node_->render_manager()->speculative_frame_host();
  if (!navigating_frame_host)
    navigating_frame_host = frame_tree_node_->current_frame_host();
  DCHECK(navigating_frame_host);

  BrowserContext* browser_context =
      frame_tree_node_->navigator()->GetController()->GetBrowserContext();
  StoragePartition* partition = BrowserContext::GetStoragePartition(
      browser_context, navigating_frame_host->GetSiteInstance());
  DCHECK(partition);

  ServiceWorkerContextWrapper* service_worker_context =
      static_cast<ServiceWorkerContextWrapper*>(
          partition->GetServiceWorkerContext());
  navigation_handle_->InitServiceWorkerHandle(service_worker_context);
}

}  // namespace content
