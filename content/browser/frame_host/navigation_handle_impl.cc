// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/navigation_handle_impl.h"

#include <utility>

#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/frame_host/navigator_delegate.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_navigation_handle.h"
#include "content/common/frame_messages.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/content_client.h"
#include "net/url_request/redirect_info.h"

namespace content {

namespace {

void UpdateThrottleCheckResult(
    NavigationThrottle::ThrottleCheckResult* to_update,
    NavigationThrottle::ThrottleCheckResult result) {
  *to_update = result;
}

}  // namespace

// static
scoped_ptr<NavigationHandleImpl> NavigationHandleImpl::Create(
    const GURL& url,
    FrameTreeNode* frame_tree_node,
    const base::TimeTicks& navigation_start) {
  return scoped_ptr<NavigationHandleImpl>(
      new NavigationHandleImpl(url, frame_tree_node, navigation_start));
}

NavigationHandleImpl::NavigationHandleImpl(
    const GURL& url,
    FrameTreeNode* frame_tree_node,
    const base::TimeTicks& navigation_start)
    : url_(url),
      is_post_(false),
      has_user_gesture_(false),
      transition_(ui::PAGE_TRANSITION_LINK),
      is_external_protocol_(false),
      net_error_code_(net::OK),
      render_frame_host_(nullptr),
      is_same_page_(false),
      state_(INITIAL),
      is_transferring_(false),
      frame_tree_node_(frame_tree_node),
      next_index_(0),
      navigation_start_(navigation_start) {
  DCHECK(!navigation_start.is_null());
  GetDelegate()->DidStartNavigation(this);
}

NavigationHandleImpl::~NavigationHandleImpl() {
  GetDelegate()->DidFinishNavigation(this);

  // Cancel the navigation on the IO thread if the NavigationHandle is being
  // destroyed in the middle of the NavigationThrottles checks.
  if (!IsBrowserSideNavigationEnabled() && !complete_callback_.is_null())
    RunCompleteCallback(NavigationThrottle::CANCEL_AND_IGNORE);
}

NavigatorDelegate* NavigationHandleImpl::GetDelegate() const {
  return frame_tree_node_->navigator()->GetDelegate();
}

const GURL& NavigationHandleImpl::GetURL() {
  return url_;
}

bool NavigationHandleImpl::IsInMainFrame() {
  return frame_tree_node_->IsMainFrame();
}

const base::TimeTicks& NavigationHandleImpl::NavigationStart() {
  return navigation_start_;
}

bool NavigationHandleImpl::IsPost() {
  CHECK_NE(INITIAL, state_)
      << "This accessor should not be called before the request is started.";
  return is_post_;
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
  CHECK(state_ >= READY_TO_COMMIT)
      << "This accessor should only be called "
         "after the navigation is ready to commit.";
  return render_frame_host_;
}

bool NavigationHandleImpl::IsSamePage() {
  DCHECK(state_ == DID_COMMIT || state_ == DID_COMMIT_ERROR_PAGE)
      << "This accessor should not be called before the navigation has "
         "committed.";
  return is_same_page_;
}

const net::HttpResponseHeaders* NavigationHandleImpl::GetResponseHeaders() {
  DCHECK(state_ >= WILL_REDIRECT_REQUEST)
      << "This accessor should only be called when the request encountered a "
         "redirect or received a response";
   return response_headers_.get();
}

bool NavigationHandleImpl::HasCommitted() {
  return state_ == DID_COMMIT || state_ == DID_COMMIT_ERROR_PAGE;
}

bool NavigationHandleImpl::IsErrorPage() {
  return state_ == DID_COMMIT_ERROR_PAGE;
}

void NavigationHandleImpl::Resume() {
  if (state_ != DEFERRING_START && state_ != DEFERRING_REDIRECT)
    return;

  NavigationThrottle::ThrottleCheckResult result = NavigationThrottle::DEFER;
  if (state_ == DEFERRING_START) {
    result = CheckWillStartRequest();
  } else {
    result = CheckWillRedirectRequest();
  }

  if (result != NavigationThrottle::DEFER)
    RunCompleteCallback(result);
}

void NavigationHandleImpl::CancelDeferredNavigation(
    NavigationThrottle::ThrottleCheckResult result) {
  DCHECK(state_ == DEFERRING_START || state_ == DEFERRING_REDIRECT);
  DCHECK(result == NavigationThrottle::CANCEL_AND_IGNORE ||
         result == NavigationThrottle::CANCEL);
  state_ = CANCELING;
  RunCompleteCallback(result);
}

void NavigationHandleImpl::RegisterThrottleForTesting(
    scoped_ptr<NavigationThrottle> navigation_throttle) {
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
  WillStartRequest(is_post, sanitized_referrer, has_user_gesture, transition,
                   is_external_protocol,
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
  WillRedirectRequest(new_url, new_method_is_post, new_referrer_url,
                      new_is_external_protocol,
                      scoped_refptr<net::HttpResponseHeaders>(),
                      base::Bind(&UpdateThrottleCheckResult, &result));

  // Reset the callback to ensure it will not be called later.
  complete_callback_.Reset();
  return result;
}

void NavigationHandleImpl::InitServiceWorkerHandle(
    ServiceWorkerContextWrapper* service_worker_context) {
  DCHECK(IsBrowserSideNavigationEnabled());
  service_worker_handle_.reset(
      new ServiceWorkerNavigationHandle(service_worker_context));
}

void NavigationHandleImpl::WillStartRequest(
    bool is_post,
    const Referrer& sanitized_referrer,
    bool has_user_gesture,
    ui::PageTransition transition,
    bool is_external_protocol,
    const ThrottleChecksFinishedCallback& callback) {
  // Update the navigation parameters.
  is_post_ = is_post;
  sanitized_referrer_ = sanitized_referrer;
  has_user_gesture_ = has_user_gesture;
  transition_ = transition;
  is_external_protocol_ = is_external_protocol;

  state_ = WILL_SEND_REQUEST;
  complete_callback_ = callback;

  // Register the navigation throttles. The ScopedVector returned by
  // GetNavigationThrottles is not assigned to throttles_ directly because it
  // would overwrite any throttle previously added with
  // RegisterThrottleForTesting.
  ScopedVector<NavigationThrottle> throttles_to_register =
      GetContentClient()->browser()->CreateThrottlesForNavigation(this);
  if (throttles_to_register.size() > 0) {
    throttles_.insert(throttles_.end(), throttles_to_register.begin(),
                      throttles_to_register.end());
    throttles_to_register.weak_clear();
  }

  // Notify each throttle of the request.
  NavigationThrottle::ThrottleCheckResult result = CheckWillStartRequest();

  // If the navigation is not deferred, run the callback.
  if (result != NavigationThrottle::DEFER)
    RunCompleteCallback(result);
}

void NavigationHandleImpl::WillRedirectRequest(
    const GURL& new_url,
    bool new_method_is_post,
    const GURL& new_referrer_url,
    bool new_is_external_protocol,
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    const ThrottleChecksFinishedCallback& callback) {
  // Update the navigation parameters.
  url_ = new_url;
  is_post_ = new_method_is_post;
  sanitized_referrer_.url = new_referrer_url;
  sanitized_referrer_ = Referrer::SanitizeForRequest(url_, sanitized_referrer_);
  is_external_protocol_ = new_is_external_protocol;
  response_headers_ = response_headers;

  state_ = WILL_REDIRECT_REQUEST;
  complete_callback_ = callback;

  // Notify each throttle of the request.
  NavigationThrottle::ThrottleCheckResult result = CheckWillRedirectRequest();

  // If the navigation is not deferred, run the callback.
  if (result != NavigationThrottle::DEFER)
    RunCompleteCallback(result);
}

void NavigationHandleImpl::DidRedirectNavigation(const GURL& new_url) {
  url_ = new_url;
  GetDelegate()->DidRedirectNavigation(this);
}

void NavigationHandleImpl::ReadyToCommitNavigation(
    RenderFrameHostImpl* render_frame_host,
    scoped_refptr<net::HttpResponseHeaders> response_headers) {
  DCHECK(!render_frame_host_ || render_frame_host_ == render_frame_host);
  render_frame_host_ = render_frame_host;
  response_headers_ = response_headers;
  state_ = READY_TO_COMMIT;

  // Only notify the WebContentsObservers when PlzNavigate is enabled, as
  // |render_frame_host_| may be wrong in the case of transfer navigations.
  if (IsBrowserSideNavigationEnabled())
    GetDelegate()->ReadyToCommitNavigation(this);
}

void NavigationHandleImpl::DidCommitNavigation(
    const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
    bool same_page,
    RenderFrameHostImpl* render_frame_host) {
  DCHECK(!render_frame_host_ || render_frame_host_ == render_frame_host);
  DCHECK_EQ(frame_tree_node_, render_frame_host->frame_tree_node());
  CHECK_EQ(url_, params.url);

  is_post_ = params.is_post;
  has_user_gesture_ = (params.gesture == NavigationGestureUser);
  transition_ = params.transition;
  render_frame_host_ = render_frame_host;
  is_same_page_ = same_page;

  state_ = net_error_code_ == net::OK ? DID_COMMIT : DID_COMMIT_ERROR_PAGE;
}

NavigationThrottle::ThrottleCheckResult
NavigationHandleImpl::CheckWillStartRequest() {
  DCHECK(state_ == WILL_SEND_REQUEST || state_ == DEFERRING_START);
  DCHECK(state_ != WILL_SEND_REQUEST || next_index_ == 0);
  DCHECK(state_ != DEFERRING_START || next_index_ != 0);
  for (size_t i = next_index_; i < throttles_.size(); ++i) {
    NavigationThrottle::ThrottleCheckResult result =
        throttles_[i]->WillStartRequest();
    switch (result) {
      case NavigationThrottle::PROCEED:
        continue;

      case NavigationThrottle::CANCEL:
      case NavigationThrottle::CANCEL_AND_IGNORE:
        state_ = CANCELING;
        return result;

      case NavigationThrottle::DEFER:
        state_ = DEFERRING_START;
        next_index_ = i + 1;
        return result;

      default:
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
    switch (result) {
      case NavigationThrottle::PROCEED:
        continue;

      case NavigationThrottle::CANCEL:
      case NavigationThrottle::CANCEL_AND_IGNORE:
        state_ = CANCELING;
        return result;

      case NavigationThrottle::DEFER:
        state_ = DEFERRING_REDIRECT;
        next_index_ = i + 1;
        return result;

      default:
        NOTREACHED();
    }
  }
  next_index_ = 0;
  state_ = WILL_REDIRECT_REQUEST;
  return NavigationThrottle::PROCEED;
}

void NavigationHandleImpl::RunCompleteCallback(
    NavigationThrottle::ThrottleCheckResult result) {
  DCHECK(result != NavigationThrottle::DEFER);
  if (!complete_callback_.is_null())
    complete_callback_.Run(result);

  complete_callback_.Reset();
}

}  // namespace content
