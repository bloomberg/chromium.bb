// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/create_presentation_session_request.h"

#include "chrome/browser/media/router/media_source_helper.h"

using content::PresentationSessionInfo;
using content::PresentationError;

namespace media_router {

CreatePresentationSessionRequest::CreatePresentationSessionRequest(
    const std::string& presentation_url,
    const GURL& frame_url,
    const PresentationSessionSuccessCallback& success_cb,
    const PresentationSessionErrorCallback& error_cb)
    : media_source_(MediaSourceForPresentationUrl(presentation_url)),
      frame_url_(frame_url),
      success_cb_(success_cb),
      error_cb_(error_cb),
      cb_invoked_(false) {
  DCHECK(!success_cb.is_null());
  DCHECK(!error_cb.is_null());
}

CreatePresentationSessionRequest::~CreatePresentationSessionRequest() {
  if (!cb_invoked_) {
    error_cb_.Run(content::PresentationError(
        content::PRESENTATION_ERROR_UNKNOWN, "Unknown error."));
  }
}

void CreatePresentationSessionRequest::MaybeInvokeSuccessCallback(
    const std::string& presentation_id,
    const MediaRoute::Id& route_id) {
  DCHECK(!cb_invoked_);
  if (!cb_invoked_) {
    // Overwrite presentation ID.
    success_cb_.Run(
        content::PresentationSessionInfo(
            PresentationUrlFromMediaSource(media_source_), presentation_id),
        route_id);
    cb_invoked_ = true;
  }
}

void CreatePresentationSessionRequest::MaybeInvokeErrorCallback(
    const content::PresentationError& error) {
  DCHECK(!cb_invoked_);
  if (!cb_invoked_) {
    error_cb_.Run(error);
    cb_invoked_ = true;
  }
}

}  // namespace media_router
