// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MOCK_WEB_JSEP_PEER_CONNECTION_HANDLER_CLIENT_H_
#define CONTENT_RENDERER_MEDIA_MOCK_WEB_JSEP_PEER_CONNECTION_HANDLER_CLIENT_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebPeerConnection00HandlerClient.h"

namespace WebKit {

class MockWebPeerConnection00HandlerClient
    : public WebPeerConnection00HandlerClient {
 public:
      MockWebPeerConnection00HandlerClient();
  virtual ~MockWebPeerConnection00HandlerClient();

  // WebPeerConnection00HandlerClient implementation.
  virtual void didGenerateICECandidate(
      const WebICECandidateDescriptor& candidate,
      bool more_to_follow) OVERRIDE;
  virtual void didChangeReadyState(ReadyState state) OVERRIDE;
  virtual void didChangeICEState(ICEState state) OVERRIDE;
  virtual void didAddRemoteStream(
      const WebMediaStreamDescriptor& stream_descriptor) OVERRIDE;
  virtual void didRemoveRemoteStream(
      const WebMediaStreamDescriptor& stream_descriptor) OVERRIDE;

  const std::string& stream_label() const { return stream_label_; }
  ReadyState ready_state() const { return ready_state_; }
  const std::string& candidate_label() const { return candidate_label_; }
  const std::string& candidate_sdp() const { return candidate_sdp_; }
  bool more_to_follow() const { return more_to_follow_; }

 private:
  std::string stream_label_;
  ReadyState ready_state_;
  std::string candidate_label_;
  std::string candidate_sdp_;
  bool more_to_follow_;

  DISALLOW_COPY_AND_ASSIGN(MockWebPeerConnection00HandlerClient);
};

}  // namespace WebKit

#endif  // CONTENT_RENDERER_MEDIA_MOCK_WEB_JSEP_PEER_CONNECTION_HANDLER_CLIENT_H_
