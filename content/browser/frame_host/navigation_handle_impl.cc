// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/navigation_handle_impl.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "content/browser/appcache/appcache_navigation_handle.h"
#include "content/browser/appcache/appcache_service_impl.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/frame_host/debug_urls.h"
#include "content/browser/frame_host/navigation_controller_impl.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/frame_host/navigator_delegate.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_navigation_handle.h"
#include "content/common/frame_messages.h"
#include "content/public/browser/navigation_ui_data.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/resource_request_body.h"

namespace content {

namespace {

// Use this to get a new unique ID for a NavigationHandle during construction.
// The returned ID is guaranteed to be nonzero (zero is the "no ID" indicator).
int64_t CreateUniqueHandleID() {
  static int64_t unique_id_counter = 0;
  return ++unique_id_counter;
}

}  // namespace

NavigationHandleImpl::NavigationHandleImpl(
    NavigationRequest* navigation_request,
    const std::vector<GURL>& redirect_chain,
    int pending_nav_entry_id,
    net::HttpRequestHeaders request_headers,
    const Referrer& sanitized_referrer)
    : navigation_request_(navigation_request),
      net_error_code_(net::OK),
      was_redirected_(false),
      request_headers_(std::move(request_headers)),
      pending_nav_entry_id_(pending_nav_entry_id),
      navigation_id_(CreateUniqueHandleID()),
      redirect_chain_(redirect_chain),
      reload_type_(ReloadType::NONE),
      restore_type_(RestoreType::NONE),
      weak_factory_(this) {
  const GURL& url = navigation_request_->common_params().url;
  TRACE_EVENT_ASYNC_BEGIN2("navigation", "NavigationHandle", this,
                           "frame_tree_node",
                           frame_tree_node()->frame_tree_node_id(), "url",
                           url.possibly_invalid_spec());
  DCHECK(!navigation_request_->common_params().navigation_start.is_null());
  DCHECK(!IsRendererDebugURL(url));

  if (redirect_chain_.empty())
    redirect_chain_.push_back(url);

  // Mirrors the logic in RenderFrameImpl::SendDidCommitProvisionalLoad.
  if (navigation_request_->common_params().transition &
      ui::PAGE_TRANSITION_CLIENT_REDIRECT) {
    // If the page contained a client redirect (meta refresh,
    // document.location), set the referrer appropriately.
    sanitized_referrer_ =
        Referrer(redirect_chain_[0], sanitized_referrer.policy);
  } else {
    sanitized_referrer_ = sanitized_referrer;
  }

  // Try to match this with a pending NavigationEntry if possible.  Note that
  // the NavigationController itself may be gone if this is a navigation inside
  // an interstitial and the interstitial is asynchronously deleting itself due
  // to its tab closing.
  NavigationControllerImpl* nav_controller =
      static_cast<NavigationControllerImpl*>(
          frame_tree_node()->navigator()->GetController());
  if (pending_nav_entry_id_ && nav_controller) {
    NavigationEntryImpl* nav_entry =
        nav_controller->GetEntryWithUniqueID(pending_nav_entry_id_);
    if (!nav_entry &&
        nav_controller->GetPendingEntry() &&
        nav_controller->GetPendingEntry()->GetUniqueID() ==
            pending_nav_entry_id_) {
      nav_entry = nav_controller->GetPendingEntry();
    }

    if (nav_entry) {
      reload_type_ = nav_entry->reload_type();
      restore_type_ = nav_entry->restore_type();
    }
  }

#if defined(OS_ANDROID)
  navigation_handle_proxy_ = std::make_unique<NavigationHandleProxy>(this);
#endif

  if (IsInMainFrame()) {
    TRACE_EVENT_ASYNC_BEGIN_WITH_TIMESTAMP1(
        "navigation", "Navigation StartToCommit", this,
        navigation_request_->common_params().navigation_start, "Initial URL",
        url.spec());
  }

  if (IsSameDocument()) {
    TRACE_EVENT_ASYNC_STEP_INTO0("navigation", "NavigationHandle", this,
                                 "Same document");
  }
}

NavigationHandleImpl::~NavigationHandleImpl() {
#if defined(OS_ANDROID)
  navigation_handle_proxy_->DidFinish();
#endif

  GetDelegate()->DidFinishNavigation(this);

  if (IsInMainFrame()) {
    TRACE_EVENT_ASYNC_END2("navigation", "Navigation StartToCommit", this,
                           "URL",
                           navigation_request_->common_params().url.spec(),
                           "Net Error Code", net_error_code_);
  }
  TRACE_EVENT_ASYNC_END0("navigation", "NavigationHandle", this);
}

NavigatorDelegate* NavigationHandleImpl::GetDelegate() const {
  return frame_tree_node()->navigator()->GetDelegate();
}

int64_t NavigationHandleImpl::GetNavigationId() {
  return navigation_id_;
}

const GURL& NavigationHandleImpl::GetURL() {
  return navigation_request_->common_params().url;
}

SiteInstanceImpl* NavigationHandleImpl::GetStartingSiteInstance() {
  return navigation_request_->starting_site_instance();
}

bool NavigationHandleImpl::IsInMainFrame() {
  return frame_tree_node()->IsMainFrame();
}

bool NavigationHandleImpl::IsParentMainFrame() {
  if (frame_tree_node()->parent())
    return frame_tree_node()->parent()->IsMainFrame();

  return false;
}

bool NavigationHandleImpl::IsRendererInitiated() {
  return !navigation_request_->browser_initiated();
}

bool NavigationHandleImpl::WasServerRedirect() {
  return was_redirected_;
}

const std::vector<GURL>& NavigationHandleImpl::GetRedirectChain() {
  return redirect_chain_;
}

int NavigationHandleImpl::GetFrameTreeNodeId() {
  return frame_tree_node()->frame_tree_node_id();
}

RenderFrameHostImpl* NavigationHandleImpl::GetParentFrame() {
  if (IsInMainFrame())
    return nullptr;

  return frame_tree_node()->parent()->current_frame_host();
}

base::TimeTicks NavigationHandleImpl::NavigationStart() {
  return navigation_request_->common_params().navigation_start;
}

base::TimeTicks NavigationHandleImpl::NavigationInputStart() {
  return navigation_request_->common_params().input_start;
}

bool NavigationHandleImpl::IsPost() {
  return navigation_request_->common_params().method == "POST";
}

const scoped_refptr<network::ResourceRequestBody>&
NavigationHandleImpl::GetResourceRequestBody() {
  return navigation_request_->common_params().post_data;
}

const Referrer& NavigationHandleImpl::GetReferrer() {
  return sanitized_referrer_;
}

bool NavigationHandleImpl::HasUserGesture() {
  return navigation_request_->common_params().has_user_gesture;
}

ui::PageTransition NavigationHandleImpl::GetPageTransition() {
  return navigation_request_->common_params().transition;
}

const NavigationUIData* NavigationHandleImpl::GetNavigationUIData() {
  return navigation_request_->navigation_ui_data();
}

bool NavigationHandleImpl::IsExternalProtocol() {
  return !GetContentClient()->browser()->IsHandledURL(GetURL());
}

net::Error NavigationHandleImpl::GetNetErrorCode() {
  return net_error_code_;
}

RenderFrameHostImpl* NavigationHandleImpl::GetRenderFrameHost() {
  return navigation_request_->render_frame_host();
}

bool NavigationHandleImpl::IsSameDocument() {
  return navigation_request_->IsSameDocument();
}

const net::HttpRequestHeaders& NavigationHandleImpl::GetRequestHeaders() {
  return request_headers_;
}

void NavigationHandleImpl::RemoveRequestHeader(const std::string& header_name) {
  DCHECK(state() == NavigationRequest::PROCESSING_WILL_REDIRECT_REQUEST ||
         state() == NavigationRequest::WILL_REDIRECT_REQUEST);
  removed_request_headers_.push_back(header_name);
}

void NavigationHandleImpl::SetRequestHeader(const std::string& header_name,
                                            const std::string& header_value) {
  DCHECK(state() == NavigationRequest::INITIAL ||
         state() == NavigationRequest::PROCESSING_WILL_START_REQUEST ||
         state() == NavigationRequest::PROCESSING_WILL_REDIRECT_REQUEST ||
         state() == NavigationRequest::WILL_START_REQUEST ||
         state() == NavigationRequest::WILL_REDIRECT_REQUEST);
  modified_request_headers_.SetHeader(header_name, header_value);
}

const net::HttpResponseHeaders* NavigationHandleImpl::GetResponseHeaders() {
  if (response_headers_for_testing_)
    return response_headers_for_testing_.get();
  return navigation_request_->response()
             ? navigation_request_->response()->head.headers.get()
             : nullptr;
}

net::HttpResponseInfo::ConnectionInfo
NavigationHandleImpl::GetConnectionInfo() {
  return navigation_request_->response()
             ? navigation_request_->response()->head.connection_info
             : net::HttpResponseInfo::ConnectionInfo();
}

const base::Optional<net::SSLInfo> NavigationHandleImpl::GetSSLInfo() {
  return navigation_request_->ssl_info();
}

bool NavigationHandleImpl::IsWaitingToCommit() {
  return state() == NavigationRequest::READY_TO_COMMIT;
}

bool NavigationHandleImpl::HasCommitted() {
  return state() == NavigationRequest::DID_COMMIT ||
         state() == NavigationRequest::DID_COMMIT_ERROR_PAGE;
}

bool NavigationHandleImpl::IsErrorPage() {
  return state() == NavigationRequest::DID_COMMIT_ERROR_PAGE;
}

bool NavigationHandleImpl::HasSubframeNavigationEntryCommitted() {
  return navigation_request_->subframe_entry_committed();
}

bool NavigationHandleImpl::DidReplaceEntry() {
  return navigation_request_->did_replace_entry();
}

bool NavigationHandleImpl::ShouldUpdateHistory() {
  return navigation_request_->should_update_history();
}

const GURL& NavigationHandleImpl::GetPreviousURL() {
  return navigation_request_->previous_url();
}

net::IPEndPoint NavigationHandleImpl::GetSocketAddress() {
  // This is CANCELING because although the data comes in after
  // WILL_PROCESS_RESPONSE, it's possible for the navigation to be cancelled
  // after and the caller might want this value.
  DCHECK_GE(state(), NavigationRequest::CANCELING);
  return navigation_request_->response()
             ? navigation_request_->response()->head.remote_endpoint
             : net::IPEndPoint();
}

void NavigationHandleImpl::RegisterThrottleForTesting(
    std::unique_ptr<NavigationThrottle> navigation_throttle) {
  navigation_request_->RegisterThrottleForTesting(
      std::move(navigation_throttle));
}

#if defined(OS_ANDROID)
base::android::ScopedJavaGlobalRef<jobject>
NavigationHandleImpl::java_navigation_handle() {
  return navigation_handle_proxy_->java_navigation_handle();
}
#endif

bool NavigationHandleImpl::IsDeferredForTesting() {
  return navigation_request_->IsDeferredForTesting();
}

bool NavigationHandleImpl::WasStartedFromContextMenu() {
  return navigation_request_->common_params().started_from_context_menu;
}

const GURL& NavigationHandleImpl::GetSearchableFormURL() {
  return navigation_request_->begin_params()->searchable_form_url;
}

const std::string& NavigationHandleImpl::GetSearchableFormEncoding() {
  return navigation_request_->begin_params()->searchable_form_encoding;
}

ReloadType NavigationHandleImpl::GetReloadType() {
  return reload_type_;
}

RestoreType NavigationHandleImpl::GetRestoreType() {
  return restore_type_;
}

const GURL& NavigationHandleImpl::GetBaseURLForDataURL() {
  return navigation_request_->common_params().base_url_for_data_url;
}

NavigationData* NavigationHandleImpl::GetNavigationData() {
  return navigation_data_.get();
}

void NavigationHandleImpl::RegisterSubresourceOverride(
    mojom::TransferrableURLLoaderPtr transferrable_loader) {
  if (!transferrable_loader)
    return;

  navigation_request_->RegisterSubresourceOverride(
      std::move(transferrable_loader));
}

const GlobalRequestID& NavigationHandleImpl::GetGlobalRequestID() {
  DCHECK_GE(state(), NavigationRequest::PROCESSING_WILL_PROCESS_RESPONSE);
  return navigation_request_->request_id();
}

bool NavigationHandleImpl::IsDownload() {
  return navigation_request_->is_download();
}

bool NavigationHandleImpl::IsFormSubmission() {
  return navigation_request_->begin_params()->is_form_submission;
}

const std::string& NavigationHandleImpl::GetHrefTranslate() {
  return navigation_request_->common_params().href_translate;
}

void NavigationHandleImpl::CallResumeForTesting() {
  navigation_request_->CallResumeForTesting();
}

const base::Optional<url::Origin>& NavigationHandleImpl::GetInitiatorOrigin() {
  return navigation_request_->common_params().initiator_origin;
}

bool NavigationHandleImpl::IsSameProcess() {
  return navigation_request_->is_same_process();
}

int NavigationHandleImpl::GetNavigationEntryOffset() {
  return navigation_request_->navigation_entry_offset();
}

bool NavigationHandleImpl::IsSignedExchangeInnerResponse() {
  return navigation_request_->response()
             ? navigation_request_->response()
                   ->head.is_signed_exchange_inner_response
             : false;
}

bool NavigationHandleImpl::WasResponseCached() {
  return navigation_request_->response()
             ? navigation_request_->response()->head.was_fetched_via_cache
             : false;
}

const net::ProxyServer& NavigationHandleImpl::GetProxyServer() {
  return proxy_server_;
}

void NavigationHandleImpl::InitServiceWorkerHandle(
    ServiceWorkerContextWrapper* service_worker_context) {
  service_worker_handle_.reset(
      new ServiceWorkerNavigationHandle(service_worker_context));
}

void NavigationHandleImpl::UpdateStateFollowingRedirect(
    const GURL& new_referrer_url,
    ThrottleChecksFinishedCallback callback) {
  // The navigation should not redirect to a "renderer debug" url. It should be
  // blocked in NavigationRequest::OnRequestRedirected or in
  // ResourceLoader::OnReceivedRedirect.
  // Note: the call to GetURL below returns the post-redirect URL.
  // See https://crbug.com/728398.
  CHECK(!IsRendererDebugURL(GetURL()));

  // Update the navigation parameters.
  if (!(GetPageTransition() & ui::PAGE_TRANSITION_CLIENT_REDIRECT)) {
    sanitized_referrer_.url = new_referrer_url;
    sanitized_referrer_ =
        Referrer::SanitizeForRequest(GetURL(), sanitized_referrer_);
  }

  was_redirected_ = true;
  redirect_chain_.push_back(GetURL());

  navigation_request_->set_handle_state(
      NavigationRequest::PROCESSING_WILL_REDIRECT_REQUEST);

#if defined(OS_ANDROID)
  navigation_handle_proxy_->DidRedirect();
#endif

  complete_callback_ = std::move(callback);
}

void NavigationHandleImpl::RunCompleteCallback(
    NavigationThrottle::ThrottleCheckResult result) {
  DCHECK(result.action() != NavigationThrottle::DEFER);

  ThrottleChecksFinishedCallback callback = std::move(complete_callback_);
  complete_callback_.Reset();

  if (!complete_callback_for_testing_.is_null())
    std::move(complete_callback_for_testing_).Run(result);

  if (!callback.is_null())
    std::move(callback).Run(result);

  // No code after running the callback, as it might have resulted in our
  // destruction.
}

}  // namespace content
