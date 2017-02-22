// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_TEST_RUNNER_MOCK_WEBRTC_DTMF_SENDER_HANDLER_H_
#define CONTENT_SHELL_TEST_RUNNER_MOCK_WEBRTC_DTMF_SENDER_HANDLER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/platform/WebRTCDTMFSenderHandler.h"
#include "third_party/WebKit/public/platform/WebString.h"

namespace test_runner {

class WebTestDelegate;

class MockWebRTCDTMFSenderHandler : public blink::WebRTCDTMFSenderHandler {
 public:
  MockWebRTCDTMFSenderHandler(const blink::WebMediaStreamTrack& track,
                              WebTestDelegate* delegate);
  ~MockWebRTCDTMFSenderHandler() override;

  // WebRTCDTMFSenderHandler related methods
  void setClient(blink::WebRTCDTMFSenderHandlerClient* client) override;
  blink::WebString currentToneBuffer() override;
  bool canInsertDTMF() override;
  bool insertDTMF(const blink::WebString& tones,
                  long duration,
                  long inter_tone_gap) override;

  void ClearToneBuffer() { tone_buffer_.reset(); }

 private:
  void PlayTone();

  blink::WebRTCDTMFSenderHandlerClient* client_;
  blink::WebMediaStreamTrack track_;
  blink::WebString tone_buffer_;
  WebTestDelegate* delegate_;

  base::WeakPtrFactory<MockWebRTCDTMFSenderHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MockWebRTCDTMFSenderHandler);
};

}  // namespace test_runner

#endif  // CONTENT_SHELL_TEST_RUNNER_MOCK_WEBRTC_DTMF_SENDER_HANDLER_H_
