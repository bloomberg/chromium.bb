// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/navigation_simulator.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "content/browser/frame_host/debug_urls.h"
#include "content/browser/frame_host/navigation_handle_impl.h"
#include "content/browser/frame_host/navigation_request.h"
#include "content/common/frame_messages.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/resource_request_body.h"
#include "content/public/common/url_utils.h"
#include "content/test/test_navigation_url_loader.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_web_contents.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_flags.h"
#include "net/url_request/redirect_info.h"

namespace content {

namespace {

class NavigationThrottleCallbackRunner : public NavigationThrottle {
 public:
  NavigationThrottleCallbackRunner(
      NavigationHandle* handle,
      const base::Closure& on_will_start_request,
      const base::Closure& on_will_redirect_request,
      const base::Closure& on_will_process_response)
      : NavigationThrottle(handle),
        on_will_start_request_(on_will_start_request),
        on_will_redirect_request_(on_will_redirect_request),
        on_will_process_response_(on_will_process_response) {}

  NavigationThrottle::ThrottleCheckResult WillStartRequest() override {
    on_will_start_request_.Run();
    return NavigationThrottle::PROCEED;
  }

  NavigationThrottle::ThrottleCheckResult WillRedirectRequest() override {
    on_will_redirect_request_.Run();
    return NavigationThrottle::PROCEED;
  }

  NavigationThrottle::ThrottleCheckResult WillProcessResponse() override {
    on_will_process_response_.Run();
    return NavigationThrottle::PROCEED;
  }

  const char* GetNameForLogging() override {
    return "NavigationThrottleCallbackRunner";
  }

