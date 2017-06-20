// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/navigation_handle_impl.h"

#include <iterator>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "content/browser/appcache/appcache_navigation_handle.h"
#include "content/browser/appcache/appcache_service_impl.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/browser/frame_host/ancestor_throttle.h"
#include "content/browser/frame_host/data_url_navigation_throttle.h"
#include "content/browser/frame_host/debug_urls.h"
#include "content/browser/frame_host/form_submission_throttle.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/mixed_content_navigation_throttle.h"
#include "content/browser/frame_host/navigation_controller_impl.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/frame_host/navigator_delegate.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_navigation_handle.h"
#include "content/common/child_process_host_impl.h"
#include "content/common/frame_messages.h"
#include "content/common/resource_request_body_impl.h"
#include "content/common/site_isolation_policy.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/navigation_ui_data.h"
#include "content/public/browser/site_instance.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "net/base/net_errors.h"
#include "net/url_request/redirect_info.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace content {

namespace {

void UpdateThrottleCheckResult(
    NavigationThrottle::ThrottleCheckResult* to_update,
    NavigationThrottle::ThrottleCheckResult result) {
  *to_update = result;
}

void NotifyAbandonedTransferNavigation(const GlobalRequestID& id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (ResourceDispatcherHostImpl* rdh = ResourceDispatcherHostImpl::Get())
    rdh->CancelRequest(id.child_id, id.request_id);
}

}  // namespace

// static
std::unique_ptr<NavigationHandleImpl> NavigationHandleImpl::Create(
    const GURL& url,
    const std::vector<GURL>& redirect_chain,
    FrameTreeNode* frame_tree_node,
    bool is_renderer_initiated,
    bool is_same_document,
    const base::TimeTicks& navigation_start,
    int pending_nav_entry_id,
    bool started_from_context_menu,
    CSPDisposition should_check_main_world_csp,
    bool is_form_submission) {
  return std::unique_ptr<NavigationHandleImpl>(new NavigationHandleImpl(
      url, redirect_chain, frame_tree_node, is_renderer_initiated,
      is_same_document, navigation_start, pending_nav_entry_id,
      started_from_context_menu, should_check_main_world_csp,
      is_form_submission));
}

NavigationHandleImpl::NavigationHandleImpl(
    const GURL& url,
    const std::vector<GURL>& redirect_chain,
    FrameTreeNode* frame_tree_node,
    bool is_renderer_initiated,
    bool is_same_document,
    const base::TimeTicks& navigation_start,
    int pending_nav_entry_id,
    bool started_from_context_menu,
    CSPDisposition should_check_main_world_csp,
    bool is_form_submission)
    : url_(url),
      has_user_gesture_(false),
      transition_(ui::PAGE_TRANSITION_LINK),
      is_external_protocol_(false),
      net_error_code_(net::OK),
      render_frame_host_(nullptr),
      is_renderer_initiated_(is_renderer_initiated),
      is_same_document_(is_same_document),
      was_redirected_(false),
      did_replace_entry_(false),
      should_update_history_(false),
      subframe_entry_committed_(false),
      connection_info_(net::HttpResponseInfo::CONNECTION_INFO_UNKNOWN),
      original_url_(url),
      state_(INITIAL),
      is_transferring_(false),
      frame_tree_node_(frame_tree_node),
      next_index_(0),
      navigation_start_(navigation_start),
      pending_nav_entry_id_(pending_nav_entry_id),
      request_context_type_(REQUEST_CONTEXT_TYPE_UNSPECIFIED),
      mixed_content_context_type_(
          blink::WebMixedContentContextType::kBlockable),
      should_replace_current_entry_(false),
      redirect_chain_(redirect_chain),
      is_download_(false),
      is_stream_(false),
      started_from_context_menu_(started_from_context_menu),
      reload_type_(ReloadType::NONE),
      restore_type_(RestoreType::NONE),
      navigation_type_(NAVIGATION_TYPE_UNKNOWN),
      should_check_main_world_csp_(should_check_main_world_csp),
      is_form_submission_(is_form_submission),
      expected_render_process_host_id_(ChildProcessHost::kInvalidUniqueID),
      weak_factory_(this) {
  TRACE_EVENT_ASYNC_BEGIN2("navigation", "NavigationHandle", this,
                           "frame_tree_node",
                           frame_tree_node_->frame_tree_node_id(), "url",
                           url_.possibly_invalid_spec());
  DCHECK(!navigation_start.is_null());

  site_url_ = SiteInstance::GetSiteForURL(frame_tree_node_->current_frame_host()
                                              ->GetSiteInstance()
                                              ->GetBrowserContext(),
                                          url_);
  if (redirect_chain_.empty())
    redirect_chain_.push_back(url);

  starting_site_instance_ =
      frame_tree_node_->current_frame_host()->GetSiteInstance();

  // Try to match this with a pending NavigationEntry if possible.  Note that
  // the NavigationController itself may be gone if this is a navigation inside
  // an interstitial and the interstitial is asynchronously deleting itself due
  // to its tab closing.
  NavigationControllerImpl* nav_controller =
      static_cast<NavigationControllerImpl*>(
          frame_tree_node_->navigator()->GetController());
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
      base_url_for_data_url_ = nav_entry->GetBaseURLForDataURL();
    }
  }

  if (!IsRendererDebugURL(url_))
    GetDelegate()->DidStartNavigation(this);

  if (IsInMainFrame()) {
    TRACE_EVENT_ASYNC_BEGIN_WITH_TIMESTAMP1(
        "navigation", "Navigation StartToCommit", this,
        navigation_start, "Initial URL", url_.spec());
  }

  if (is_same_document_) {
    TRACE_EVENT_ASYNC_STEP_INTO0("navigation", "NavigationHandle", this,
                                 "Same document");
  }
}

