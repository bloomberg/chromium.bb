// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_VIDEO_FRAME_PROVIDER_H_
#define CC_TEST_FAKE_VIDEO_FRAME_PROVIDER_H_

#include "cc/layers/video_frame_provider.h"
#include "media/base/video_frame.h"

namespace cc {

// Fake video frame provider that always provides the same VideoFrame.
class FakeVideoFrameProvider : public VideoFrameProvider {
 public:
  FakeVideoFrameProvider();
  virtual ~FakeVideoFrameProvider();

  virtual void SetVideoFrameProviderClient(Client* client) OVERRIDE;
  virtual scoped_refptr<media::VideoFrame> GetCurrentFrame() OVERRIDE;
  virtual void PutCurrentFrame(const scoped_refptr<media::VideoFrame>&)
      OVERRIDE {}

  Client* client() { return client_; }

  void set_frame(const scoped_refptr<media::VideoFrame>& frame) {
    frame_ = frame;
  }

 private:
  scoped_refptr<media::VideoFrame> frame_;
  Client* client_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_VIDEO_FRAME_PROVIDER_H_