 private:
  base::Closure on_will_start_request_;
  base::Closure on_will_redirect_request_;
  base::Closure on_will_process_response_;
};

}  // namespace

// static
RenderFrameHost* NavigationSimulator::NavigateAndCommitFromBrowser(
    WebContents* web_contents,
    const GURL& url) {
  NavigationSimulator simulator(url, true /* browser_initiated */,
                                static_cast<WebContentsImpl*>(web_contents),
                                nullptr);
  simulator.Commit();
  return simulator.GetFinalRenderFrameHost();
}

// static
RenderFrameHost* NavigationSimulator::NavigateAndCommitFromDocument(
    const GURL& original_url,
    RenderFrameHost* render_frame_host) {
  NavigationSimulator simulator(
      original_url, false /* browser_initiated */,
      static_cast<WebContentsImpl*>(
          WebContents::FromRenderFrameHost(render_frame_host)),
      static_cast<TestRenderFrameHost*>(render_frame_host));
  simulator.Commit();
  return simulator.GetFinalRenderFrameHost();
}

// static
RenderFrameHost* NavigationSimulator::NavigateAndFailFromBrowser(
    WebContents* web_contents,
    const GURL& url,
    int net_error_code) {
  NavigationSimulator simulator(url, true /* browser_initiated */,
                                static_cast<WebContentsImpl*>(web_contents),
                                nullptr);
  simulator.Fail(net_error_code);
  if (net_error_code == net::ERR_ABORTED)
    return nullptr;
  simulator.CommitErrorPage();
  return simulator.GetFinalRenderFrameHost();
}

// static
RenderFrameHost* NavigationSimulator::NavigateAndFailFromDocument(
    const GURL& original_url,
    int net_error_code,
    RenderFrameHost* render_frame_host) {
  NavigationSimulator simulator(
      original_url, false /* browser_initiated */,
      static_cast<WebContentsImpl*>(
          WebContents::FromRenderFrameHost(render_frame_host)),
      static_cast<TestRenderFrameHost*>(render_frame_host));
  simulator.Fail(net_error_code);
  if (net_error_code == net::ERR_ABORTED)
    return nullptr;
  simulator.CommitErrorPage();
  return simulator.GetFinalRenderFrameHost();
}

// static
std::unique_ptr<NavigationSimulator>
NavigationSimulator::CreateBrowserInitiated(const GURL& original_url,
                                            WebContents* web_contents) {
  return std::unique_ptr<NavigationSimulator>(new NavigationSimulator(
      original_url, true /* browser_initiated */,
      static_cast<WebContentsImpl*>(web_contents), nullptr));
}

// static
std::unique_ptr<NavigationSimulator>
NavigationSimulator::CreateRendererInitiated(
    const GURL& original_url,
    RenderFrameHost* render_frame_host) {
  return std::unique_ptr<NavigationSimulator>(new NavigationSimulator(
      original_url, false /* browser_initiated */,
      static_cast<WebContentsImpl*>(
          WebContents::FromRenderFrameHost(render_frame_host)),
      static_cast<TestRenderFrameHost*>(render_frame_host)));
}

NavigationSimulator::NavigationSimulator(const GURL& original_url,
                                         bool browser_initiated,
                                         WebContentsImpl* web_contents,
                                         TestRenderFrameHost* render_frame_host)
    : WebContentsObserver(web_contents),
      web_contents_(web_contents),
      render_frame_host_(render_frame_host),
      frame_tree_node_(render_frame_host
                           ? render_frame_host->frame_tree_node()
                           : web_contents->GetMainFrame()->frame_tree_node()),
      handle_(nullptr),
      navigation_url_(original_url),
      socket_address_("2001:db8::1", 80),
      browser_initiated_(browser_initiated),
      transition_(browser_initiated ? ui::PAGE_TRANSITION_TYPED
                                    : ui::PAGE_TRANSITION_LINK),
      weak_factory_(this) {
  // For renderer-initiated navigation, the RenderFrame must be initialized. Do
  // it if it hasn't happened yet.
  if (!browser_initiated)
    render_frame_host->InitializeRenderFrameIfNeeded();

  if (render_frame_host && render_frame_host->GetParent()) {
    if (!render_frame_host->frame_tree_node()->has_committed_real_load())
      transition_ = ui::PAGE_TRANSITION_AUTO_SUBFRAME;
    else
      transition_ = ui::PAGE_TRANSITION_MANUAL_SUBFRAME;
  }
}

NavigationSimulator::~NavigationSimulator() {}

void NavigationSimulator::Start() {
  CHECK(state_ == INITIALIZATION)
      << "NavigationSimulator::Start should only be called once.";
  state_ = STARTED;

  if (browser_initiated_) {
    if (!SimulateBrowserInitiatedStart())
      return;
  } else {
    if (!SimulateRendererInitiatedStart())
      return;
  }

  CHECK(handle_);
  if (IsRendererDebugURL(navigation_url_))
    return;

  if (same_document_ ||
      (IsBrowserSideNavigationEnabled() &&
       !IsURLHandledByNetworkStack(navigation_url_)) ||
      navigation_url_.IsAboutBlank()) {
    CHECK_EQ(1, num_did_start_navigation_called_);
    return;
  }

  WaitForThrottleChecksComplete();

  CHECK_EQ(1, num_did_start_navigation_called_);
  if (GetLastThrottleCheckResult().action() == NavigationThrottle::PROCEED) {
    CHECK_EQ(1, num_will_start_request_called_);
  } else {
    FailFromThrottleCheck(GetLastThrottleCheckResult());
  }
}

void NavigationSimulator::Redirect(const GURL& new_url) {
  CHECK_LE(state_, STARTED) << "NavigationSimulator::Redirect should be "
                               "called before Fail or Commit";
  CHECK_EQ(0, num_did_finish_navigation_called_)
      << "NavigationSimulator::Redirect cannot be called after the "
         "navigation has finished";

  if (state_ < STARTED) {
    Start();
    if (state_ == FAILED)
      return;
  }

  navigation_url_ = new_url;

  int previous_num_will_redirect_request_called =
      num_will_redirect_request_called_;
  int previous_did_redirect_navigation_called =
      num_did_redirect_navigation_called_;

  PrepareCompleteCallbackOnHandle();
  if (IsBrowserSideNavigationEnabled()) {
    NavigationRequest* request =
        render_frame_host_->frame_tree_node()->navigation_request();
    TestNavigationURLLoader* url_loader =
        static_cast<TestNavigationURLLoader*>(request->loader_for_testing());
    CHECK(url_loader);

    net::RedirectInfo redirect_info;
    redirect_info.status_code = 302;
    redirect_info.new_method = "GET";
    redirect_info.new_url = new_url;
    redirect_info.new_site_for_cookies = new_url;
    redirect_info.new_referrer = referrer_.url.spec();
    redirect_info.new_referrer_policy =
        Referrer::ReferrerPolicyForUrlRequest(referrer_);

    url_loader->CallOnRequestRedirected(
        redirect_info, scoped_refptr<ResourceResponse>(new ResourceResponse));
  } else {
    handle_->WillRedirectRequest(
        new_url, "GET", referrer_.url, false /* is_external_protocol */,
        scoped_refptr<net::HttpResponseHeaders>(),
        net::HttpResponseInfo::ConnectionInfo(), nullptr,
        base::Callback<void(NavigationThrottle::ThrottleCheckResult)>());
  }

  WaitForThrottleChecksComplete();

  if (GetLastThrottleCheckResult().action() == NavigationThrottle::PROCEED) {
    CHECK_EQ(previous_num_will_redirect_request_called + 1,
             num_will_redirect_request_called_);
    CHECK_EQ(previous_did_redirect_navigation_called + 1,
             num_did_redirect_navigation_called_);
  } else {
    FailFromThrottleCheck(GetLastThrottleCheckResult());
  }
}

void NavigationSimulator::ReadyToCommit() {
  CHECK_LE(state_, STARTED) << "NavigationSimulator::ReadyToCommit can only "
                               "be called once, and cannot be called after "
                               "NavigationSimulator::Fail";
  CHECK_EQ(0, num_did_finish_navigation_called_)
      << "NavigationSimulator::ReadyToCommit cannot be called after the "
         "navigation has finished";

  if (state_ < STARTED) {
    Start();
    if (state_ == FAILED)
      return;
  }

  if (!IsBrowserSideNavigationEnabled() && same_document_) {
    CommitSameDocument();
    return;
  }

  PrepareCompleteCallbackOnHandle();
  if (IsBrowserSideNavigationEnabled()) {
    if (frame_tree_node_->navigation_request()) {
      static_cast<TestRenderFrameHost*>(frame_tree_node_->current_frame_host())
          ->PrepareForCommit();
    }

    // Synchronous failure can cause the navigation to finish here.
    if (!handle_) {
      state_ = FAILED;
      return;
    }
  }

  // Call NavigationHandle::WillProcessResponse if needed.
  // Note that the handle's state can be CANCELING if a throttle cancelled it
  // synchronously in PrepareForCommit.
  if (handle_->state_for_testing() < NavigationHandleImpl::CANCELING) {
    // This code path should only be executed when browser-side navigation isn't
    // enabled. When browser-side navigation is enabled, WillProcessResponse
    // gets invoked via the call to PrepareForCommit() above.
    DCHECK(!IsBrowserSideNavigationEnabled());

    // Start the request_ids at 1000 to avoid collisions with request ids from
    // network resources (it should be rare to compare these in unit tests).
    static int request_id = 1000;
    GlobalRequestID global_id(render_frame_host_->GetProcess()->GetID(),
                              ++request_id);
    handle_->WillProcessResponse(
        render_frame_host_, scoped_refptr<net::HttpResponseHeaders>(),
        net::HttpResponseInfo::ConnectionInfo(), SSLStatus(), global_id,
        false /* should_replace_current_entry */, false /* is_download */,
        false /* is_stream */, base::Closure(),
        base::Callback<void(NavigationThrottle::ThrottleCheckResult)>());
  }

  if (!same_document_ && !IsRendererDebugURL(navigation_url_) &&
      !navigation_url_.IsAboutBlank() &&
      (!IsBrowserSideNavigationEnabled() ||
       IsURLHandledByNetworkStack(navigation_url_))) {
    WaitForThrottleChecksComplete();

    if (GetLastThrottleCheckResult().action() != NavigationThrottle::PROCEED) {
      FailFromThrottleCheck(GetLastThrottleCheckResult());
      return;
    }
    CHECK_EQ(1, num_will_process_response_called_);
    CHECK_EQ(1, num_ready_to_commit_called_);
  }


  request_id_ = handle_->GetGlobalRequestID();

  // Update the RenderFrameHost now that we know which RenderFrameHost will
  // commit the navigation.
  TestRenderFrameHost* new_render_frame_host =
      static_cast<TestRenderFrameHost*>(handle_->GetRenderFrameHost());
  if (!IsBrowserSideNavigationEnabled() &&
      new_render_frame_host != render_frame_host_) {
    CHECK(handle_->is_transferring());
    // Simulate the renderer transfer.
    new_render_frame_host->OnMessageReceived(FrameHostMsg_DidStartLoading(
        new_render_frame_host->GetRoutingID(), true));
    new_render_frame_host->OnMessageReceived(
        FrameHostMsg_DidStartProvisionalLoad(
            new_render_frame_host->GetRoutingID(), navigation_url_,
            std::vector<GURL>(), base::TimeTicks::Now()));
    CHECK(!handle_->is_transferring());
  }
  render_frame_host_ = new_render_frame_host;
  state_ = READY_TO_COMMIT;
}

void NavigationSimulator::Commit() {
  CHECK_LE(state_, READY_TO_COMMIT) << "NavigationSimulator::Commit can only "
                                       "be called once, and cannot be called "
                                       "after NavigationSimulator::Fail";
  CHECK_EQ(0, num_did_finish_navigation_called_)
      << "NavigationSimulator::Commit cannot be called after the navigation "
         "has finished";

  if (state_ < READY_TO_COMMIT) {
    ReadyToCommit();
    if (state_ == FAILED || state_ == FINISHED)
      return;
  }

  // Keep a pointer to the current RenderFrameHost that may be pending deletion
  // after commit.
  RenderFrameHostImpl* previous_rfh =
      render_frame_host_->frame_tree_node()->current_frame_host();

  FrameHostMsg_DidCommitProvisionalLoad_Params params;
  params.nav_entry_id = handle_->pending_nav_entry_id();
  params.url = navigation_url_;
  params.origin = url::Origin(navigation_url_);
  params.transition = transition_;
  params.should_update_history = true;
  params.did_create_new_entry = !ui::PageTransitionCoreTypeIs(
      transition_, ui::PAGE_TRANSITION_AUTO_SUBFRAME);
  params.gesture =
      has_user_gesture_ ? NavigationGestureUser : NavigationGestureAuto;
  params.contents_mime_type = "text/html";
  params.method = "GET";
  params.http_status_code = 200;
  params.socket_address = socket_address_;
  params.history_list_was_cleared = false;
  params.original_request_url = navigation_url_;
  params.was_within_same_document = same_document_;

  // Simulate Blink assigning an item and document sequence number to the
  // navigation.
  params.item_sequence_number = base::Time::Now().ToDoubleT() * 1000000;
  params.document_sequence_number = params.item_sequence_number + 1;

  params.page_state = PageState::CreateForTestingWithSequenceNumbers(
      navigation_url_, params.item_sequence_number,
      params.document_sequence_number);

  render_frame_host_->SendNavigateWithParams(&params);

  // Simulate the UnloadACK in the old RenderFrameHost if it was swapped out at
  // commit time.
  if (previous_rfh != render_frame_host_) {
    previous_rfh->OnMessageReceived(
        FrameHostMsg_SwapOut_ACK(previous_rfh->GetRoutingID()));
  }

  state_ = FINISHED;

  if (!IsRendererDebugURL(navigation_url_))
    CHECK_EQ(1, num_did_finish_navigation_called_);
}

void NavigationSimulator::Fail(int error_code) {
  CHECK_LE(state_, STARTED) << "NavigationSimulator::Fail can only be "
                               "called once, and cannot be called after "
                               "NavigationSimulator::ReadyToCommit";
  CHECK_EQ(0, num_did_finish_navigation_called_)
      << "NavigationSimulator::Fail cannot be called after the "
         "navigation has finished";

  if (state_ == INITIALIZATION)
    Start();

  state_ = FAILED;

  bool should_result_in_error_page = error_code != net::ERR_ABORTED;
  if (IsBrowserSideNavigationEnabled()) {
    NavigationRequest* request = frame_tree_node_->navigation_request();
    CHECK(request);
    TestNavigationURLLoader* url_loader =
        static_cast<TestNavigationURLLoader*>(request->loader_for_testing());
    CHECK(url_loader);
    url_loader->SimulateError(error_code);
  } else {
    FrameHostMsg_DidFailProvisionalLoadWithError_Params error_params;
    error_params.error_code = error_code;
    error_params.url = navigation_url_;
    render_frame_host_->OnMessageReceived(
        FrameHostMsg_DidFailProvisionalLoadWithError(
            render_frame_host_->GetRoutingID(), error_params));
    if (!should_result_in_error_page) {
      render_frame_host_->OnMessageReceived(
          FrameHostMsg_DidStopLoading(render_frame_host_->GetRoutingID()));
    }
  }

  if (IsBrowserSideNavigationEnabled()) {
    if (should_result_in_error_page) {
      CHECK_EQ(1, num_ready_to_commit_called_);
      // Update the RenderFrameHost now that we know which RenderFrameHost will
      // commit the error page.
      render_frame_host_ =
          static_cast<TestRenderFrameHost*>(handle_->GetRenderFrameHost());
    }
  }

  if (should_result_in_error_page)
    CHECK_EQ(0, num_did_finish_navigation_called_);
  else
    CHECK_EQ(1, num_did_finish_navigation_called_);
}

void NavigationSimulator::CommitErrorPage() {
  CHECK_EQ(FAILED, state_)
      << "NavigationSimulator::CommitErrorPage can only be "
         "called once, and should be called after Fail "
         "has been called";
  CHECK_EQ(0, num_did_finish_navigation_called_)
      << "NavigationSimulator::CommitErrorPage cannot be called after the "
         "navigation has finished";

  // Keep a pointer to the current RenderFrameHost that may be pending deletion
  // after commit.
  RenderFrameHostImpl* previous_rfh =
      render_frame_host_->frame_tree_node()->current_frame_host();

  GURL error_url = GURL(kUnreachableWebDataURL);
  render_frame_host_->OnMessageReceived(FrameHostMsg_DidStartProvisionalLoad(
      render_frame_host_->GetRoutingID(), error_url, std::vector<GURL>(),
      base::TimeTicks::Now()));
  FrameHostMsg_DidCommitProvisionalLoad_Params params;
  params.nav_entry_id = handle_->pending_nav_entry_id();
  params.did_create_new_entry = !ui::PageTransitionCoreTypeIs(
      transition_, ui::PAGE_TRANSITION_AUTO_SUBFRAME);
  params.url = navigation_url_;
  params.transition = transition_;
  params.was_within_same_document = false;
  params.url_is_unreachable = true;

  // Simulate Blink assigning an item and document sequence number to the
  // navigation.
  params.item_sequence_number = base::Time::Now().ToDoubleT() * 1000000;
  params.document_sequence_number = params.item_sequence_number + 1;

  params.page_state = PageState::CreateForTestingWithSequenceNumbers(
      navigation_url_, params.item_sequence_number,
      params.document_sequence_number);

  render_frame_host_->SendNavigateWithParams(&params);

  // Simulate the UnloadACK in the old RenderFrameHost if it was swapped out at
  // commit time.
  if (previous_rfh != render_frame_host_) {
    previous_rfh->OnMessageReceived(
        FrameHostMsg_SwapOut_ACK(previous_rfh->GetRoutingID()));
  }

  state_ = FINISHED;

  CHECK_EQ(1, num_did_finish_navigation_called_);
}

void NavigationSimulator::CommitSameDocument() {
  if (!browser_initiated_) {
    CHECK_EQ(INITIALIZATION, state_)
        << "NavigationSimulator::CommitErrorPage should be the only "
           "navigation event function called on the NavigationSimulator";
  } else {
    CHECK(same_document_);
    CHECK_EQ(STARTED, state_);
  }

  render_frame_host_->OnMessageReceived(
      FrameHostMsg_DidStartLoading(render_frame_host_->GetRoutingID(), false));

  FrameHostMsg_DidCommitProvisionalLoad_Params params;
  params.nav_entry_id = 0;
  params.url = navigation_url_;
  params.origin = url::Origin(navigation_url_);
  params.transition = transition_;
  params.should_update_history = true;
  params.did_create_new_entry = false;
  params.gesture =
      has_user_gesture_ ? NavigationGestureUser : NavigationGestureAuto;
  params.contents_mime_type = "text/html";
  params.method = "GET";
  params.http_status_code = 200;
  params.socket_address = socket_address_;
  params.history_list_was_cleared = false;
  params.original_request_url = navigation_url_;
  params.was_within_same_document = true;
  params.page_state =
      PageState::CreateForTesting(navigation_url_, false, nullptr, nullptr);

  render_frame_host_->SendNavigateWithParams(&params);

  state_ = FINISHED;

  CHECK_EQ(1, num_did_start_navigation_called_);
  CHECK_EQ(0, num_will_start_request_called_);
  CHECK_EQ(0, num_will_process_response_called_);
  CHECK_EQ(0, num_ready_to_commit_called_);
  CHECK_EQ(1, num_did_finish_navigation_called_);
}

void NavigationSimulator::SetTransition(ui::PageTransition transition) {
  CHECK_EQ(INITIALIZATION, state_)
      << "The transition cannot be set after the navigation has started";
  transition_ = transition;
}

void NavigationSimulator::SetHasUserGesture(bool has_user_gesture) {
  CHECK_EQ(INITIALIZATION, state_) << "The has_user_gesture parameter cannot "
                                      "be set after the navigation has started";
  has_user_gesture_ = has_user_gesture;
}

void NavigationSimulator::SetReferrer(const Referrer& referrer) {
  CHECK_LE(state_, STARTED) << "The referrer cannot be set after the "
                               "navigation has committed or has failed";
  referrer_ = referrer;
}

void NavigationSimulator::SetSocketAddress(
    const net::HostPortPair& socket_address) {
  CHECK_LE(state_, STARTED) << "The socket address cannot be set after the "
                               "navigation has committed or failed";
  socket_address_ = socket_address;
}

NavigationThrottle::ThrottleCheckResult
NavigationSimulator::GetLastThrottleCheckResult() {
  return last_throttle_check_result_.value();
}

NavigationHandle* NavigationSimulator::GetNavigationHandle() const {
  CHECK_EQ(STARTED, state_);
  return handle_;
}

content::GlobalRequestID NavigationSimulator::GetGlobalRequestID() const {
  CHECK_GT(state_, STARTED) << "The GlobalRequestID is not available until "
                               "after the navigation has completed "
                               "WillProcessResponse";
  return request_id_;
}

void NavigationSimulator::SetOnDeferCallback(
    const base::Closure& on_defer_callback) {
  CHECK_LT(state_, FINISHED)
      << "The callback should not be set after the navigation has finished";
  if (handle_) {
    handle_->SetOnDeferCallbackForTesting(on_defer_callback);
    return;
  }

  // If there is no NavigationHandle for the navigation yet, store the callback
  // until one has been created.
  on_defer_callback_ = on_defer_callback;
}

void NavigationSimulator::DidStartNavigation(
    NavigationHandle* navigation_handle) {
  // Check if this navigation is the one we're simulating.
  if (handle_)
    return;

  NavigationHandleImpl* handle =
      static_cast<NavigationHandleImpl*>(navigation_handle);

  if (handle->frame_tree_node() != frame_tree_node_)
    return;

  if (!IsBrowserSideNavigationEnabled() &&
      navigation_handle->GetURL() != navigation_url_) {
    return;
  }

  handle_ = handle;

  num_did_start_navigation_called_++;

  // Add a throttle to count NavigationThrottle calls count.
  handle->RegisterThrottleForTesting(
      base::MakeUnique<NavigationThrottleCallbackRunner>(
          handle,
          base::Bind(&NavigationSimulator::OnWillStartRequest,
                     weak_factory_.GetWeakPtr()),
          base::Bind(&NavigationSimulator::OnWillRedirectRequest,
                     weak_factory_.GetWeakPtr()),
          base::Bind(&NavigationSimulator::OnWillProcessResponse,
                     weak_factory_.GetWeakPtr())));

  // Pass the |on_defer_callback_| if it was registered.
  if (!on_defer_callback_.is_null()) {
    handle->SetOnDeferCallbackForTesting(on_defer_callback_);
    on_defer_callback_.Reset();
  }

  PrepareCompleteCallbackOnHandle();
}

void NavigationSimulator::DidRedirectNavigation(
    NavigationHandle* navigation_handle) {
  if (navigation_handle == handle_)
    num_did_redirect_navigation_called_++;
}

void NavigationSimulator::ReadyToCommitNavigation(
    NavigationHandle* navigation_handle) {
  if (navigation_handle == handle_)
    num_ready_to_commit_called_++;
}

void NavigationSimulator::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  if (navigation_handle == handle_) {
    num_did_finish_navigation_called_++;
    handle_ = nullptr;
  }
}