NavigationHandleImpl::~NavigationHandleImpl() {
  // Inform the RenderProcessHost to no longer expect a navigation.
  if (expected_render_process_host_id_ != ChildProcessHost::kInvalidUniqueID) {
    RenderProcessHost* process =
        RenderProcessHost::FromID(expected_render_process_host_id_);
    if (process) {
      RenderProcessHostImpl::RemoveExpectedNavigationToSite(
          frame_tree_node_->navigator()->GetController()->GetBrowserContext(),
          process, site_url_);
    }
  }

  // Transfer requests that have not matched up with another navigation request
  // from the renderer need to be cleaned up. These are marked as protected in
  // the RDHI, so they do not get cancelled when frames are destroyed.
  if (is_transferring()) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&NotifyAbandonedTransferNavigation, GetGlobalRequestID()));
  }

  if (!IsRendererDebugURL(url_))
    GetDelegate()->DidFinishNavigation(this);

  // Cancel the navigation on the IO thread if the NavigationHandle is being
  // destroyed in the middle of the NavigationThrottles checks.
  if (!IsBrowserSideNavigationEnabled() && !complete_callback_.is_null())
    RunCompleteCallback(NavigationThrottle::CANCEL_AND_IGNORE);

  if (IsInMainFrame()) {
    TRACE_EVENT_ASYNC_END2("navigation", "Navigation StartToCommit", this,
                           "URL", url_.spec(), "Net Error Code",
                           net_error_code_);
  }
  TRACE_EVENT_ASYNC_END0("navigation", "NavigationHandle", this);
}

NavigatorDelegate* NavigationHandleImpl::GetDelegate() const {
  return frame_tree_node_->navigator()->GetDelegate();
}

const GURL& NavigationHandleImpl::GetURL() {
  return url_;
}

SiteInstance* NavigationHandleImpl::GetStartingSiteInstance() {
  return starting_site_instance_.get();
}

bool NavigationHandleImpl::IsInMainFrame() {
  return frame_tree_node_->IsMainFrame();
}

bool NavigationHandleImpl::IsParentMainFrame() {
  if (frame_tree_node_->parent())
    return frame_tree_node_->parent()->IsMainFrame();

  return false;
}

bool NavigationHandleImpl::IsRendererInitiated() {
  return is_renderer_initiated_;
}

bool NavigationHandleImpl::WasServerRedirect() {
  return was_redirected_;
}

const std::vector<GURL>& NavigationHandleImpl::GetRedirectChain() {
  return redirect_chain_;
}

int NavigationHandleImpl::GetFrameTreeNodeId() {
  return frame_tree_node_->frame_tree_node_id();
}

RenderFrameHostImpl* NavigationHandleImpl::GetParentFrame() {
  if (frame_tree_node_->IsMainFrame())
    return nullptr;

  return frame_tree_node_->parent()->current_frame_host();
}

const base::TimeTicks& NavigationHandleImpl::NavigationStart() {
  return navigation_start_;
}

bool NavigationHandleImpl::IsPost() {
  CHECK_NE(INITIAL, state_)
      << "This accessor should not be called before the request is started.";
  return method_ == "POST";
}

const Referrer& NavigationHandleImpl::GetReferrer() {
  CHECK_NE(INITIAL, state_)
      << "This accessor should not be called before the request is started.";
  return sanitized_referrer_;
}

bool NavigationHandleImpl::HasUserGesture() {
  CHECK_NE(INITIAL, state_)
      << "This accessor should not be called before the request is started.";
  return has_user_gesture_;
}

ui::PageTransition NavigationHandleImpl::GetPageTransition() {
  CHECK_NE(INITIAL, state_)
      << "This accessor should not be called before the request is started.";
  return transition_;
}

bool NavigationHandleImpl::IsExternalProtocol() {
  CHECK_NE(INITIAL, state_)
      << "This accessor should not be called before the request is started.";
  return is_external_protocol_;
}

net::Error NavigationHandleImpl::GetNetErrorCode() {
  return net_error_code_;
}

RenderFrameHostImpl* NavigationHandleImpl::GetRenderFrameHost() {
  // TODO(mkwst): Change this to check against 'READY_TO_COMMIT' once
  // ReadyToCommitNavigation is available whether or not PlzNavigate is
  // enabled. https://crbug.com/621856
  CHECK_GE(state_, WILL_PROCESS_RESPONSE)
      << "This accessor should only be called after a response has been "
         "delivered for processing.";
  return render_frame_host_;
}

bool NavigationHandleImpl::IsSameDocument() {
  return is_same_document_;
}

const net::HttpResponseHeaders* NavigationHandleImpl::GetResponseHeaders() {
  return response_headers_.get();
}

net::HttpResponseInfo::ConnectionInfo
NavigationHandleImpl::GetConnectionInfo() {
  return connection_info_;
}

bool NavigationHandleImpl::HasCommitted() {
  return state_ == DID_COMMIT || state_ == DID_COMMIT_ERROR_PAGE;
}

bool NavigationHandleImpl::IsErrorPage() {
  return state_ == DID_COMMIT_ERROR_PAGE;
}

bool NavigationHandleImpl::HasSubframeNavigationEntryCommitted() {
  DCHECK(!IsInMainFrame());
  DCHECK(state_ == DID_COMMIT || state_ == DID_COMMIT_ERROR_PAGE);
  return subframe_entry_committed_;
}

