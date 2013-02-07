// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_CAPTURE_SCREEN_SCREEN_CAPTURER_FAKE_H_
#define MEDIA_VIDEO_CAPTURE_SCREEN_SCREEN_CAPTURER_FAKE_H_

#include "base/memory/scoped_ptr.h"
#include "media/base/media_export.h"
#include "media/video/capture/screen/screen_capturer.h"
#include "media/video/capture/screen/screen_capturer_helper.h"

namespace media {

// A ScreenCapturerFake generates artificial image for testing purpose.
//
// ScreenCapturerFake is double-buffered as required by ScreenCapturer.
class MEDIA_EXPORT ScreenCapturerFake : public ScreenCapturer {
 public:
  // ScreenCapturerFake generates a picture of size kWidth x kHeight.
  static const int kWidth = 800;
  static const int kHeight = 600;

  ScreenCapturerFake();
  virtual ~ScreenCapturerFake();

  // Overridden from ScreenCapturer:
  virtual void Start(Delegate* delegate) OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual void InvalidateRegion(const SkRegion& invalid_region) OVERRIDE;
  virtual void CaptureFrame() OVERRIDE;

 private:
  // Generates an image in the front buffer.
  void GenerateImage();

  // Called when the screen configuration is changed.
  void ScreenConfigurationChanged();

  Delegate* delegate_;

  SkISize size_;
  int bytes_per_row_;
  int box_pos_x_;
  int box_pos_y_;
  int box_speed_x_;
  int box_speed_y_;

  ScreenCapturerHelper helper_;

  // We have two buffers for the screen images as required by Capturer.
  static const int kNumBuffers = 2;
  uint8* buffers_[kNumBuffers];

  // The current buffer with valid data for reading.
  int current_buffer_;

  // Used when |delegate_| implements CreateSharedBuffer().
  scoped_refptr<SharedBuffer> shared_buffers_[kNumBuffers];

  // Used when |delegate_| does not implement CreateSharedBuffer().
  scoped_array<uint8> private_buffers_[kNumBuffers];

  DISALLOW_COPY_AND_ASSIGN(ScreenCapturerFake);
};

}  // namespace media

#endif  // MEDIA_VIDEO_CAPTURE_SCREEN_SCREEN_CAPTURER_FAKE_H_