void NavigationSimulator::OnWillStartRequest() {
  num_will_start_request_called_++;
}

void NavigationSimulator::OnWillRedirectRequest() {
  num_will_redirect_request_called_++;
}

void NavigationSimulator::OnWillProcessResponse() {
  num_will_process_response_called_++;
}

bool NavigationSimulator::SimulateBrowserInitiatedStart() {
  web_contents_->GetController().LoadURL(navigation_url_, referrer_,
                                         transition_, std::string());

  // The navigation url might have been rewritten by the NavigationController.
  // Update it.
  navigation_url_ = web_contents_->GetController().GetPendingEntry()->GetURL();

  if (!IsBrowserSideNavigationEnabled()) {
    // Update the RenderFrameHost for navigation.
    render_frame_host_ = static_cast<TestRenderFrameHost*>(
        frame_tree_node_->render_manager()->pending_frame_host());
    if (!render_frame_host_) {
      render_frame_host_ =
          static_cast<TestRenderFrameHost*>(web_contents_->GetMainFrame());
    }
    CHECK(render_frame_host_);

    // Simulate the BeforeUnloadACK if needed.
    if (web_contents_->GetMainFrame()->is_waiting_for_beforeunload_ack()) {
      static_cast<TestRenderFrameHost*>(web_contents_->GetMainFrame())
          ->SendBeforeUnloadACK(true /*proceed */);
    }

    // If this is a same-document navigation, there is no need to simulate
    // anything else.
    if (CheckIfSameDocument()) {
      same_document_ = true;
      return false;
    }

    // From there on, the calls are similar to a renderer-initiated navigation.
    return SimulateRendererInitiatedStart();
  }

  // Simulate the BeforeUnload ACK if needed.
  NavigationRequest* request = frame_tree_node_->navigation_request();
  if (request &&
      request->state() == NavigationRequest::WAITING_FOR_RENDERER_RESPONSE) {
    static_cast<TestRenderFrameHost*>(web_contents_->GetMainFrame())
        ->SendBeforeUnloadACK(true /*proceed */);
  }

  // Note: WillStartRequest checks can destroy the request synchronously, or
  // this can be a navigation that doesn't need a network request and that was
  // passed directly to a RenderFrameHost for commit.
  request =
      web_contents_->GetMainFrame()->frame_tree_node()->navigation_request();
  if (!request) {
    if (web_contents_->GetMainFrame()->navigation_handle() == handle_) {
      DCHECK(handle_->IsSameDocument() ||
             !IsURLHandledByNetworkStack(handle_->GetURL()));
      same_document_ = handle_->IsSameDocument();
      return true;
    }
    return false;
  } else if (IsRendererDebugURL(navigation_url_)) {
    // There will be no DidStartNavigation for renderer-debug URLs. Register the
    // NavigationHandle now.
    handle_ = request->navigation_handle();
  }

  DCHECK_EQ(handle_, request->navigation_handle());
  return true;
}