bool NavigationHandleImpl::DidReplaceEntry() {
  DCHECK(state_ == DID_COMMIT || state_ == DID_COMMIT_ERROR_PAGE);
  return did_replace_entry_;
}

bool NavigationHandleImpl::ShouldUpdateHistory() {
  DCHECK(state_ == DID_COMMIT || state_ == DID_COMMIT_ERROR_PAGE);
  return should_update_history_;
}

const GURL& NavigationHandleImpl::GetPreviousURL() {
  DCHECK(state_ == DID_COMMIT || state_ == DID_COMMIT_ERROR_PAGE);
  return previous_url_;
}

net::HostPortPair NavigationHandleImpl::GetSocketAddress() {
  DCHECK(state_ == DID_COMMIT || state_ == DID_COMMIT_ERROR_PAGE);
  return socket_address_;
}

void NavigationHandleImpl::Resume() {
  if (state_ != DEFERRING_START && state_ != DEFERRING_REDIRECT &&
      state_ != DEFERRING_RESPONSE) {
    return;
  }
  TRACE_EVENT_ASYNC_STEP_INTO0("navigation", "NavigationHandle", this,
                               "Resume");

  NavigationThrottle::ThrottleCheckResult result = NavigationThrottle::DEFER;
  if (state_ == DEFERRING_START) {
    result = CheckWillStartRequest();
  } else if (state_ == DEFERRING_REDIRECT) {
    result = CheckWillRedirectRequest();
  } else {
    result = CheckWillProcessResponse();

    // If the navigation is about to proceed after having been deferred while
    // processing the response, then it's ready to commit. Determine which
    // RenderFrameHost should render the response, based on its site (after any
    // redirects).
    // Note: if MaybeTransferAndProceed returns false, this means that this
    // NavigationHandle was deleted, so return immediately.
    if (result == NavigationThrottle::PROCEED && !MaybeTransferAndProceed())
      return;
  }

  if (result != NavigationThrottle::DEFER) {
    TRACE_EVENT_ASYNC_STEP_INTO0("navigation", "NavigationHandle", this,
                                 "Resuming");
    RunCompleteCallback(result);
  }
}

void NavigationHandleImpl::CancelDeferredNavigation(
    NavigationThrottle::ThrottleCheckResult result) {
  DCHECK(state_ == DEFERRING_START ||
         state_ == DEFERRING_REDIRECT ||
         state_ == DEFERRING_RESPONSE);
  DCHECK(result == NavigationThrottle::CANCEL_AND_IGNORE ||
         result == NavigationThrottle::CANCEL ||
         result == NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE);
  DCHECK(result != NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE ||
         state_ == DEFERRING_START ||
         (state_ == DEFERRING_REDIRECT && IsBrowserSideNavigationEnabled()));

  if (result == NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE)
    frame_tree_node_->SetCollapsed(true);

  TRACE_EVENT_ASYNC_STEP_INTO0("navigation", "NavigationHandle", this,
                               "CancelDeferredNavigation");
  state_ = CANCELING;
  RunCompleteCallback(result);
}

void NavigationHandleImpl::RegisterThrottleForTesting(
    std::unique_ptr<NavigationThrottle> navigation_throttle) {
  throttles_.push_back(std::move(navigation_throttle));
}

NavigationThrottle::ThrottleCheckResult
NavigationHandleImpl::CallWillStartRequestForTesting(
    bool is_post,
    const Referrer& sanitized_referrer,
    bool has_user_gesture,
    ui::PageTransition transition,
    bool is_external_protocol) {
  NavigationThrottle::ThrottleCheckResult result = NavigationThrottle::DEFER;

  scoped_refptr<ResourceRequestBodyImpl> resource_request_body;
  std::string method = "GET";
  if (is_post) {
    method = "POST";

    std::string body = "test=body";
    resource_request_body = new ResourceRequestBodyImpl();
    resource_request_body->AppendBytes(body.data(), body.size());
  }

  WillStartRequest(method, resource_request_body, sanitized_referrer,
                   has_user_gesture, transition, is_external_protocol,
                   REQUEST_CONTEXT_TYPE_LOCATION,
                   blink::WebMixedContentContextType::kBlockable,
                   base::Bind(&UpdateThrottleCheckResult, &result));

  // Reset the callback to ensure it will not be called later.
  complete_callback_.Reset();
  return result;
}

NavigationThrottle::ThrottleCheckResult
NavigationHandleImpl::CallWillRedirectRequestForTesting(
    const GURL& new_url,
    bool new_method_is_post,
    const GURL& new_referrer_url,
    bool new_is_external_protocol) {
  NavigationThrottle::ThrottleCheckResult result = NavigationThrottle::DEFER;
  WillRedirectRequest(new_url, new_method_is_post ? "POST" : "GET",
                      new_referrer_url, new_is_external_protocol,
                      scoped_refptr<net::HttpResponseHeaders>(),
                      net::HttpResponseInfo::CONNECTION_INFO_UNKNOWN,
                      base::Bind(&UpdateThrottleCheckResult, &result));

  // Reset the callback to ensure it will not be called later.
  complete_callback_.Reset();
  return result;
}

NavigationThrottle::ThrottleCheckResult
NavigationHandleImpl::CallWillProcessResponseForTesting(
    content::RenderFrameHost* render_frame_host,
    const std::string& raw_response_headers) {
  scoped_refptr<net::HttpResponseHeaders> headers =
      new net::HttpResponseHeaders(raw_response_headers);
  NavigationThrottle::ThrottleCheckResult result = NavigationThrottle::DEFER;
  WillProcessResponse(static_cast<RenderFrameHostImpl*>(render_frame_host),
                      headers, net::HttpResponseInfo::CONNECTION_INFO_UNKNOWN,
                      SSLStatus(), GlobalRequestID(), false, false, false,
                      base::Closure(),
                      base::Bind(&UpdateThrottleCheckResult, &result));

  // Reset the callback to ensure it will not be called later.
  complete_callback_.Reset();
  return result;
}

