// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/navigation_handle_impl.h"

#include "content/browser/frame_host/navigator_delegate.h"
#include "net/url_request/redirect_info.h"

namespace content {

// static
scoped_ptr<NavigationHandleImpl> NavigationHandleImpl::Create(
    const GURL& url,
    const bool is_main_frame,
    NavigatorDelegate* delegate) {
  return scoped_ptr<NavigationHandleImpl>(
      new NavigationHandleImpl(url, is_main_frame, delegate));
}

NavigationHandleImpl::NavigationHandleImpl(const GURL& url,
                                           const bool is_main_frame,
                                           NavigatorDelegate* delegate)
    : url_(url),
      net_error_code_(net::OK),
      state_(DID_START),
      is_main_frame_(is_main_frame),
      is_transferring_(false),
      delegate_(delegate) {
  delegate_->DidStartNavigation(this);
}

NavigationHandleImpl::~NavigationHandleImpl() {
  delegate_->DidFinishNavigation(this);
}

const GURL& NavigationHandleImpl::GetURL() const {
  return url_;
}

net::Error NavigationHandleImpl::GetNetErrorCode() const {
  return net_error_code_;
}

bool NavigationHandleImpl::IsInMainFrame() const {
  return is_main_frame_;
}

bool NavigationHandleImpl::HasCommittedDocument() const {
  return state_ == DID_COMMIT;
}

bool NavigationHandleImpl::HasCommittedErrorPage() const {
  return state_ == DID_COMMIT_ERROR_PAGE;
}

void NavigationHandleImpl::DidRedirectNavigation(const GURL& new_url) {
  url_ = new_url;
  delegate_->DidRedirectNavigation(this);
}

void NavigationHandleImpl::DidCommitNavigation() {
  state_ = net_error_code_ == net::OK ? DID_COMMIT : DID_COMMIT_ERROR_PAGE;
  delegate_->DidCommitNavigation(this);
}

}  // namespace content
