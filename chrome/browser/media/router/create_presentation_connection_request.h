// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_CREATE_PRESENTATION_CONNECTION_REQUEST_H_
#define CHROME_BROWSER_MEDIA_ROUTER_CREATE_PRESENTATION_CONNECTION_REQUEST_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "content/public/browser/presentation_request.h"
#include "content/public/browser/presentation_service_delegate.h"

namespace content {
struct PresentationError;
struct PresentationInfo;
}  // namespace content

namespace media_router {

class MediaRoute;
class RouteRequestResult;

// Holds parameters for creating a presentation.  A request object is created by
// presentation_service_delegate_impl, which is then passed to and owned by the
// MediaRouterUI. |success_cb| will be invoked when create-session succeeds, or
// |error_cb| will be invoked when create-session fails or the UI closes.
// TODO(mfoltz): Combine this with PresentationRequest as it's largely
// redundant.
class CreatePresentationConnectionRequest {
 public:
  using PresentationConnectionCallback =
      base::OnceCallback<void(const content::PresentationInfo&,
                              const MediaRoute&)>;
  using PresentationConnectionErrorCallback =
      content::PresentationConnectionErrorCallback;
  // |presentation_request|: Request object containing the presentation URLs,
  //     and the ID and origin of the initiating frame.
  // |success_cb|: Callback to invoke when the request succeeds. Must be valid.
  // |error_cb|: Callback to invoke when the request fails. Must be valid.
  CreatePresentationConnectionRequest(
      const content::PresentationRequest& presentation_request,
      PresentationConnectionCallback success_cb,
      PresentationConnectionErrorCallback error_cb);
  ~CreatePresentationConnectionRequest();

  const content::PresentationRequest& presentation_request() const {
    return presentation_request_;
  }

  // Invokes |success_cb_| or |error_cb_| with the given arguments.
  // These functions can only be invoked once per instance. It is an error
  // to invoke these functions more than once.
  void InvokeSuccessCallback(const std::string& presentation_id,
                             const GURL& presentation_url,
                             const MediaRoute& route);
  void InvokeErrorCallback(const content::PresentationError& error);

  // Handle route creation/joining response by invoking the right callback.
  static void HandleRouteResponse(
      std::unique_ptr<CreatePresentationConnectionRequest> presentation_request,
      const RouteRequestResult& result);

 private:
  content::PresentationRequest presentation_request_;
  PresentationConnectionCallback success_cb_;
  PresentationConnectionErrorCallback error_cb_;
  bool cb_invoked_;

  DISALLOW_COPY_AND_ASSIGN(CreatePresentationConnectionRequest);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_CREATE_PRESENTATION_CONNECTION_REQUEST_H_
