// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/navigation_handle_impl.h"

#include <iterator>

#include "base/bind.h"
#include "base/debug/dump_without_crashing.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "content/browser/appcache/appcache_navigation_handle.h"
#include "content/browser/appcache/appcache_service_impl.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/frame_host/debug_urls.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/navigation_controller_impl.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/frame_host/navigator_delegate.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_navigation_handle.h"
#include "content/common/child_process_host_impl.h"
#include "content/common/frame_messages.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/navigation_ui_data.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "net/base/net_errors.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace content {

namespace {

// Default timeout for the READY_TO_COMMIT -> COMMIT transition.  Chosen
// initially based on the Navigation.ReadyToCommitUntilCommit UMA, and then
// refined based on feedback based on CrashExitCodes.Renderer/RESULT_CODE_HUNG.
constexpr base::TimeDelta kDefaultCommitTimeout =
    base::TimeDelta::FromSeconds(30);

// Timeout for the READY_TO_COMMIT -> COMMIT transition.
// Overrideable via SetCommitTimeoutForTesting.
base::TimeDelta g_commit_timeout = kDefaultCommitTimeout;

// Use this to get a new unique ID for a NavigationHandle during construction.
// The returned ID is guaranteed to be nonzero (zero is the "no ID" indicator).
int64_t CreateUniqueHandleID() {
  static int64_t unique_id_counter = 0;
  return ++unique_id_counter;
}

// LOG_NAVIGATION_TIMING_HISTOGRAM logs |value| for "Navigation.<histogram>" UMA
// as well as supplementary UMAs (depending on |transition| and |is_background|)
// for BackForward/Reload/NewNavigation variants.
//
// kMaxTime and kBuckets constants are consistent with
// UMA_HISTOGRAM_MEDIUM_TIMES, but a custom kMinTime is used for high fidelity
// near the low end of measured values.
//
// TODO(csharrison,nasko): This macro is incorrect for subframe navigations,
// which will only have subframe-specific transition types. This means that all
// subframes currently are tagged as NewNavigations.
#define LOG_NAVIGATION_TIMING_HISTOGRAM(histogram, transition, is_background, \
                                        duration)                             \
  do {                                                                        \
    const base::TimeDelta kMinTime = base::TimeDelta::FromMilliseconds(1);    \
    const base::TimeDelta kMaxTime = base::TimeDelta::FromMinutes(3);         \
    const int kBuckets = 50;                                                  \
    UMA_HISTOGRAM_CUSTOM_TIMES("Navigation." histogram, duration, kMinTime,   \
                               kMaxTime, kBuckets);                           \
    if (transition & ui::PAGE_TRANSITION_FORWARD_BACK) {                      \
      UMA_HISTOGRAM_CUSTOM_TIMES("Navigation." histogram ".BackForward",      \
                                 duration, kMinTime, kMaxTime, kBuckets);     \
    } else if (ui::PageTransitionCoreTypeIs(transition,                       \
                                            ui::PAGE_TRANSITION_RELOAD)) {    \
      UMA_HISTOGRAM_CUSTOM_TIMES("Navigation." histogram ".Reload", duration, \
                                 kMinTime, kMaxTime, kBuckets);               \
    } else if (ui::PageTransitionIsNewNavigation(transition)) {               \
      UMA_HISTOGRAM_CUSTOM_TIMES("Navigation." histogram ".NewNavigation",    \
                                 duration, kMinTime, kMaxTime, kBuckets);     \
    } else {                                                                  \
      NOTREACHED() << "Invalid page transition: " << transition;              \
    }                                                                         \
    if (is_background.has_value()) {                                          \
      if (is_background.value()) {                                            \
        UMA_HISTOGRAM_CUSTOM_TIMES("Navigation." histogram                    \
                                   ".BackgroundProcessPriority",              \
                                   duration, kMinTime, kMaxTime, kBuckets);   \
      } else {                                                                \
        UMA_HISTOGRAM_CUSTOM_TIMES("Navigation." histogram                    \
                                   ".ForegroundProcessPriority",              \
                                   duration, kMinTime, kMaxTime, kBuckets);   \
      }                                                                       \
    }                                                                         \
  } while (0)