void NavigationHandleImpl::CallDidCommitNavigationForTesting(const GURL& url) {
  FrameHostMsg_DidCommitProvisionalLoad_Params params;

  params.nav_entry_id = 1;
  params.url = url;
  params.referrer = content::Referrer();
  params.transition = ui::PAGE_TRANSITION_TYPED;
  params.redirects = std::vector<GURL>();
  params.should_update_history = false;
  params.searchable_form_url = GURL();
  params.searchable_form_encoding = std::string();
  params.did_create_new_entry = false;
  params.gesture = NavigationGestureUser;
  params.was_within_same_document = false;
  params.method = "GET";
  params.page_state = PageState::CreateFromURL(url);
  params.contents_mime_type = std::string("text/html");

  DidCommitNavigation(params, true, false, GURL(), NAVIGATION_TYPE_NEW_PAGE,
                      render_frame_host_);
}

bool NavigationHandleImpl::WasStartedFromContextMenu() const {
  return started_from_context_menu_;
}

const GURL& NavigationHandleImpl::GetSearchableFormURL() {
  return searchable_form_url_;
}

const std::string& NavigationHandleImpl::GetSearchableFormEncoding() {
  return searchable_form_encoding_;
}

ReloadType NavigationHandleImpl::GetReloadType() {
  return reload_type_;
}

RestoreType NavigationHandleImpl::GetRestoreType() {
  return restore_type_;
}

const GURL& NavigationHandleImpl::GetBaseURLForDataURL() {
  return base_url_for_data_url_;
}

NavigationData* NavigationHandleImpl::GetNavigationData() {
  return navigation_data_.get();
}

const GlobalRequestID& NavigationHandleImpl::GetGlobalRequestID() {
  DCHECK(state_ == WILL_PROCESS_RESPONSE || state_ == DEFERRING_RESPONSE ||
         state_ == READY_TO_COMMIT);
  return request_id_;
}

void NavigationHandleImpl::InitServiceWorkerHandle(
    ServiceWorkerContextWrapper* service_worker_context) {
  DCHECK(IsBrowserSideNavigationEnabled());
  service_worker_handle_.reset(
      new ServiceWorkerNavigationHandle(service_worker_context));
}

void NavigationHandleImpl::InitAppCacheHandle(
    ChromeAppCacheService* appcache_service) {
  DCHECK(IsBrowserSideNavigationEnabled());
  appcache_handle_.reset(new AppCacheNavigationHandle(appcache_service));
}

void NavigationHandleImpl::WillStartRequest(
    const std::string& method,
    scoped_refptr<content::ResourceRequestBodyImpl> resource_request_body,
    const Referrer& sanitized_referrer,
    bool has_user_gesture,
    ui::PageTransition transition,
    bool is_external_protocol,
    RequestContextType request_context_type,
    blink::WebMixedContentContextType mixed_content_context_type,
    const ThrottleChecksFinishedCallback& callback) {
  TRACE_EVENT_ASYNC_STEP_INTO0("navigation", "NavigationHandle", this,
                               "WillStartRequest");
  if (method != "POST")
    DCHECK(!resource_request_body);

  // Update the navigation parameters.
  method_ = method;
  if (method_ == "POST")
    resource_request_body_ = resource_request_body;
  has_user_gesture_ = has_user_gesture;
  transition_ = transition;
  // Mirrors the logic in RenderFrameImpl::SendDidCommitProvisionalLoad.
  if (transition_ & ui::PAGE_TRANSITION_CLIENT_REDIRECT) {
    // If the page contained a client redirect (meta refresh,
    // document.location), set the referrer appropriately.
    sanitized_referrer_ =
        Referrer(redirect_chain_[0], sanitized_referrer.policy);
  } else {
    sanitized_referrer_ = sanitized_referrer;
  }
  is_external_protocol_ = is_external_protocol;
  request_context_type_ = request_context_type;
  mixed_content_context_type_ = mixed_content_context_type;
  state_ = WILL_SEND_REQUEST;
  complete_callback_ = callback;

  if (IsSelfReferentialURL()) {
    state_ = CANCELING;
    RunCompleteCallback(NavigationThrottle::CANCEL);
    return;
  }

  RegisterNavigationThrottles();

  if (IsBrowserSideNavigationEnabled())
    navigation_ui_data_ = GetDelegate()->GetNavigationUIData(this);

  // Notify each throttle of the request.
  NavigationThrottle::ThrottleCheckResult result = CheckWillStartRequest();

  // If the navigation is not deferred, run the callback.
  if (result != NavigationThrottle::DEFER) {
    TRACE_EVENT_ASYNC_STEP_INTO1("navigation", "NavigationHandle", this,
                                 "StartRequest", "result", result);
    RunCompleteCallback(result);
  }
}