bool NavigationSimulator::SimulateRendererInitiatedStart() {
  if (IsBrowserSideNavigationEnabled()) {
    BeginNavigationParams begin_params(
        std::string(), net::LOAD_NORMAL, has_user_gesture_,
        false /* skip_service_worker */, REQUEST_CONTEXT_TYPE_HYPERLINK,
        blink::WebMixedContentContextType::kBlockable,
        false,  // is_form_submission
        url::Origin());
    CommonNavigationParams common_params;
    common_params.url = navigation_url_;
    common_params.referrer = referrer_;
    common_params.transition = transition_;
    common_params.navigation_type =
        PageTransitionCoreTypeIs(transition_, ui::PAGE_TRANSITION_RELOAD)
            ? FrameMsg_Navigate_Type::RELOAD
            : FrameMsg_Navigate_Type::DIFFERENT_DOCUMENT;
    render_frame_host_->OnMessageReceived(FrameHostMsg_BeginNavigation(
        render_frame_host_->GetRoutingID(), common_params, begin_params));
    NavigationRequest* request =
        render_frame_host_->frame_tree_node()->navigation_request();

    // The request failed synchronously.
    if (!request)
      return false;

    DCHECK_EQ(handle_, request->navigation_handle());
    return true;
  }

  render_frame_host_->OnMessageReceived(
      FrameHostMsg_DidStartLoading(render_frame_host_->GetRoutingID(), true));
  render_frame_host_->OnMessageReceived(FrameHostMsg_DidStartProvisionalLoad(
      render_frame_host_->GetRoutingID(), navigation_url_, std::vector<GURL>(),
      base::TimeTicks::Now()));
  if (IsRendererDebugURL(navigation_url_)) {
    // DidStartNavigation was not fired in that case.
    handle_ = render_frame_host_->navigation_handle();
  }
  DCHECK_EQ(handle_, render_frame_host_->navigation_handle());
  // Note: When PlzNavigate is enabled, WillStartRequest will have been fired as
  // part of the processing of BeginNavigation. When not enabled, simulate the
  // ResourceRequest having been received on the IO thread.
  handle_->WillStartRequest(
      "GET", scoped_refptr<content::ResourceRequestBody>(), referrer_,
      has_user_gesture_, transition_, false /* is_external_protocol */,
      REQUEST_CONTEXT_TYPE_LOCATION,
      blink::WebMixedContentContextType::kNotMixedContent,
      base::Callback<void(NavigationThrottle::ThrottleCheckResult)>());
  return true;
}