void LogIsSameProcess(ui::PageTransition transition, bool is_same_process) {
  // Log overall value, then log specific value per type of navigation.
  UMA_HISTOGRAM_BOOLEAN("Navigation.IsSameProcess", is_same_process);

  if (transition & ui::PAGE_TRANSITION_FORWARD_BACK) {
    UMA_HISTOGRAM_BOOLEAN("Navigation.IsSameProcess.BackForward",
                          is_same_process);
    return;
  }
  if (ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_RELOAD)) {
    UMA_HISTOGRAM_BOOLEAN("Navigation.IsSameProcess.Reload", is_same_process);
    return;
  }
  if (ui::PageTransitionIsNewNavigation(transition)) {
    UMA_HISTOGRAM_BOOLEAN("Navigation.IsSameProcess.NewNavigation",
                          is_same_process);
    return;
  }
  NOTREACHED() << "Invalid page transition: " << transition;
}

}  // namespace

NavigationHandleImpl::NavigationHandleImpl(
    NavigationRequest* navigation_request,
    const std::vector<GURL>& redirect_chain,
    bool is_same_document,
    int pending_nav_entry_id,
    std::unique_ptr<NavigationUIData> navigation_ui_data,
    net::HttpRequestHeaders request_headers,
    const Referrer& sanitized_referrer,
    bool is_external_protocol)
    : navigation_request_(navigation_request),
      is_external_protocol_(is_external_protocol),
      net_error_code_(net::OK),
      render_frame_host_(nullptr),
      is_same_document_(is_same_document),
      was_redirected_(false),
      did_replace_entry_(false),
      should_update_history_(false),
      subframe_entry_committed_(false),
      connection_info_(net::HttpResponseInfo::CONNECTION_INFO_UNKNOWN),
      request_headers_(std::move(request_headers)),
      state_(INITIAL),
      pending_nav_entry_id_(pending_nav_entry_id),
      navigation_ui_data_(std::move(navigation_ui_data)),
      navigation_id_(CreateUniqueHandleID()),
      redirect_chain_(redirect_chain),
      reload_type_(ReloadType::NONE),
      restore_type_(RestoreType::NONE),
      navigation_type_(NAVIGATION_TYPE_UNKNOWN),
      expected_render_process_host_id_(ChildProcessHost::kInvalidUniqueID),
      is_download_(false),
      is_stream_(false),
      is_signed_exchange_inner_response_(false),
      was_cached_(false),
      is_same_process_(true),
      throttle_runner_(this, this),
      weak_factory_(this) {
  const GURL& url = navigation_request_->common_params().url;
  TRACE_EVENT_ASYNC_BEGIN2("navigation", "NavigationHandle", this,
                           "frame_tree_node",
                           frame_tree_node()->frame_tree_node_id(), "url",
                           url.possibly_invalid_spec());
  DCHECK(!navigation_request_->common_params().navigation_start.is_null());
  DCHECK(!IsRendererDebugURL(url));

  starting_site_instance_ =
      frame_tree_node()->current_frame_host()->GetSiteInstance();

  site_url_ = SiteInstanceImpl::GetSiteForURL(
      starting_site_instance_->GetBrowserContext(),
      starting_site_instance_->GetIsolationContext(), url);
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

  GetDelegate()->DidStartNavigation(this);

  if (IsInMainFrame()) {
    TRACE_EVENT_ASYNC_BEGIN_WITH_TIMESTAMP1(
        "navigation", "Navigation StartToCommit", this,
        navigation_request_->common_params().navigation_start, "Initial URL",
        url.spec());
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
          frame_tree_node()->navigator()->GetController()->GetBrowserContext(),
          process, site_url_);
    }
  }

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

int64_t NavigationHandleImpl::GetNavigationId() const {
  return navigation_id_;
}

const GURL& NavigationHandleImpl::GetURL() {
  return navigation_request_->common_params().url;
}

SiteInstanceImpl* NavigationHandleImpl::GetStartingSiteInstance() {
  return starting_site_instance_.get();
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
  return navigation_ui_data_.get();
}

bool NavigationHandleImpl::IsExternalProtocol() {
  return is_external_protocol_;
}

net::Error NavigationHandleImpl::GetNetErrorCode() {
  return net_error_code_;
}

