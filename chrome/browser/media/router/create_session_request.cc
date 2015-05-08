// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/create_session_request.h"

#include "chrome/browser/media/router/media_source_helper.h"

using content::PresentationSessionInfo;
using content::PresentationError;

namespace media_router {

CreateSessionRequest::CreateSessionRequest(
    const std::string& presentation_url,
    const std::string& presentation_id,
    const GURL& frame_url,
    const PresentationSessionSuccessCallback& success_cb,
    const PresentationSessionErrorCallback& error_cb)
    : presentation_info_(presentation_url, presentation_id),
      media_source_(presentation_url),
      frame_url_(frame_url),
      success_cb_(success_cb),
      error_cb_(error_cb),
      cb_invoked_(false) {
}

CreateSessionRequest::~CreateSessionRequest() {
}

void CreateSessionRequest::MaybeInvokeSuccessCallback() {
  if (!cb_invoked_) {
    success_cb_.Run(presentation_info_);
    cb_invoked_ = true;
  }
}

void CreateSessionRequest::MaybeInvokeErrorCallback(
    const content::PresentationError& error) {
  if (!cb_invoked_) {
    error_cb_.Run(error);
    cb_invoked_ = true;
  }
}

}  // namespace media_router