void NavigationSimulator::WaitForThrottleChecksComplete() {
  // If last_throttle_check_result_ is set, then throttle checks completed
  // synchronously.
  if (!last_throttle_check_result_) {
    base::RunLoop run_loop;
    throttle_checks_wait_closure_ = run_loop.QuitClosure();
    run_loop.Run();
    throttle_checks_wait_closure_.Reset();
  }

  if (IsBrowserSideNavigationEnabled()) {
    // Run message loop once since NavigationRequest::OnStartChecksComplete
    // posted a task.
    base::RunLoop().RunUntilIdle();
  }
}

void NavigationSimulator::OnThrottleChecksComplete(
    NavigationThrottle::ThrottleCheckResult result) {
  DCHECK(!last_throttle_check_result_);
  last_throttle_check_result_ = result;
  if (throttle_checks_wait_closure_)
    throttle_checks_wait_closure_.Run();
}

void NavigationSimulator::PrepareCompleteCallbackOnHandle() {
  last_throttle_check_result_.reset();
  handle_->set_complete_callback_for_testing(
      base::Bind(&NavigationSimulator::OnThrottleChecksComplete,
                 weak_factory_.GetWeakPtr()));
}

RenderFrameHost* NavigationSimulator::GetFinalRenderFrameHost() {
  CHECK_GE(state_, READY_TO_COMMIT);
  return render_frame_host_;
}

