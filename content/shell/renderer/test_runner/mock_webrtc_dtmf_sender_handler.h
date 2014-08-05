// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCKWEBRTCDTMFSENDERHANDLER_H_
#define CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCKWEBRTCDTMFSENDERHANDLER_H_

#include "base/basictypes.h"
#include "content/shell/renderer/test_runner/WebTask.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/platform/WebRTCDTMFSenderHandler.h"
#include "third_party/WebKit/public/platform/WebString.h"

namespace content {

class WebTestDelegate;

class MockWebRTCDTMFSenderHandler : public blink::WebRTCDTMFSenderHandler {
 public:
  MockWebRTCDTMFSenderHandler(const blink::WebMediaStreamTrack& track,
                              WebTestDelegate* delegate);

  // WebRTCDTMFSenderHandler related methods
  virtual void setClient(blink::WebRTCDTMFSenderHandlerClient* client) OVERRIDE;
  virtual blink::WebString currentToneBuffer() OVERRIDE;
  virtual bool canInsertDTMF() OVERRIDE;
  virtual bool insertDTMF(const blink::WebString& tones,
                          long duration,
                          long inter_tone_gap) OVERRIDE;

  // WebTask related methods
  WebTaskList* mutable_task_list() { return &task_list_; }

  void ClearToneBuffer() { tone_buffer_.reset(); }

 private:
  MockWebRTCDTMFSenderHandler();

  blink::WebRTCDTMFSenderHandlerClient* client_;
  blink::WebMediaStreamTrack track_;
  blink::WebString tone_buffer_;
  WebTaskList task_list_;
  WebTestDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(MockWebRTCDTMFSenderHandler);
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCKWEBRTCDTMFSENDERHANDLER_H_