RenderFrameHostImpl* NavigationHandleImpl::GetRenderFrameHost() {
  // Only allow the RenderFrameHost to be retrieved once it has been set for
  // this navigation.  This will happens either at WillProcessResponse time for
  // regular navigations or at WillFailRequest time for error pages.
  CHECK_GE(state_, PROCESSING_WILL_FAIL_REQUEST)
      << "This accessor should only be called after a RenderFrameHost has been "
         "picked for this navigation.";
  static_assert(WILL_FAIL_REQUEST < WILL_PROCESS_RESPONSE,
                "WillFailRequest state should come before WillProcessResponse");
  return render_frame_host_;
}

bool NavigationHandleImpl::IsSameDocument() {
  return is_same_document_;
}

const net::HttpRequestHeaders& NavigationHandleImpl::GetRequestHeaders() {
  return request_headers_;
}

void NavigationHandleImpl::RemoveRequestHeader(const std::string& header_name) {
  DCHECK(state_ == PROCESSING_WILL_REDIRECT_REQUEST ||
         state_ == WILL_REDIRECT_REQUEST);
  removed_request_headers_.push_back(header_name);
}

void NavigationHandleImpl::SetRequestHeader(const std::string& header_name,
                                            const std::string& header_value) {
  DCHECK(state_ == INITIAL || state_ == PROCESSING_WILL_START_REQUEST ||
         state_ == PROCESSING_WILL_REDIRECT_REQUEST ||
         state_ == WILL_START_REQUEST || state_ == WILL_REDIRECT_REQUEST);
  modified_request_headers_.SetHeader(header_name, header_value);
}

const net::HttpResponseHeaders* NavigationHandleImpl::GetResponseHeaders() {
  if (response_headers_for_testing_)
    return response_headers_for_testing_.get();
  return response_headers_.get();
}

net::HttpResponseInfo::ConnectionInfo
NavigationHandleImpl::GetConnectionInfo() {
  return connection_info_;
}

const net::SSLInfo& NavigationHandleImpl::GetSSLInfo() {
  return ssl_info_;
}

bool NavigationHandleImpl::IsWaitingToCommit() {
  return state_ == READY_TO_COMMIT;
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
  // This is CANCELING because although the data comes in after
  // WILL_PROCESS_RESPONSE, it's possible for the navigation to be cancelled
  // after and the caller might want this value.
  DCHECK(state_ >= CANCELING);
  return socket_address_;
}

void NavigationHandleImpl::Resume(NavigationThrottle* resuming_throttle) {
  DCHECK(resuming_throttle);
  TRACE_EVENT_ASYNC_STEP_INTO0("navigation", "NavigationHandle", this,
                               "Resume");
  throttle_runner_.ResumeProcessingNavigationEvent(resuming_throttle);
  // DO NOT ADD CODE AFTER THIS, as the NavigationHandle might have been deleted
  // by the previous call.
}

void NavigationHandleImpl::CancelDeferredNavigation(
    NavigationThrottle* cancelling_throttle,
    NavigationThrottle::ThrottleCheckResult result) {
  DCHECK(cancelling_throttle);
  DCHECK_EQ(cancelling_throttle, throttle_runner_.GetDeferringThrottle());
  CancelDeferredNavigationInternal(result);
}

void NavigationHandleImpl::RegisterThrottleForTesting(
    std::unique_ptr<NavigationThrottle> navigation_throttle) {
  throttle_runner_.AddThrottle(std::move(navigation_throttle));
}

bool NavigationHandleImpl::IsDeferredForTesting() {
  return throttle_runner_.GetDeferringThrottle() != nullptr;
}

bool NavigationHandleImpl::WasStartedFromContextMenu() const {
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
  DCHECK(state_ >= PROCESSING_WILL_PROCESS_RESPONSE);
  return request_id_;
}

bool NavigationHandleImpl::IsDownload() {
  return is_download_;
}

bool NavigationHandleImpl::IsFormSubmission() {
  return navigation_request_->begin_params()->is_form_submission;
}

const std::string& NavigationHandleImpl::GetHrefTranslate() {
  return navigation_request_->common_params().href_translate;
}

void NavigationHandleImpl::CallResumeForTesting() {
  throttle_runner_.CallResumeForTesting();
}

const base::Optional<url::Origin>& NavigationHandleImpl::GetInitiatorOrigin() {
  return navigation_request_->common_params().initiator_origin;
}

bool NavigationHandleImpl::IsSignedExchangeInnerResponse() {
  return is_signed_exchange_inner_response_;
}

