// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of a fake VideoCaptureDevice class. Used for testing other
// video capture classes when no real hardware is available.

#ifndef MEDIA_VIDEO_CAPTURE_FAKE_VIDEO_CAPTURE_DEVICE_H_
#define MEDIA_VIDEO_CAPTURE_FAKE_VIDEO_CAPTURE_DEVICE_H_

#include <string>

#include "base/atomicops.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "media/video/capture/video_capture_device.h"

namespace media {

class MEDIA_EXPORT FakeVideoCaptureDevice : public VideoCaptureDevice {
 public:
  enum FakeVideoCaptureDeviceType {
    USING_OWN_BUFFERS,
    USING_OWN_BUFFERS_TRIPLANAR,
    USING_CLIENT_BUFFERS,
  };

  static int FakeCapturePeriodMs() { return kFakeCapturePeriodMs; }

  explicit FakeVideoCaptureDevice(FakeVideoCaptureDeviceType device_type);
  ~FakeVideoCaptureDevice() override;

  // VideoCaptureDevice implementation.
  void AllocateAndStart(const VideoCaptureParams& params,
                        scoped_ptr<Client> client) override;
  void StopAndDeAllocate() override;

 private:
  static const int kFakeCapturePeriodMs = 50;

  void CaptureUsingOwnBuffers(base::TimeTicks expected_execution_time);
  void CaptureUsingClientBuffers(VideoPixelFormat pixel_format,
                                 VideoPixelStorage pixel_storage,
                                 base::TimeTicks expected_execution_time);
  void BeepAndScheduleNextCapture(
      base::TimeTicks expected_execution_time,
      const base::Callback<void(base::TimeTicks)>& next_capture);

  // |thread_checker_| is used to check that all methods are called in the
  // correct thread that owns the object.
  base::ThreadChecker thread_checker_;

  const FakeVideoCaptureDeviceType device_type_;

  scoped_ptr<VideoCaptureDevice::Client> client_;
  // |fake_frame_| is used for capturing on Own Buffers.
  scoped_ptr<uint8[]> fake_frame_;
  int frame_count_;
  VideoCaptureFormat capture_format_;

  // FakeVideoCaptureDevice post tasks to itself for frame construction and
  // needs to deal with asynchronous StopAndDeallocate().
  base::WeakPtrFactory<FakeVideoCaptureDevice> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeVideoCaptureDevice);
};

}  // namespace media

#endif  // MEDIA_VIDEO_CAPTURE_FAKE_VIDEO_CAPTURE_DEVICE_H_