void NavigationHandleImpl::WillRedirectRequest(
    const GURL& new_url,
    const std::string& new_method,
    const GURL& new_referrer_url,
    bool new_is_external_protocol,
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    net::HttpResponseInfo::ConnectionInfo connection_info,
    const ThrottleChecksFinishedCallback& callback) {
  TRACE_EVENT_ASYNC_STEP_INTO1("navigation", "NavigationHandle", this,
                               "WillRedirectRequest", "url",
                               new_url.possibly_invalid_spec());

  // Update the navigation parameters.
  url_ = new_url;
  method_ = new_method;
  UpdateSiteURL();

  if (!(transition_ & ui::PAGE_TRANSITION_CLIENT_REDIRECT)) {
    sanitized_referrer_.url = new_referrer_url;
    sanitized_referrer_ =
        Referrer::SanitizeForRequest(url_, sanitized_referrer_);
  }

  is_external_protocol_ = new_is_external_protocol;
  response_headers_ = response_headers;
  connection_info_ = connection_info;
  was_redirected_ = true;
  redirect_chain_.push_back(new_url);
  if (new_method != "POST")
    resource_request_body_ = nullptr;

  state_ = WILL_REDIRECT_REQUEST;
  complete_callback_ = callback;

  if (IsSelfReferentialURL()) {
    state_ = CANCELING;
    RunCompleteCallback(NavigationThrottle::CANCEL);
    return;
  }

  // Notify each throttle of the request.
  NavigationThrottle::ThrottleCheckResult result = CheckWillRedirectRequest();

  // If the navigation is not deferred, run the callback.
  if (result != NavigationThrottle::DEFER) {
    TRACE_EVENT_ASYNC_STEP_INTO1("navigation", "NavigationHandle", this,
                                 "RedirectRequest", "result", result);
    RunCompleteCallback(result);
  }
}

void NavigationHandleImpl::WillProcessResponse(
    RenderFrameHostImpl* render_frame_host,
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    net::HttpResponseInfo::ConnectionInfo connection_info,
    const SSLStatus& ssl_status,
    const GlobalRequestID& request_id,
    bool should_replace_current_entry,
    bool is_download,
    bool is_stream,
    const base::Closure& transfer_callback,
    const ThrottleChecksFinishedCallback& callback) {
  TRACE_EVENT_ASYNC_STEP_INTO0("navigation", "NavigationHandle", this,
                               "WillProcessResponse");

  DCHECK(!render_frame_host_ || render_frame_host_ == render_frame_host);
  render_frame_host_ = render_frame_host;
  response_headers_ = response_headers;
  connection_info_ = connection_info;
  request_id_ = request_id;
  should_replace_current_entry_ = should_replace_current_entry;
  is_download_ = is_download;
  is_stream_ = is_stream;
  state_ = WILL_PROCESS_RESPONSE;
  ssl_status_ = ssl_status;
  complete_callback_ = callback;
  transfer_callback_ = transfer_callback;

  // Notify each throttle of the response.
  NavigationThrottle::ThrottleCheckResult result = CheckWillProcessResponse();

  // If the navigation is done processing the response, then it's ready to
  // commit. Determine which RenderFrameHost should render the response, based
  // on its site (after any redirects).
  // Note: if MaybeTransferAndProceed returns false, this means that this
  // NavigationHandle was deleted, so return immediately.
  if (result == NavigationThrottle::PROCEED && !MaybeTransferAndProceed())
    return;

  // If the navigation is not deferred, run the callback.
  if (result != NavigationThrottle::DEFER) {
    TRACE_EVENT_ASYNC_STEP_INTO1("navigation", "NavigationHandle", this,
                                 "ProcessResponse", "result", result);
    RunCompleteCallback(result);
  }
}

void NavigationHandleImpl::ReadyToCommitNavigation(
    RenderFrameHostImpl* render_frame_host) {
  TRACE_EVENT_ASYNC_STEP_INTO0("navigation", "NavigationHandle", this,
                               "ReadyToCommitNavigation");

  DCHECK(!render_frame_host_ || render_frame_host_ == render_frame_host);
  render_frame_host_ = render_frame_host;
  state_ = READY_TO_COMMIT;
  ready_to_commit_time_ = base::TimeTicks::Now();

  // For back-forward navigations, record metrics.
  if ((transition_ & ui::PAGE_TRANSITION_FORWARD_BACK) && !IsSameDocument()) {
    bool is_same_process =
        render_frame_host_->GetProcess()->GetID() ==
        frame_tree_node_->current_frame_host()->GetProcess()->GetID();
    UMA_HISTOGRAM_BOOLEAN("Navigation.BackForward.IsSameProcess",
                          is_same_process);
    UMA_HISTOGRAM_TIMES("Navigation.BackForward.TimeToReadyToCommit",
                        ready_to_commit_time_ - navigation_start_);
  }

  if (IsBrowserSideNavigationEnabled())
    SetExpectedProcess(render_frame_host->GetProcess());

  if (!IsRendererDebugURL(url_) && !IsSameDocument())
    GetDelegate()->ReadyToCommitNavigation(this);
}