bool NavigationHandleImpl::WasResponseCached() {
  return was_cached_;
}

const net::ProxyServer& NavigationHandleImpl::GetProxyServer() {
  return proxy_server_;
}

void NavigationHandleImpl::InitServiceWorkerHandle(
    ServiceWorkerContextWrapper* service_worker_context) {
  service_worker_handle_.reset(
      new ServiceWorkerNavigationHandle(service_worker_context));
}

void NavigationHandleImpl::InitAppCacheHandle(
    ChromeAppCacheService* appcache_service) {
  // The final process id won't be available until
  // NavigationHandleImpl::ReadyToCommitNavigation.
  appcache_handle_.reset(new AppCacheNavigationHandle(
      appcache_service, ChildProcessHost::kInvalidUniqueID));
}

void NavigationHandleImpl::WillStartRequest(
    ThrottleChecksFinishedCallback callback) {
  TRACE_EVENT_ASYNC_STEP_INTO0("navigation", "NavigationHandle", this,
                               "WillStartRequest");
  // WillStartRequest should only be called once.
  if (state_ != INITIAL) {
    state_ = CANCELING;
    RunCompleteCallback(NavigationThrottle::CANCEL);
    return;
  }

  state_ = PROCESSING_WILL_START_REQUEST;
  complete_callback_ = std::move(callback);

  if (IsSelfReferentialURL()) {
    state_ = CANCELING;
    RunCompleteCallback(NavigationThrottle::CANCEL);
    return;
  }

  throttle_runner_.RegisterNavigationThrottles();

  // If the content/ embedder did not pass the NavigationUIData at the beginning
  // of the navigation, ask for it now.
  if (!navigation_ui_data_)
    navigation_ui_data_ = GetDelegate()->GetNavigationUIData(this);

  // Notify each throttle of the request.
  throttle_runner_.ProcessNavigationEvent(
      NavigationThrottleRunner::Event::WillStartRequest);
  // DO NOT ADD CODE AFTER THIS, as the NavigationHandle might have been deleted
  // by the previous call.
}

void NavigationHandleImpl::UpdateStateFollowingRedirect(
    const GURL& new_referrer_url,
    bool new_is_external_protocol,
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    net::HttpResponseInfo::ConnectionInfo connection_info,
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

  is_external_protocol_ = new_is_external_protocol;
  response_headers_ = response_headers;
  connection_info_ = connection_info;
  was_redirected_ = true;
  redirect_chain_.push_back(GetURL());

  state_ = PROCESSING_WILL_REDIRECT_REQUEST;
  complete_callback_ = std::move(callback);
}

void NavigationHandleImpl::WillRedirectRequest(
    const GURL& new_referrer_url,
    bool new_is_external_protocol,
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    net::HttpResponseInfo::ConnectionInfo connection_info,
    RenderProcessHost* post_redirect_process,
    ThrottleChecksFinishedCallback callback) {
  TRACE_EVENT_ASYNC_STEP_INTO1("navigation", "NavigationHandle", this,
                               "WillRedirectRequest", "url",
                               GetURL().possibly_invalid_spec());
  UpdateStateFollowingRedirect(new_referrer_url, new_is_external_protocol,
                               response_headers, connection_info,
                               std::move(callback));
  UpdateSiteURL(post_redirect_process);

  if (IsSelfReferentialURL()) {
    state_ = CANCELING;
    RunCompleteCallback(NavigationThrottle::CANCEL);
    return;
  }

  // Notify each throttle of the request.
  throttle_runner_.ProcessNavigationEvent(
      NavigationThrottleRunner::Event::WillRedirectRequest);
  // DO NOT ADD CODE AFTER THIS, as the NavigationHandle might have been deleted
  // by the previous call.
}

void NavigationHandleImpl::WillFailRequest(
    RenderFrameHostImpl* render_frame_host,
    base::Optional<net::SSLInfo> ssl_info,
    ThrottleChecksFinishedCallback callback) {
  TRACE_EVENT_ASYNC_STEP_INTO0("navigation", "NavigationHandle", this,
                               "WillFailRequest");
  if (ssl_info.has_value())
    ssl_info_ = ssl_info.value();

  render_frame_host_ = render_frame_host;
  complete_callback_ = std::move(callback);
  state_ = PROCESSING_WILL_FAIL_REQUEST;

  // Notify each throttle of the request.
  throttle_runner_.ProcessNavigationEvent(
      NavigationThrottleRunner::Event::WillFailRequest);
  // DO NOT ADD CODE AFTER THIS, as the NavigationHandle might have been deleted
  // by the previous call.
}

