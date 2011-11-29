// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "content/renderer/media/mock_web_peer_connection_handler_client.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebMediaStreamDescriptor.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"

namespace WebKit {

MockWebPeerConnectionHandlerClient::MockWebPeerConnectionHandlerClient() {}

MockWebPeerConnectionHandlerClient::~MockWebPeerConnectionHandlerClient() {}

void MockWebPeerConnectionHandlerClient::didCompleteICEProcessing() {}

void MockWebPeerConnectionHandlerClient::didGenerateSDP(const WebString& sdp) {
  sdp_ = UTF16ToUTF8(sdp);
}

void MockWebPeerConnectionHandlerClient::didReceiveDataStreamMessage(
    const char* data, size_t length) {
}

void MockWebPeerConnectionHandlerClient::didAddRemoteStream(
    const WebMediaStreamDescriptor& stream_descriptor) {
  stream_label_ = UTF16ToUTF8(stream_descriptor.label());
}

void MockWebPeerConnectionHandlerClient::didRemoveRemoteStream(
    const WebMediaStreamDescriptor& stream_descriptor) {
  DCHECK(stream_label_ == UTF16ToUTF8(stream_descriptor.label()));
  stream_label_.clear();
}

}  // namespace WebKit
