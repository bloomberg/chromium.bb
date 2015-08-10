// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_PEER_CONNECTION_IDENTITY_SERVICE_H_
#define CONTENT_RENDERER_MEDIA_PEER_CONNECTION_IDENTITY_SERVICE_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "content/public/renderer/render_process_observer.h"
#include "third_party/libjingle/source/talk/app/webrtc/peerconnectioninterface.h"
#include "url/gurl.h"

namespace content {

// This class is associated with a peer connection and handles WebRTC DTLS
// identity requests by delegating to the per-renderer WebRTCIdentityProxy.
// TODO(hbos): Delete this after rolling webrtc CL 1176383004.
class PeerConnectionIdentityService
    : public webrtc::DTLSIdentityServiceInterface {
 public:
  PeerConnectionIdentityService(const GURL& url,
                                const GURL& first_party_for_cookies);

  ~PeerConnectionIdentityService() override;

  // webrtc::DTLSIdentityServiceInterface implementation.
  bool RequestIdentity(const std::string& identity_name,
                       const std::string& common_name,
                       webrtc::DTLSIdentityRequestObserver* observer) override;

 private:
  base::ThreadChecker signaling_thread_;

  const scoped_refptr<base::SingleThreadTaskRunner> main_thread_;

  const GURL url_;

  const GURL first_party_for_cookies_;

  DISALLOW_COPY_AND_ASSIGN(PeerConnectionIdentityService);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_PEER_CONNECTION_IDENTITY_SERVICE_H_