void NavigationHandleImpl::WillProcessResponse(
    RenderFrameHostImpl* render_frame_host,
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    net::HttpResponseInfo::ConnectionInfo connection_info,
    const net::HostPortPair& socket_address,
    const net::SSLInfo& ssl_info,
    const GlobalRequestID& request_id,
    bool is_download,
    bool is_stream,
    bool is_signed_exchange_inner_response,
    bool was_cached,
    ThrottleChecksFinishedCallback callback) {
  TRACE_EVENT_ASYNC_STEP_INTO0("navigation", "NavigationHandle", this,
                               "WillProcessResponse");

  DCHECK(!render_frame_host_ || render_frame_host_ == render_frame_host);
  render_frame_host_ = render_frame_host;
  response_headers_ = response_headers;
  connection_info_ = connection_info;
  request_id_ = request_id;
  is_download_ = is_download;
  is_stream_ = is_stream;
  is_signed_exchange_inner_response_ = is_signed_exchange_inner_response;
  was_cached_ = was_cached;
  state_ = PROCESSING_WILL_PROCESS_RESPONSE;
  ssl_info_ = ssl_info;
  socket_address_ = socket_address;
  complete_callback_ = std::move(callback);

  // Notify each throttle of the response.
  throttle_runner_.ProcessNavigationEvent(
      NavigationThrottleRunner::Event::WillProcessResponse);
  // DO NOT ADD CODE AFTER THIS, as the NavigationHandle might have been deleted
  // by the previous call.
}

