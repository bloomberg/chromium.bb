// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_TEST_RUNNER_MOCK_WEB_MEDIA_STREAM_CENTER_H_
#define CONTENT_SHELL_TEST_RUNNER_MOCK_WEB_MEDIA_STREAM_CENTER_H_

#include "base/macros.h"
#include "third_party/WebKit/public/platform/WebMediaStreamCenter.h"

namespace blink {
class WebAudioSourceProvider;
};

namespace test_runner {

class MockWebMediaStreamCenter : public blink::WebMediaStreamCenter {
 public:
  MockWebMediaStreamCenter() = default;
  ~MockWebMediaStreamCenter() override {}

  void DidEnableMediaStreamTrack(
      const blink::WebMediaStreamTrack& track) override;
  void DidDisableMediaStreamTrack(
      const blink::WebMediaStreamTrack& track) override;
  bool DidAddMediaStreamTrack(const blink::WebMediaStream& stream,
                              const blink::WebMediaStreamTrack& track) override;
  bool DidRemoveMediaStreamTrack(
      const blink::WebMediaStream& stream,
      const blink::WebMediaStreamTrack& track) override;
  void DidStopLocalMediaStream(const blink::WebMediaStream& stream) override;
  bool DidStopMediaStreamTrack(
      const blink::WebMediaStreamTrack& track) override;
  void DidCreateMediaStream(blink::WebMediaStream& stream) override;
  blink::WebAudioSourceProvider* CreateWebAudioSourceFromMediaStreamTrack(
      const blink::WebMediaStreamTrack& track) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockWebMediaStreamCenter);
};

}  // namespace test_runner

#endif  // CONTENT_SHELL_TEST_RUNNER_MOCK_WEB_MEDIA_STREAM_CENTER_H_