void NavigationHandleImpl::DidCommitNavigation(
    const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
    bool navigation_entry_committed,
    bool did_replace_entry,
    const GURL& previous_url,
    NavigationType navigation_type,
    RenderFrameHostImpl* render_frame_host) {
  DCHECK(!render_frame_host_ || render_frame_host_ == render_frame_host);
  DCHECK_EQ(frame_tree_node_, render_frame_host->frame_tree_node());
  CHECK_EQ(url_, params.url);

  did_replace_entry_ = did_replace_entry;
  method_ = params.method;
  has_user_gesture_ = (params.gesture == NavigationGestureUser);
  transition_ = params.transition;
  should_update_history_ = params.should_update_history;
  render_frame_host_ = render_frame_host;
  previous_url_ = previous_url;
  base_url_ = params.base_url;
  socket_address_ = params.socket_address;
  navigation_type_ = navigation_type;

  // For back-forward navigations, record metrics.
  if ((transition_ & ui::PAGE_TRANSITION_FORWARD_BACK) &&
      !ready_to_commit_time_.is_null() && !IsSameDocument()) {
    UMA_HISTOGRAM_TIMES("Navigation.BackForward.ReadyToCommitUntilCommit",
                        base::TimeTicks::Now() - ready_to_commit_time_);
  }

  DCHECK(!IsInMainFrame() || navigation_entry_committed)
      << "Only subframe navigations can get here without changing the "
      << "NavigationEntry";
  subframe_entry_committed_ = navigation_entry_committed;

  // If an error page reloads, net_error_code might be 200 but we still want to
  // count it as an error page.
  if (params.base_url.spec() == kUnreachableWebDataURL ||
      net_error_code_ != net::OK) {
    TRACE_EVENT_ASYNC_STEP_INTO0("navigation", "NavigationHandle", this,
                                 "DidCommitNavigation: error page");
    state_ = DID_COMMIT_ERROR_PAGE;
  } else {
    TRACE_EVENT_ASYNC_STEP_INTO0("navigation", "NavigationHandle", this,
                                 "DidCommitNavigation");
    state_ = DID_COMMIT;

    // Getting this far means that the navigation was not blocked, and neither
    // is this the error page navigation following a blocked navigation. Ensure
    // the frame owner element is no longer collapsed as a result of a prior
    // navigation having been blocked with BLOCK_REQUEST_AND_COLLAPSE.
    if (!frame_tree_node()->IsMainFrame()) {
      // The last committed load in collapsed frames will be an error page with
      // |kUnreachableWebDataURL|. Same-document navigation should not be
      // possible.
      DCHECK(!is_same_document_ || !frame_tree_node()->is_collapsed());
      frame_tree_node()->SetCollapsed(false);
    }
  }
}

void NavigationHandleImpl::SetExpectedProcess(
    RenderProcessHost* expected_process) {
  if (expected_process &&
      expected_process->GetID() == expected_render_process_host_id_) {
    // This |expected_process| has already been informed of the navigation,
    // no need to update it again.
    return;
  }

  // If a RenderProcessHost was expecting this navigation to commit, have it
  // stop tracking this site.
  RenderProcessHost* old_process =
      RenderProcessHost::FromID(expected_render_process_host_id_);
  if (old_process) {
    RenderProcessHostImpl::RemoveExpectedNavigationToSite(
        frame_tree_node_->navigator()->GetController()->GetBrowserContext(),
        old_process, site_url_);
  }

  if (expected_process == nullptr) {
    expected_render_process_host_id_ = ChildProcessHost::kInvalidUniqueID;
    return;
  }

  // Keep track of the speculative RenderProcessHost and tell it to expect a
  // navigation to |site_url_|.
  expected_render_process_host_id_ = expected_process->GetID();
  RenderProcessHostImpl::AddExpectedNavigationToSite(
      frame_tree_node_->navigator()->GetController()->GetBrowserContext(),
      expected_process, site_url_);
}

void NavigationHandleImpl::Transfer() {
  DCHECK(!IsBrowserSideNavigationEnabled());
  // This is an actual transfer. Inform the NavigationResourceThrottle. This
  // will allow to mark the URLRequest as transferring. When it is marked as
  // transferring, the URLRequest can no longer be cancelled by its original
  // RenderFrame. Instead it will persist until being picked up by the transfer
  // RenderFrame, even if the original RenderFrame is destroyed.
  // Note: |transfer_callback_| can be null in unit tests.
  if (!transfer_callback_.is_null())
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, transfer_callback_);
  transfer_callback_.Reset();
}

NavigationThrottle::ThrottleCheckResult
NavigationHandleImpl::CheckWillStartRequest() {
  DCHECK(state_ == WILL_SEND_REQUEST || state_ == DEFERRING_START);
  DCHECK(state_ != WILL_SEND_REQUEST || next_index_ == 0);
  DCHECK(state_ != DEFERRING_START || next_index_ != 0);
  for (size_t i = next_index_; i < throttles_.size(); ++i) {
    NavigationThrottle::ThrottleCheckResult result =
        throttles_[i]->WillStartRequest();
    TRACE_EVENT_ASYNC_STEP_INTO0(
        "navigation", "NavigationHandle", this,
        base::StringPrintf("CheckWillStartRequest: %s: %d",
                           throttles_[i]->GetNameForLogging(), result));
    switch (result) {
      case NavigationThrottle::PROCEED:
        continue;

      case NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE:
        frame_tree_node_->SetCollapsed(true);  // Fall through.
      case NavigationThrottle::BLOCK_REQUEST:
      case NavigationThrottle::CANCEL:
      case NavigationThrottle::CANCEL_AND_IGNORE:
        state_ = CANCELING;
        return result;

      case NavigationThrottle::DEFER:
        state_ = DEFERRING_START;
        next_index_ = i + 1;
        return result;

      case NavigationThrottle::BLOCK_RESPONSE:
        NOTREACHED();
    }
  }
  next_index_ = 0;
  state_ = WILL_SEND_REQUEST;

  return NavigationThrottle::PROCEED;
}