void NavigationHandleImpl::ReadyToCommitNavigation(
    RenderFrameHostImpl* render_frame_host,
    bool is_error) {
  TRACE_EVENT_ASYNC_STEP_INTO0("navigation", "NavigationHandle", this,
                               "ReadyToCommitNavigation");

  // If the NavigationHandle already has a RenderFrameHost set at
  // WillProcessResponse time, we should not be changing it.  One exception is
  // errors originating from WillProcessResponse throttles, which might commit
  // in a different RenderFrameHost.  For example, a throttle might return
  // CANCEL with an error code from WillProcessResponse, which will cancel the
  // navigation and get here to commit the error page, with |render_frame_host|
  // recomputed for the error page.
  DCHECK(!render_frame_host_ || is_error ||
         render_frame_host_ == render_frame_host)
      << "Unsupported RenderFrameHost change from " << render_frame_host_
      << " to " << render_frame_host << " with is_error=" << is_error;

  render_frame_host_ = render_frame_host;
  state_ = READY_TO_COMMIT;
  ready_to_commit_time_ = base::TimeTicks::Now();
  RestartCommitTimeout();

  if (appcache_handle_)
    appcache_handle_->SetProcessId(render_frame_host->GetProcess()->GetID());

  // Record metrics for the time it takes to get to this state from the
  // beginning of the navigation.
  if (!IsSameDocument() && !is_error) {
    is_same_process_ =
        render_frame_host_->GetProcess()->GetID() ==
        frame_tree_node()->current_frame_host()->GetProcess()->GetID();
    LogIsSameProcess(GetPageTransition(), is_same_process_);

    // Don't log process-priority-specific UMAs for TimeToReadyToCommit2 metric
    // (which shouldn't be influenced by renderer priority).
    constexpr base::Optional<bool> kIsBackground = base::nullopt;

    base::TimeDelta delta =
        ready_to_commit_time_ -
        navigation_request_->common_params().navigation_start;
    LOG_NAVIGATION_TIMING_HISTOGRAM("TimeToReadyToCommit2", GetPageTransition(),
                                    kIsBackground, delta);

    if (IsInMainFrame()) {
      LOG_NAVIGATION_TIMING_HISTOGRAM("TimeToReadyToCommit2.MainFrame",
                                      GetPageTransition(), kIsBackground,
                                      delta);
    } else {
      LOG_NAVIGATION_TIMING_HISTOGRAM("TimeToReadyToCommit2.Subframe",
                                      GetPageTransition(), kIsBackground,
                                      delta);
    }

    if (is_same_process_) {
      LOG_NAVIGATION_TIMING_HISTOGRAM("TimeToReadyToCommit2.SameProcess",
                                      GetPageTransition(), kIsBackground,
                                      delta);
    } else {
      LOG_NAVIGATION_TIMING_HISTOGRAM("TimeToReadyToCommit2.CrossProcess",
                                      GetPageTransition(), kIsBackground,
                                      delta);
    }
  }

  SetExpectedProcess(render_frame_host->GetProcess());

  if (!IsSameDocument())
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
  DCHECK_EQ(frame_tree_node(), render_frame_host->frame_tree_node());
  CHECK_EQ(GetURL(), params.url);

  did_replace_entry_ = did_replace_entry;
  should_update_history_ = params.should_update_history;
  render_frame_host_ = render_frame_host;
  previous_url_ = previous_url;
  base_url_ = params.base_url;
  navigation_type_ = navigation_type;

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
  }

  StopCommitTimeout();

  // Record metrics for the time it took to commit the navigation if it was to
  // another document without error.
  if (!IsSameDocument() && !IsErrorPage()) {
    base::TimeTicks now = base::TimeTicks::Now();
    base::TimeDelta delta =
        now - navigation_request_->common_params().navigation_start;
    ui::PageTransition transition = GetPageTransition();
    base::Optional<bool> is_background =
        render_frame_host->GetProcess()->IsProcessBackgrounded();
    LOG_NAVIGATION_TIMING_HISTOGRAM("StartToCommit", transition, is_background,
                                    delta);
    if (IsInMainFrame()) {
      LOG_NAVIGATION_TIMING_HISTOGRAM("StartToCommit.MainFrame", transition,
                                      is_background, delta);
    } else {
      LOG_NAVIGATION_TIMING_HISTOGRAM("StartToCommit.Subframe", transition,
                                      is_background, delta);
    }
    if (is_same_process_) {
      LOG_NAVIGATION_TIMING_HISTOGRAM("StartToCommit.SameProcess", transition,
                                      is_background, delta);
      if (IsInMainFrame()) {
        LOG_NAVIGATION_TIMING_HISTOGRAM("StartToCommit.SameProcess.MainFrame",
                                        transition, is_background, delta);
      } else {
        LOG_NAVIGATION_TIMING_HISTOGRAM("StartToCommit.SameProcess.Subframe",
                                        transition, is_background, delta);
      }
    } else {
      LOG_NAVIGATION_TIMING_HISTOGRAM("StartToCommit.CrossProcess", transition,
                                      is_background, delta);
      if (IsInMainFrame()) {
        LOG_NAVIGATION_TIMING_HISTOGRAM("StartToCommit.CrossProcess.MainFrame",
                                        transition, is_background, delta);
      } else {
        LOG_NAVIGATION_TIMING_HISTOGRAM("StartToCommit.CrossProcess.Subframe",
                                        transition, is_background, delta);
      }
    }

    if (!ready_to_commit_time_.is_null()) {
      LOG_NAVIGATION_TIMING_HISTOGRAM("ReadyToCommitUntilCommit2",
                                      GetPageTransition(), is_background,
                                      now - ready_to_commit_time_);
    }
  }

  DCHECK(!IsInMainFrame() || navigation_entry_committed)
      << "Only subframe navigations can get here without changing the "
      << "NavigationEntry";
  subframe_entry_committed_ = navigation_entry_committed;

  // For successful navigations, ensure the frame owner element is no longer
  // collapsed as a result of a prior navigation.
  if (!IsErrorPage() && !frame_tree_node()->IsMainFrame()) {
    // The last committed load in collapsed frames will be an error page with
    // |kUnreachableWebDataURL|. Same-document navigation should not be
    // possible.
    DCHECK(!is_same_document_ || !frame_tree_node()->is_collapsed());
    frame_tree_node()->SetCollapsed(false);
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
        frame_tree_node()->navigator()->GetController()->GetBrowserContext(),
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
      frame_tree_node()->navigator()->GetController()->GetBrowserContext(),
      expected_process, site_url_);
}

