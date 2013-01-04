// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_VIDEO_FRAME_H_
#define CC_TEST_FAKE_VIDEO_FRAME_H_

#include "media/base/video_frame.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebVideoFrame.h"

namespace cc {

class FakeVideoFrame : public WebKit::WebVideoFrame {
 public:
  explicit FakeVideoFrame(const scoped_refptr<media::VideoFrame>& frame);
  virtual ~FakeVideoFrame();

  // WebKit::WebVideoFrame implementation.
  virtual Format format() const;
  virtual unsigned width() const;
  virtual unsigned height() const;
  virtual unsigned planes() const;
  virtual int stride(unsigned plane) const;
  virtual const void* data(unsigned plane) const;
  virtual unsigned textureId() const;
  virtual unsigned textureTarget() const;
  virtual WebKit::WebRect visibleRect() const;
  virtual WebKit::WebSize textureSize() const;

  static media::VideoFrame* ToVideoFrame(
      WebKit::WebVideoFrame* web_video_frame);

 private:
  scoped_refptr<media::VideoFrame> frame_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_VIDEO_FRAME_H_
