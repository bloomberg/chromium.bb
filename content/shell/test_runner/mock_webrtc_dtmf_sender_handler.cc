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

void MockWebRTCDTMFSenderHandler::SetClient(
    WebRTCDTMFSenderHandlerClient* client) {
  client_ = client;
}

WebString MockWebRTCDTMFSenderHandler::CurrentToneBuffer() {
  return tone_buffer_;
}

bool MockWebRTCDTMFSenderHandler::CanInsertDTMF() {
  DCHECK(client_ && !track_.IsNull());
  return track_.Source().GetType() == WebMediaStreamSource::kTypeAudio &&
         track_.IsEnabled() &&
         track_.Source().GetReadyState() ==
             WebMediaStreamSource::kReadyStateLive;
}

bool MockWebRTCDTMFSenderHandler::InsertDTMF(const WebString& tones,
                                             long duration,
                                             long inter_tone_gap) {
  DCHECK(client_);
  if (!CanInsertDTMF())
    return false;

  tone_buffer_ = tones;
  base::Closure closure = base::Bind(&MockWebRTCDTMFSenderHandler::PlayTone,
                                     weak_factory_.GetWeakPtr());
  delegate_->PostTask(closure);
  delegate_->PostTask(closure);
  return true;
}

void MockWebRTCDTMFSenderHandler::PlayTone() {
  WebString tones = CurrentToneBuffer();
  ClearToneBuffer();
  client_->DidPlayTone(tones);
}

}  // namespace test_runner
