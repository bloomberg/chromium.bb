// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/navigation_handle_impl.h"

#include "content/browser/frame_host/navigator_delegate.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "net/url_request/redirect_info.h"

namespace content {

// static
scoped_ptr<NavigationHandleImpl> NavigationHandleImpl::Create(
    const GURL& url,
    bool is_main_frame,
    NavigatorDelegate* delegate) {
  return scoped_ptr<NavigationHandleImpl>(
      new NavigationHandleImpl(url, is_main_frame, delegate));
}

NavigationHandleImpl::NavigationHandleImpl(const GURL& url,
                                           const bool is_main_frame,
                                           NavigatorDelegate* delegate)
    : url_(url),
      is_main_frame_(is_main_frame),
      is_post_(false),
      has_user_gesture_(false),
      transition_(ui::PAGE_TRANSITION_LINK),
      is_external_protocol_(false),
      net_error_code_(net::OK),
      render_frame_host_(nullptr),
      is_same_page_(false),
      state_(INITIAL),
      is_transferring_(false),
      delegate_(delegate) {
  delegate_->DidStartNavigation(this);
}

NavigationHandleImpl::~NavigationHandleImpl() {
  delegate_->DidFinishNavigation(this);
}

const GURL& NavigationHandleImpl::GetURL() {
  return url_;
}

bool NavigationHandleImpl::IsInMainFrame() {
  return is_main_frame_;
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

bool NavigationHandleImpl::HasCommitted() {
  return state_ == DID_COMMIT || state_ == DID_COMMIT_ERROR_PAGE;
}

bool NavigationHandleImpl::IsErrorPage() {
  return state_ == DID_COMMIT_ERROR_PAGE;
}

void NavigationHandleImpl::RegisterThrottleForTesting(
    scoped_ptr<NavigationThrottle> navigation_throttle) {
  throttles_.push_back(navigation_throttle.Pass());
}

NavigationThrottle::ThrottleCheckResult
NavigationHandleImpl::CallWillStartRequestForTesting(
    bool is_post,
    const Referrer& sanitized_referrer,
    bool has_user_gesture,
    ui::PageTransition transition,
    bool is_external_protocol) {
  return WillStartRequest(is_post, sanitized_referrer, has_user_gesture,
                          transition, is_external_protocol);
}

NavigationThrottle::ThrottleCheckResult
NavigationHandleImpl::CallWillRedirectRequestForTesting(
    const GURL& new_url,
    bool new_method_is_post,
    const GURL& new_referrer_url,
    bool new_is_external_protocol) {
  return WillRedirectRequest(new_url, new_method_is_post, new_referrer_url,
                             new_is_external_protocol);
}

NavigationThrottle::ThrottleCheckResult NavigationHandleImpl::WillStartRequest(
    bool is_post,
    const Referrer& sanitized_referrer,
    bool has_user_gesture,
    ui::PageTransition transition,
    bool is_external_protocol) {
  // Update the navigation parameters.
  is_post_ = is_post;
  sanitized_referrer_ = sanitized_referrer;
  has_user_gesture_ = has_user_gesture;
  transition_ = transition;
  is_external_protocol_ = is_external_protocol;

  state_ = WILL_SEND_REQUEST;

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
  for (NavigationThrottle* throttle : throttles_) {
    NavigationThrottle::ThrottleCheckResult result =
        throttle->WillStartRequest();
    if (result == NavigationThrottle::CANCEL_AND_IGNORE)
      return NavigationThrottle::CANCEL_AND_IGNORE;
  }
  return NavigationThrottle::PROCEED;
}

NavigationThrottle::ThrottleCheckResult
NavigationHandleImpl::WillRedirectRequest(const GURL& new_url,
                                          bool new_method_is_post,
                                          const GURL& new_referrer_url,
                                          bool new_is_external_protocol) {
  // Update the navigation parameters.
  url_ = new_url;
  is_post_ = new_method_is_post;
  sanitized_referrer_.url = new_referrer_url;
  sanitized_referrer_ = Referrer::SanitizeForRequest(url_, sanitized_referrer_);
  is_external_protocol_ = new_is_external_protocol;

  // Have each throttle be notified of the request.
  for (NavigationThrottle* throttle : throttles_) {
    NavigationThrottle::ThrottleCheckResult result =
        throttle->WillRedirectRequest();
    if (result == NavigationThrottle::CANCEL_AND_IGNORE)
      return NavigationThrottle::CANCEL_AND_IGNORE;
  }
  return NavigationThrottle::PROCEED;
}

void NavigationHandleImpl::DidRedirectNavigation(const GURL& new_url) {
  url_ = new_url;
  delegate_->DidRedirectNavigation(this);
}

void NavigationHandleImpl::ReadyToCommitNavigation(
    RenderFrameHostImpl* render_frame_host) {
  CHECK(!render_frame_host_);
  render_frame_host_ = render_frame_host;
  state_ = READY_TO_COMMIT;
  delegate_->ReadyToCommitNavigation(this);
}

void NavigationHandleImpl::DidCommitNavigation(
    bool same_page,
    RenderFrameHostImpl* render_frame_host) {
  CHECK_IMPLIES(render_frame_host_, render_frame_host_ == render_frame_host);
  is_same_page_ = same_page;
  render_frame_host_ = render_frame_host;
  state_ = net_error_code_ == net::OK ? DID_COMMIT : DID_COMMIT_ERROR_PAGE;
}

}  // namespace content
