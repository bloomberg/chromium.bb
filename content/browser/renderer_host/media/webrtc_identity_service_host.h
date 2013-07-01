// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_WEBRTC_IDENTITY_SERVICE_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_WEBRTC_IDENTITY_SERVICE_HOST_H_

#include <string>

#include "base/basictypes.h"
#include "content/public/browser/browser_message_filter.h"

class GURL;

namespace content {

class WebRTCIdentityStore;

// This class is the host for WebRTCIdentityService in the browser process.
// It converts the IPC messages for requesting a WebRTC DTLS identity and
// cancelling a pending request into calls of WebRTCIdentityStore. It also sends
// the request result back to the renderer through IPC.
// Only one outstanding request is allowed per renderer at a time. If a second
// request is made before the first one completes, an IPC with error
// ERR_INSUFFICIENT_RESOURCES will be sent back to the renderer.
class WebRTCIdentityServiceHost : public BrowserMessageFilter {
 public:
  explicit WebRTCIdentityServiceHost(WebRTCIdentityStore* identity_store);

 private:
  virtual ~WebRTCIdentityServiceHost();

  // content::BrowserMessageFilter override.
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;

  void OnComplete(int request_id,
                  int error,
                  const std::string& certificate,
                  const std::string& private_key);

  // Requests a DTLS identity from the DTLS identity store for the given
  // |origin| and |identity_name|. If no such identity exists, a new one will be
  // generated using the given |common_name|.
  // |request_id| is a unique id chosen by the client and used to cancel a
  // pending request.
  void OnRequestIdentity(int request_id,
                         const GURL& origin,
                         const std::string& identity_name,
                         const std::string& common_name);

  // Cancels a pending request by its id. If there is no pending request having
  // the same id, the call is ignored.
  void OnCancelRequest(int request_id);

  void SendErrorMessage(int request_id, int error);

  base::Closure cancel_callback_;
  WebRTCIdentityStore* identity_store_;

  DISALLOW_COPY_AND_ASSIGN(WebRTCIdentityServiceHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_WEBRTC_IDENTITY_SERVICE_HOST_H_