void NavigationSimulator::FailFromThrottleCheck(
    NavigationThrottle::ThrottleCheckResult result) {
  DCHECK_NE(NavigationThrottle::PROCEED, result.action());
  state_ = FAILED;

  // Special failure logic only needed for non-PlzNavigate case.
  if (IsBrowserSideNavigationEnabled())
    return;
  DCHECK_NE(NavigationThrottle::DEFER, result.action());
  DCHECK_NE(NavigationThrottle::PROCEED, result.action());
  DCHECK_NE(net::OK, result.net_error_code());

  FrameHostMsg_DidFailProvisionalLoadWithError_Params error_params;
  error_params.error_code = result.net_error_code();
  error_params.url = navigation_url_;
  render_frame_host_->OnMessageReceived(
      FrameHostMsg_DidFailProvisionalLoadWithError(
          render_frame_host_->GetRoutingID(), error_params));
  bool should_result_in_error_page =
      result.net_error_code() != net::ERR_ABORTED;
  if (!should_result_in_error_page) {
    render_frame_host_->OnMessageReceived(
        FrameHostMsg_DidStopLoading(render_frame_host_->GetRoutingID()));
    CHECK_EQ(1, num_did_finish_navigation_called_);
  } else {
    CHECK_EQ(0, num_did_finish_navigation_called_);
  }
}

bool NavigationSimulator::CheckIfSameDocument() {
  // This approach to determining whether a navigation is to be treated as
  // same document is not robust, as it will not handle pushState type
  // navigation. Do not use elsewhere!

  // First we need a valid document that is not an error page.
  if (!render_frame_host_->GetLastCommittedURL().is_valid() ||
      render_frame_host_->last_commit_was_error_page()) {
    return false;
  }

  // Exclude reloads.
  if (ui::PageTransitionCoreTypeIs(transition_, ui::PAGE_TRANSITION_RELOAD)) {
    return false;
  }

  // A browser-initiated navigation to the exact same url in the address bar is
  // not a same document navigation.
  if (browser_initiated_ &&
      render_frame_host_->GetLastCommittedURL() == navigation_url_) {
    return false;
  }

  // Finally, the navigation url and the last committed url should match,
  // except for the fragment.
  GURL url_copy(navigation_url_);
  url::Replacements<char> replacements;
  replacements.ClearRef();
  return url_copy.ReplaceComponents(replacements) ==
         render_frame_host_->GetLastCommittedURL().ReplaceComponents(
             replacements);
}

}  // namespace content
