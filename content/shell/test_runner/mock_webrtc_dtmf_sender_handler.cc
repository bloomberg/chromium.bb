// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/test_runner/mock_webrtc_dtmf_sender_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "content/shell/test_runner/web_test_delegate.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebRTCDTMFSenderHandlerClient.h"

using namespace blink;

namespace test_runner {

MockWebRTCDTMFSenderHandler::MockWebRTCDTMFSenderHandler(
    const WebMediaStreamTrack& track,
    WebTestDelegate* delegate)
    : client_(0), track_(track), delegate_(delegate), weak_factory_(this) {}

MockWebRTCDTMFSenderHandler::~MockWebRTCDTMFSenderHandler() {}

void MockWebRTCDTMFSenderHandler::setClient(
    WebRTCDTMFSenderHandlerClient* client) {
  client_ = client;
}

WebString MockWebRTCDTMFSenderHandler::currentToneBuffer() {
  return tone_buffer_;
}

bool MockWebRTCDTMFSenderHandler::canInsertDTMF() {
  DCHECK(client_ && !track_.isNull());
  return track_.source().getType() == WebMediaStreamSource::TypeAudio &&
         track_.isEnabled() &&
         track_.source().getReadyState() ==
             WebMediaStreamSource::ReadyStateLive;
}

bool MockWebRTCDTMFSenderHandler::insertDTMF(const WebString& tones,
                                             long duration,
                                             long inter_tone_gap) {
  DCHECK(client_);
  if (!canInsertDTMF())
    return false;

  tone_buffer_ = tones;
  base::Closure closure = base::Bind(&MockWebRTCDTMFSenderHandler::PlayTone,
                                     weak_factory_.GetWeakPtr());
  delegate_->PostTask(closure);
  delegate_->PostTask(closure);
  return true;
}

void MockWebRTCDTMFSenderHandler::PlayTone() {
  WebString tones = currentToneBuffer();
  ClearToneBuffer();
  client_->didPlayTone(tones);
}

}  // namespace test_runner
