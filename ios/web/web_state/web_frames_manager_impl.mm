// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/web_state/web_frames_manager_impl.h"

#include "ios/web/public/web_state/web_frame.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

DEFINE_WEB_STATE_USER_DATA_KEY(WebFramesManagerImpl);

WebFramesManagerImpl::~WebFramesManagerImpl() = default;

WebFramesManagerImpl::WebFramesManagerImpl(web::WebState* web_state) {}

void WebFramesManagerImpl::AddFrame(WebFrame& frame) {
  web_frames_.push_back(&frame);
  if (frame.IsMainFrame()) {
    main_web_frame_ = &frame;
  }
}

void WebFramesManagerImpl::RemoveFrame(const std::string& frame_id) {
  std::vector<WebFrame*>::const_iterator it;
  for (it = web_frames_.begin(); it != web_frames_.end(); ++it) {
    if ((*it)->GetFrameId() == frame_id) {
      web_frames_.erase(it);
      break;
    }
  }

  if (main_web_frame_ && main_web_frame_->GetFrameId() == frame_id) {
    main_web_frame_ = nullptr;
  }
}

const std::vector<WebFrame*>& WebFramesManagerImpl::GetAllWebFrames() {
  return web_frames_;
}

WebFrame* WebFramesManagerImpl::GetMainWebFrame() {
  return main_web_frame_;
}

}  // namespace
