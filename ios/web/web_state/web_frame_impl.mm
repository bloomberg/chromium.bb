// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/web_state/web_frame_impl.h"

#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

WebFrameImpl::WebFrameImpl(const std::string& frame_id,
                           std::unique_ptr<crypto::SymmetricKey> frame_key,
                           bool is_main_frame,
                           GURL security_origin,
                           web::WebState* web_state)
    : frame_id_(frame_id),
      frame_key_(std::move(frame_key)),
      is_main_frame_(is_main_frame),
      security_origin_(security_origin),
      web_state_(web_state) {
  DCHECK(frame_key_);
}

WebFrameImpl::~WebFrameImpl() = default;

WebState* WebFrameImpl::GetWebState() {
  return web_state_;
}

std::string WebFrameImpl::GetFrameId() const {
  return frame_id_;
}

bool WebFrameImpl::IsMainFrame() const {
  return is_main_frame_;
}

GURL WebFrameImpl::GetSecurityOrigin() const {
  return security_origin_;
}

}  // namespace web