NavigationThrottle::ThrottleCheckResult
NavigationHandleImpl::CheckWillRedirectRequest() {
  DCHECK(state_ == WILL_REDIRECT_REQUEST || state_ == DEFERRING_REDIRECT);
  DCHECK(state_ != WILL_REDIRECT_REQUEST || next_index_ == 0);
  DCHECK(state_ != DEFERRING_REDIRECT || next_index_ != 0);

  for (size_t i = next_index_; i < throttles_.size(); ++i) {
    NavigationThrottle::ThrottleCheckResult result =
        throttles_[i]->WillRedirectRequest();
    TRACE_EVENT_ASYNC_STEP_INTO0(
        "navigation", "NavigationHandle", this,
        base::StringPrintf("CheckWillRedirectRequest: %s: %d",
                           throttles_[i]->GetNameForLogging(), result));
    switch (result) {
      case NavigationThrottle::PROCEED:
        continue;

      case NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE:
        frame_tree_node_->SetCollapsed(true);  // Fall through.
      case NavigationThrottle::BLOCK_REQUEST:
        CHECK(IsBrowserSideNavigationEnabled())
            << "BLOCK_REQUEST and BLOCK_REQUEST_AND_COLLAPSE must not be used "
               "on redirect without PlzNavigate";
      case NavigationThrottle::CANCEL:
      case NavigationThrottle::CANCEL_AND_IGNORE:
        state_ = CANCELING;
        return result;

      case NavigationThrottle::DEFER:
        state_ = DEFERRING_REDIRECT;
        next_index_ = i + 1;
        return result;

      case NavigationThrottle::BLOCK_RESPONSE:
        NOTREACHED();
    }
  }
  next_index_ = 0;
  state_ = WILL_REDIRECT_REQUEST;

  // Notify the delegate that a redirect was encountered and will be followed.
  if (GetDelegate())
    GetDelegate()->DidRedirectNavigation(this);

  return NavigationThrottle::PROCEED;
}

NavigationThrottle::ThrottleCheckResult
NavigationHandleImpl::CheckWillProcessResponse() {
  DCHECK(state_ == WILL_PROCESS_RESPONSE || state_ == DEFERRING_RESPONSE);
  DCHECK(state_ != WILL_PROCESS_RESPONSE || next_index_ == 0);
  DCHECK(state_ != DEFERRING_RESPONSE || next_index_ != 0);

  for (size_t i = next_index_; i < throttles_.size(); ++i) {
    NavigationThrottle::ThrottleCheckResult result =
        throttles_[i]->WillProcessResponse();
    TRACE_EVENT_ASYNC_STEP_INTO0(
        "navigation", "NavigationHandle", this,
        base::StringPrintf("CheckWillProcessResponse: %s: %d",
                           throttles_[i]->GetNameForLogging(), result));
    switch (result) {
      case NavigationThrottle::PROCEED:
        continue;

      case NavigationThrottle::CANCEL:
      case NavigationThrottle::CANCEL_AND_IGNORE:
      case NavigationThrottle::BLOCK_RESPONSE:
        state_ = CANCELING;
        return result;

      case NavigationThrottle::DEFER:
        state_ = DEFERRING_RESPONSE;
        next_index_ = i + 1;
        return result;

      case NavigationThrottle::BLOCK_REQUEST:
      case NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE:
        NOTREACHED();
    }
  }
  next_index_ = 0;
  state_ = WILL_PROCESS_RESPONSE;

  return NavigationThrottle::PROCEED;
}

bool NavigationHandleImpl::MaybeTransferAndProceed() {
  DCHECK_EQ(WILL_PROCESS_RESPONSE, state_);

  // Check if the navigation should transfer. This may result in the
  // destruction of this NavigationHandle, and the cancellation of the request.
  if (!MaybeTransferAndProceedInternal())
    return false;

  // Inform observers that the navigation is now ready to commit, unless a
  // transfer of the navigation failed.
  // PlzNavigate: when a navigation is not set to commit (204/205s/downloads) do
  // not call ReadyToCommitNavigation.
  DCHECK(!IsBrowserSideNavigationEnabled() || render_frame_host_ ||
         net_error_code_ == net::ERR_ABORTED);
  if (!IsBrowserSideNavigationEnabled() || render_frame_host_)
    ReadyToCommitNavigation(render_frame_host_);
  return true;
}

bool NavigationHandleImpl::MaybeTransferAndProceedInternal() {
  DCHECK(render_frame_host_ || (IsBrowserSideNavigationEnabled() &&
                                net_error_code_ == net::ERR_ABORTED));

  // PlzNavigate: the final RenderFrameHost handling this navigation has been
  // decided before calling WillProcessResponse in
  // NavigationRequest::OnResponseStarted.
  // TODO(clamy): See if PlzNavigate could use this code to check whether to
  // use the RFH determined at the start of the navigation or to switch to
  // another one.
  if (IsBrowserSideNavigationEnabled())
    return true;

  // A navigation from a RenderFrame that is no longer active should not attempt
  // to transfer.
  if (!render_frame_host_->is_active()) {
    // This will cause the deletion of this NavigationHandle and the
    // cancellation of the navigation.
    render_frame_host_->SetNavigationHandle(nullptr);
    return false;
  }

  // Subframes shouldn't swap processes unless out-of-process iframes are
  // possible.
  if (!IsInMainFrame() && !SiteIsolationPolicy::AreCrossProcessFramesPossible())
    return true;

  // If this is a download, do not do a cross-site check. The renderer will
  // see it is a download and abort the request.
  //
  // Similarly, HTTP 204 (No Content) responses leave the renderer showing the
  // previous page. The navigation should be allowed to finish without running
  // the unload handler or swapping in the pending RenderFrameHost.
  if (is_download_ || is_stream_ ||
      (response_headers_.get() && response_headers_->response_code() == 204)) {
    return true;
  }

  // The content embedder can decide that a transfer to a different process is
  // required for this URL.
  bool should_transfer =
      GetContentClient()->browser()->ShouldSwapProcessesForRedirect(
          frame_tree_node_->navigator()->GetController()->GetBrowserContext(),
          original_url_, url_);

  RenderFrameHostManager* manager =
      render_frame_host_->frame_tree_node()->render_manager();

  // In the site-per-process model, the RenderFrameHostManager may also decide
  // (independently from the content embedder's ShouldSwapProcessesForRedirect
  // above) that a process transfer is needed. Process transfers are skipped for
  // WebUI processes for now, since e.g. chrome://settings has multiple
  // "cross-site" chrome:// frames, and that doesn't yet work cross-process.
  if (SiteIsolationPolicy::AreCrossProcessFramesPossible() &&
      !ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
          render_frame_host_->GetProcess()->GetID())) {
    should_transfer |= manager->IsRendererTransferNeededForNavigation(
        render_frame_host_, url_);
  }

  // Start the transfer if needed.
  if (should_transfer) {
    // This may destroy the NavigationHandle if the transfer fails.
    base::WeakPtr<NavigationHandleImpl> weak_self = weak_factory_.GetWeakPtr();
    manager->OnCrossSiteResponse(render_frame_host_, request_id_,
                                 redirect_chain_, sanitized_referrer_,
                                 transition_, should_replace_current_entry_);
    if (!weak_self)
      return false;
  }

  return true;
}

