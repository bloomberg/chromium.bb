// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_CREATE_PRESENTATION_SESSION_REQUEST_H_
#define CHROME_BROWSER_MEDIA_ROUTER_CREATE_PRESENTATION_SESSION_REQUEST_H_

#include <string>

#include "chrome/browser/media/router/media_route.h"
#include "chrome/browser/media/router/media_source.h"
#include "content/public/browser/presentation_service_delegate.h"
#include "url/gurl.h"

namespace content {
struct PresentationError;
struct PresentationSessionInfo;
}  // namespace content

namespace media_router {

// Holds parameters for creating a presentation session.
// A request object is created by presentation_service_delegate_impl when it
// gets create-session request. The object is then passed to and owned by the
// MediaRouterUI. |success_cb| will be invoked when create-session
// succeeds, or |error_cb| will be invoked when create-session fails or
// the UI closes.
class CreatePresentationSessionRequest {
 public:
  using PresentationSessionSuccessCallback =
      base::Callback<void(const content::PresentationSessionInfo&,
                          const MediaRoute::Id&)>;
  using PresentationSessionErrorCallback =
      content::PresentationServiceDelegate::PresentationSessionErrorCallback;

  // |presentation_url|: The presentation URL of the request. Must be a valid
  //                     URL.
  // |frame_url|: The URL of the frame that initiated the presentation request.
  // |success_cb|: Callback to invoke when the request succeeds. Must be valid.
  // |erorr_cb|: Callback to invoke when the request fails. Must be valid.
  CreatePresentationSessionRequest(
      const std::string& presentation_url,
      const GURL& frame_url,
      const PresentationSessionSuccessCallback& success_cb,
      const PresentationSessionErrorCallback& error_cb);
  ~CreatePresentationSessionRequest();

  const MediaSource& media_source() const { return media_source_; }
  const GURL& frame_url() const { return frame_url_; }

  // Invokes |success_cb_| or |error_cb_| with the given arguments.
  // These functions can only be invoked once per instance. Further invocations
  // are no-op.
  void MaybeInvokeSuccessCallback(const std::string& presentation_id,
                                  const MediaRoute::Id& route_id);
  void MaybeInvokeErrorCallback(const content::PresentationError& error);

 private:
  const MediaSource media_source_;
  const GURL frame_url_;
  PresentationSessionSuccessCallback success_cb_;
  PresentationSessionErrorCallback error_cb_;
  bool cb_invoked_;

  DISALLOW_COPY_AND_ASSIGN(CreatePresentationSessionRequest);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_CREATE_PRESENTATION_SESSION_REQUEST_H_
