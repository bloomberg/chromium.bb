// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/find_in_page/find_in_page_manager_impl.h"

#import "ios/web/public/web_state/web_frame.h"
#import "ios/web/web_state/web_state_impl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

// static

FindInPageManagerImpl::FindInPageManagerImpl(web::WebState* web_state)
    : web_state_(web_state), weak_factory_(this) {
  web_state_->AddObserver(this);
}

void FindInPageManagerImpl::CreateForWebState(web::WebState* web_state) {
  DCHECK(web_state);
  if (!FromWebState(web_state)) {
    web_state->SetUserData(UserDataKey(),
                           std::make_unique<FindInPageManagerImpl>(web_state));
  }
}

FindInPageManagerImpl::~FindInPageManagerImpl() {
  if (web_state_) {
    web_state_->RemoveObserver(this);
    web_state_ = nullptr;
  }
}

FindInPageManagerImpl::FindRequest::FindRequest() {}

FindInPageManagerImpl::FindRequest::~FindRequest() {}

void FindInPageManagerImpl::WebFrameDidBecomeAvailable(WebState* web_state,
                                                       WebFrame* web_frame) {
  const std::string frame_id = web_frame->GetFrameId();
  last_find_request_.frame_matches_count[frame_id] = 0;
  if (web_frame->IsMainFrame()) {
    // Main frame matches should show up first.
    last_find_request_.frame_order.push_front(frame_id);
  } else {
    // The order of iframes is not important.
    last_find_request_.frame_order.push_back(frame_id);
  }
}

void FindInPageManagerImpl::WebFrameWillBecomeUnavailable(WebState* web_state,
                                                          WebFrame* web_frame) {
  last_find_request_.frame_order.remove(web_frame->GetFrameId());
  last_find_request_.frame_matches_count.erase(web_frame->GetFrameId());
}

void FindInPageManagerImpl::WebStateDestroyed(WebState* web_state) {
  web_state_->RemoveObserver(this);
  web_state_ = nullptr;
}

void FindInPageManagerImpl::Find(NSString* query, FindInPageOptions options) {}

void FindInPageManagerImpl::StopFinding() {}

}
