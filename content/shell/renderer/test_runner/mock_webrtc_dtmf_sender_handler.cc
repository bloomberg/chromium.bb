// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/test_runner/mock_webrtc_dtmf_sender_handler.h"

#include "base/logging.h"
#include "content/shell/renderer/test_runner/web_test_delegate.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebRTCDTMFSenderHandlerClient.h"

using namespace blink;

namespace content {

class DTMFSenderToneTask : public WebMethodTask<MockWebRTCDTMFSenderHandler> {
 public:
  DTMFSenderToneTask(MockWebRTCDTMFSenderHandler* object,
                     WebRTCDTMFSenderHandlerClient* client)
      : WebMethodTask<MockWebRTCDTMFSenderHandler>(object), client_(client) {}

  virtual void RunIfValid() OVERRIDE {
    WebString tones = object_->currentToneBuffer();
    object_->ClearToneBuffer();
    client_->didPlayTone(tones);
  }

 private:
  WebRTCDTMFSenderHandlerClient* client_;
};

/////////////////////

MockWebRTCDTMFSenderHandler::MockWebRTCDTMFSenderHandler(
    const WebMediaStreamTrack& track,
    WebTestDelegate* delegate)
    : client_(0), track_(track), delegate_(delegate) {
}

void MockWebRTCDTMFSenderHandler::setClient(
    WebRTCDTMFSenderHandlerClient* client) {
  client_ = client;
}

WebString MockWebRTCDTMFSenderHandler::currentToneBuffer() {
  return tone_buffer_;
}

bool MockWebRTCDTMFSenderHandler::canInsertDTMF() {
  DCHECK(client_ && !track_.isNull());
  return track_.source().type() == WebMediaStreamSource::TypeAudio &&
         track_.isEnabled() &&
         track_.source().readyState() == WebMediaStreamSource::ReadyStateLive;
}

bool MockWebRTCDTMFSenderHandler::insertDTMF(const WebString& tones,
                                             long duration,
                                             long inter_tone_gap) {
  DCHECK(client_);
  if (!canInsertDTMF())
    return false;

  tone_buffer_ = tones;
  delegate_->PostTask(new DTMFSenderToneTask(this, client_));
  delegate_->PostTask(new DTMFSenderToneTask(this, client_));
  return true;
}

}  // namespace content