void NavigationHandleImpl::OnNavigationEventProcessed(
    NavigationThrottleRunner::Event event,
    NavigationThrottle::ThrottleCheckResult result) {
  DCHECK_NE(NavigationThrottle::DEFER, result.action());
  switch (event) {
    case NavigationThrottleRunner::Event::WillStartRequest:
      OnWillStartRequestProcessed(result);
      return;
    case NavigationThrottleRunner::Event::WillRedirectRequest:
      OnWillRedirectRequestProcessed(result);
      return;
    case NavigationThrottleRunner::Event::WillFailRequest:
      OnWillFailRequestProcessed(result);
      return;
    case NavigationThrottleRunner::Event::WillProcessResponse:
      OnWillProcessResponseProcessed(result);
      return;
    default:
      NOTREACHED();
  }
  NOTREACHED();
}

void NavigationHandleImpl::OnWillStartRequestProcessed(
    NavigationThrottle::ThrottleCheckResult result) {
  DCHECK_EQ(PROCESSING_WILL_START_REQUEST, state_);
  DCHECK_NE(NavigationThrottle::BLOCK_RESPONSE, result.action());
  if (result.action() == NavigationThrottle::PROCEED)
    state_ = WILL_START_REQUEST;
  else
    state_ = CANCELING;
  RunCompleteCallback(result);
}

void NavigationHandleImpl::OnWillRedirectRequestProcessed(
    NavigationThrottle::ThrottleCheckResult result) {
  DCHECK_EQ(PROCESSING_WILL_REDIRECT_REQUEST, state_);
  DCHECK_NE(NavigationThrottle::BLOCK_RESPONSE, result.action());
  if (result.action() == NavigationThrottle::PROCEED) {
    state_ = WILL_REDIRECT_REQUEST;

    // Notify the delegate that a redirect was encountered and will be followed.
    if (GetDelegate())
      GetDelegate()->DidRedirectNavigation(this);
  } else {
    state_ = CANCELING;
  }
  RunCompleteCallback(result);
}

void NavigationHandleImpl::OnWillFailRequestProcessed(
    NavigationThrottle::ThrottleCheckResult result) {
  DCHECK_EQ(PROCESSING_WILL_FAIL_REQUEST, state_);
  DCHECK_NE(NavigationThrottle::BLOCK_RESPONSE, result.action());
  if (result.action() == NavigationThrottle::PROCEED) {
    state_ = WILL_FAIL_REQUEST;
    result = NavigationThrottle::ThrottleCheckResult(
        NavigationThrottle::PROCEED, net_error_code_);
  } else {
    state_ = CANCELING;
  }
  RunCompleteCallback(result);
}

void NavigationHandleImpl::OnWillProcessResponseProcessed(
    NavigationThrottle::ThrottleCheckResult result) {
  DCHECK_EQ(PROCESSING_WILL_PROCESS_RESPONSE, state_);
  DCHECK_NE(NavigationThrottle::BLOCK_REQUEST, result.action());
  DCHECK_NE(NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE, result.action());
  if (result.action() == NavigationThrottle::PROCEED) {
    state_ = WILL_PROCESS_RESPONSE;
    // If the navigation is done processing the response, then it's ready to
    // commit. Inform observers that the navigation is now ready to commit,
    // unless it is not set to commit (204/205s/downloads).
    if (render_frame_host_) {
      base::WeakPtr<NavigationHandleImpl> weak_ptr = weak_factory_.GetWeakPtr();
      ReadyToCommitNavigation(render_frame_host_, false);
      // TODO(https://crbug.com/880741): Remove this once the bug is fixed.
      if (!weak_ptr) {
        base::debug::DumpWithoutCrashing();
        return;
      }
    }
  } else {
    state_ = CANCELING;
  }
  RunCompleteCallback(result);
}

