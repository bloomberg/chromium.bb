// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MOCK_WEB_PEER_CONNECTION_HANDLER_CLIENT_H_
#define CONTENT_RENDERER_MEDIA_MOCK_WEB_PEER_CONNECTION_HANDLER_CLIENT_H_

#include <string>

#include "base/basictypes.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPeerConnectionHandlerClient.h"

namespace WebKit {

class MockWebPeerConnectionHandlerClient
    : public WebPeerConnectionHandlerClient {
 public:
  MockWebPeerConnectionHandlerClient();
  virtual ~MockWebPeerConnectionHandlerClient();

  // WebPeerConnectionHandlerClient implementation.
  virtual void didCompleteICEProcessing();
  virtual void didGenerateSDP(const WebString& sdp);
  virtual void didReceiveDataStreamMessage(const char* data, size_t length);
  virtual void didAddRemoteStream(
      const WebMediaStreamDescriptor& stream_descriptor);
  virtual void didRemoveRemoteStream(
      const WebMediaStreamDescriptor& stream_descriptor);

  const std::string& stream_label() { return stream_label_; }
  const std::string& sdp() const { return sdp_; }

 private:
  std::string stream_label_;
  std::string sdp_;

  DISALLOW_COPY_AND_ASSIGN(MockWebPeerConnectionHandlerClient);
};

}  // namespace WebKit

#endif  // CONTENT_RENDERER_MEDIA_MOCK_WEB_PEER_CONNECTION_HANDLER_CLIENT_H_
