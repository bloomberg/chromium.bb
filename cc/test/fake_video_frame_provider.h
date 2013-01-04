// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_VIDEO_FRAME_PROVIDER_H_
#define CC_TEST_FAKE_VIDEO_FRAME_PROVIDER_H_

#include "base/memory/scoped_ptr.h"
#include "cc/test/fake_video_frame.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebVideoFrameProvider.h"

namespace cc {

// Fake video frame provider that always provides the same VideoFrame.
class FakeVideoFrameProvider: public WebKit::WebVideoFrameProvider {
 public:
  FakeVideoFrameProvider();
  virtual ~FakeVideoFrameProvider();

  virtual void setVideoFrameProviderClient(Client* client) OVERRIDE;
  virtual WebKit::WebVideoFrame* getCurrentFrame() OVERRIDE;
  virtual void putCurrentFrame(WebKit::WebVideoFrame*) OVERRIDE {}

  void set_frame(FakeVideoFrame* frame) {
    frame_ = frame;
  }

 private:
  FakeVideoFrame* frame_;
  Client* client_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_VIDEO_FRAME_PROVIDER_H_
