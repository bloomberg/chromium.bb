// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/web_state/web_frames_manager_impl.h"

#include "base/strings/utf_string_conversions.h"
#include "ios/web/public/web_state/web_frame.h"
#include "ios/web/public/web_state/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Matcher for finding a WebFrame object with the given |frame_id|.
struct FrameIdMatcher {
  explicit FrameIdMatcher(const std::string& frame_id) : frame_id_(frame_id) {}

  // Returns true if the receiver was initialized with the same frame id as
  // |web_frame|.
  bool operator()(web::WebFrame* web_frame) {
    return web_frame->GetFrameId() == frame_id_;
  }

 private:
  // The frame id to match against.
  const std::string frame_id_;
};
}  // namespace

namespace web {

DEFINE_WEB_STATE_USER_DATA_KEY(WebFramesManagerImpl);

WebFramesManagerImpl::~WebFramesManagerImpl() = default;

WebFramesManagerImpl::WebFramesManagerImpl(web::WebState* web_state)
    : web_state_(web_state) {}

void WebFramesManagerImpl::AddFrame(std::unique_ptr<WebFrame> frame) {
  if (frame->IsMainFrame()) {
    main_web_frame_ = frame.get();
  }
  web_frame_ptrs_.push_back(frame.get());
  web_frames_.push_back(std::move(frame));
}

void WebFramesManagerImpl::RemoveFrameWithId(const std::string& frame_id) {
  if (main_web_frame_ && main_web_frame_->GetFrameId() == frame_id) {
    main_web_frame_ = nullptr;
  }

  auto web_frame_ptrs_it = std::find_if(
      web_frame_ptrs_.begin(), web_frame_ptrs_.end(), FrameIdMatcher(frame_id));
  if (web_frame_ptrs_it != web_frame_ptrs_.end()) {
    web_frame_ptrs_.erase(web_frame_ptrs_it);
    web_frames_.erase(web_frames_.begin() +
                      (web_frame_ptrs_it - web_frame_ptrs_.begin()));
  }
}

void WebFramesManagerImpl::RemoveAllWebFrames() {
  main_web_frame_ = nullptr;
  web_frames_.clear();
  web_frame_ptrs_.clear();
}

WebFrame* WebFramesManagerImpl::GetFrameWithId(const std::string& frame_id) {
  auto web_frame_ptrs_it = std::find_if(
      web_frame_ptrs_.begin(), web_frame_ptrs_.end(), FrameIdMatcher(frame_id));
  return web_frame_ptrs_it == web_frame_ptrs_.end() ? nullptr
                                                    : *web_frame_ptrs_it;
}

const std::vector<WebFrame*>& WebFramesManagerImpl::GetAllWebFrames() {
  return web_frame_ptrs_;
}

WebFrame* WebFramesManagerImpl::GetMainWebFrame() {
  return main_web_frame_;
}

void WebFramesManagerImpl::RegisterExistingFrames() {
  web_state_->ExecuteJavaScript(
      base::UTF8ToUTF16("__gCrWeb.frameMessaging.getExistingFrames();"));
}

}  // namespace
