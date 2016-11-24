// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TEST_RUNNER_MOCK_WEB_MEDIA_STREAM_CENTER_H_
#define COMPONENTS_TEST_RUNNER_MOCK_WEB_MEDIA_STREAM_CENTER_H_

#include "base/macros.h"
#include "third_party/WebKit/public/platform/WebMediaStreamCenter.h"

namespace blink {
class WebAudioSourceProvider;
};

namespace test_runner {

class MockWebMediaStreamCenter : public blink::WebMediaStreamCenter {
 public:
  MockWebMediaStreamCenter() = default;
  ~MockWebMediaStreamCenter() override {};

  void didEnableMediaStreamTrack(
      const blink::WebMediaStreamTrack& track) override;
  void didDisableMediaStreamTrack(
      const blink::WebMediaStreamTrack& track) override;
  bool didAddMediaStreamTrack(const blink::WebMediaStream& stream,
                              const blink::WebMediaStreamTrack& track) override;
  bool didRemoveMediaStreamTrack(
      const blink::WebMediaStream& stream,
      const blink::WebMediaStreamTrack& track) override;
  void didStopLocalMediaStream(const blink::WebMediaStream& stream) override;
  bool didStopMediaStreamTrack(
      const blink::WebMediaStreamTrack& track) override;
  void didCreateMediaStream(blink::WebMediaStream& stream) override;
  blink::WebAudioSourceProvider* createWebAudioSourceFromMediaStreamTrack(
      const blink::WebMediaStreamTrack& track) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockWebMediaStreamCenter);
};

}  // namespace test_runner

#endif  // COMPONENTS_TEST_RUNNER_MOCK_WEB_MEDIA_STREAM_CENTER_H_