void NavigationHandleImpl::CancelDeferredNavigationInternal(
    NavigationThrottle::ThrottleCheckResult result) {
  DCHECK(state_ == PROCESSING_WILL_START_REQUEST ||
         state_ == PROCESSING_WILL_REDIRECT_REQUEST ||
         state_ == PROCESSING_WILL_FAIL_REQUEST ||
         state_ == PROCESSING_WILL_PROCESS_RESPONSE);
  DCHECK(result.action() == NavigationThrottle::CANCEL_AND_IGNORE ||
         result.action() == NavigationThrottle::CANCEL ||
         result.action() == NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE);
  DCHECK(result.action() != NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE ||
         state_ == PROCESSING_WILL_START_REQUEST ||
         state_ == PROCESSING_WILL_REDIRECT_REQUEST);

  TRACE_EVENT_ASYNC_STEP_INTO0("navigation", "NavigationHandle", this,
                               "CancelDeferredNavigation");
  state_ = CANCELING;
  RunCompleteCallback(result);
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

bool NavigationHandleImpl::IsSelfReferentialURL() {
  // about: URLs should be exempted since they are reserved for other purposes
  // and cannot be the source of infinite recursion. See
  // https://crbug.com/341858 .
  if (GetURL().SchemeIs("about"))
    return false;

  // Browser-triggered navigations should be exempted.
  if (navigation_request_->browser_initiated())
    return false;

  // Some sites rely on constructing frame hierarchies where frames are loaded
  // via POSTs with the same URLs, so exempt POST requests.  See
  // https://crbug.com/710008.
  if (navigation_request_->common_params().method == "POST")
    return false;

  // We allow one level of self-reference because some sites depend on that,
  // but we don't allow more than one.
  bool found_self_reference = false;
  for (const FrameTreeNode* node = frame_tree_node()->parent(); node;
       node = node->parent()) {
    if (node->current_url().EqualsIgnoringRef(GetURL())) {
      if (found_self_reference)
        return true;
      found_self_reference = true;
    }
  }
  return false;
}

void NavigationHandleImpl::UpdateSiteURL(
    RenderProcessHost* post_redirect_process) {
  // TODO(alexmos): Using |starting_site_instance_|'s IsolationContext may not
  // be correct for cross-BrowsingInstance redirects.
  GURL new_site_url = SiteInstanceImpl::GetSiteForURL(
      frame_tree_node()->navigator()->GetController()->GetBrowserContext(),
      starting_site_instance_->GetIsolationContext(), GetURL());
  int post_redirect_process_id = post_redirect_process
                                     ? post_redirect_process->GetID()
                                     : ChildProcessHost::kInvalidUniqueID;
  if (new_site_url == site_url_ &&
      post_redirect_process_id == expected_render_process_host_id_) {
    return;
  }

  // Stop expecting a navigation to the current site URL in the current expected
  // process.
  SetExpectedProcess(nullptr);

  // Update the site URL and the expected process.
  site_url_ = new_site_url;
  SetExpectedProcess(post_redirect_process);
}

void NavigationHandleImpl::RenderProcessBlockedStateChanged(bool blocked) {
  if (blocked)
    StopCommitTimeout();
  else
    RestartCommitTimeout();
}

void NavigationHandleImpl::StopCommitTimeout() {
  commit_timeout_timer_.Stop();
  render_process_blocked_state_changed_subscription_.reset();
  GetRenderFrameHost()->GetRenderWidgetHost()->RendererIsResponsive();
}

void NavigationHandleImpl::RestartCommitTimeout() {
  commit_timeout_timer_.Stop();
  if (state_ >= DID_COMMIT)
    return;

  RenderProcessHost* renderer_host =
      GetRenderFrameHost()->GetRenderWidgetHost()->GetProcess();
  render_process_blocked_state_changed_subscription_ =
      renderer_host->RegisterBlockStateChangedCallback(base::BindRepeating(
          &NavigationHandleImpl::RenderProcessBlockedStateChanged,
          base::Unretained(this)));
  if (!renderer_host->IsBlocked())
    commit_timeout_timer_.Start(
        FROM_HERE, g_commit_timeout,
        base::BindRepeating(&NavigationHandleImpl::OnCommitTimeout,
                            weak_factory_.GetWeakPtr()));
}

void NavigationHandleImpl::OnCommitTimeout() {
  DCHECK_EQ(READY_TO_COMMIT, state_);
  render_process_blocked_state_changed_subscription_.reset();
  GetRenderFrameHost()->GetRenderWidgetHost()->RendererIsUnresponsive(
      base::BindRepeating(&NavigationHandleImpl::RestartCommitTimeout,
                          weak_factory_.GetWeakPtr()));
}

// static
void NavigationHandleImpl::SetCommitTimeoutForTesting(
    const base::TimeDelta& timeout) {
  if (timeout.is_zero())
    g_commit_timeout = kDefaultCommitTimeout;
  else
    g_commit_timeout = timeout;
}

}  // namespace content