void NavigationHandleImpl::RunCompleteCallback(
    NavigationThrottle::ThrottleCheckResult result) {
  DCHECK(result != NavigationThrottle::DEFER);

  ThrottleChecksFinishedCallback callback = complete_callback_;
  complete_callback_.Reset();

  if (!complete_callback_for_testing_.is_null()) {
    complete_callback_for_testing_.Run(result);
    complete_callback_for_testing_.Reset();
  }

  if (!callback.is_null())
    callback.Run(result);

  // No code after running the callback, as it might have resulted in our
  // destruction.
}

void NavigationHandleImpl::RegisterNavigationThrottles() {
  // Register the navigation throttles. The vector returned by
  // CreateThrottlesForNavigation is not assigned to throttles_ directly because
  // it would overwrite any throttles previously added with
  // RegisterThrottleForTesting.
  // TODO(carlosk, arthursonzogni): should simplify this to either use
  // |throttles_| directly (except for the case described above) or
  // |throttles_to_register| for registering all throttles.
  std::vector<std::unique_ptr<NavigationThrottle>> throttles_to_register =
      GetDelegate()->CreateThrottlesForNavigation(this);

  // Check for renderer-inititated main frame navigations to data URLs. This is
  // done first as it may block the main frame navigation altogether.
  std::unique_ptr<NavigationThrottle> data_url_navigation_throttle =
      DataUrlNavigationThrottle::CreateThrottleForNavigation(this);
  if (data_url_navigation_throttle)
    throttles_to_register.push_back(std::move(data_url_navigation_throttle));

  std::unique_ptr<content::NavigationThrottle> ancestor_throttle =
      content::AncestorThrottle::MaybeCreateThrottleFor(this);
  if (ancestor_throttle)
    throttles_.push_back(std::move(ancestor_throttle));

  std::unique_ptr<content::NavigationThrottle> form_submission_throttle =
      content::FormSubmissionThrottle::MaybeCreateThrottleFor(this);
  if (form_submission_throttle)
    throttles_.push_back(std::move(form_submission_throttle));

  // Check for mixed content. This is done after the AncestorThrottle and the
  // FormSubmissionThrottle so that when folks block mixed content with a CSP
  // policy, they don't get a warning. They'll still get a warning in the
  // console about CSP blocking the load.
  std::unique_ptr<NavigationThrottle> mixed_content_throttle =
      MixedContentNavigationThrottle::CreateThrottleForNavigation(this);
  if (mixed_content_throttle)
    throttles_to_register.push_back(std::move(mixed_content_throttle));

  std::unique_ptr<NavigationThrottle> devtools_throttle =
      RenderFrameDevToolsAgentHost::CreateThrottleForNavigation(this);
  if (devtools_throttle)
    throttles_to_register.push_back(std::move(devtools_throttle));

  throttles_.insert(throttles_.begin(),
                    std::make_move_iterator(throttles_to_register.begin()),
                    std::make_move_iterator(throttles_to_register.end()));
}

bool NavigationHandleImpl::IsSelfReferentialURL() {
  // about: URLs should be exempted since they are reserved for other purposes
  // and cannot be the source of infinite recursion. See
  // https://crbug.com/341858 .
  if (url_.SchemeIs("about"))
    return false;

  // Browser-triggered navigations should be exempted.
  if (!is_renderer_initiated_)
    return false;

  // Some sites rely on constructing frame hierarchies where frames are loaded
  // via POSTs with the same URLs, so exempt POST requests.  See
  // https://crbug.com/710008.
  if (method_ == "POST")
    return false;

  // We allow one level of self-reference because some sites depend on that,
  // but we don't allow more than one.
  bool found_self_reference = false;
  for (const FrameTreeNode* node = frame_tree_node_->parent(); node;
       node = node->parent()) {
    if (node->current_url().EqualsIgnoringRef(url_)) {
      if (found_self_reference)
        return true;
      found_self_reference = true;
    }
  }
  return false;
}

void NavigationHandleImpl::UpdateSiteURL() {
  GURL new_site_url = SiteInstance::GetSiteForURL(
      frame_tree_node_->navigator()->GetController()->GetBrowserContext(),
      url_);
  if (new_site_url == site_url_)
    return;

  // When redirecting cross-site, stop telling the speculative
  // RenderProcessHost to expect a navigation commit.
  SetExpectedProcess(nullptr);
  site_url_ = new_site_url;
}

}  // namespace content
