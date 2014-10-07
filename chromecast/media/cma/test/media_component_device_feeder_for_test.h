// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_TEST_MEDIA_COMPONENT_DEVICE_FEEDER_FOR_TEST_H_
#define CHROMECAST_MEDIA_CMA_TEST_MEDIA_COMPONENT_DEVICE_FEEDER_FOR_TEST_H_

#include <list>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "chromecast/media/cma/backend/audio_pipeline_device.h"
#include "chromecast/media/cma/backend/media_clock_device.h"
#include "chromecast/media/cma/backend/media_pipeline_device.h"
#include "chromecast/media/cma/backend/video_pipeline_device.h"
#include "chromecast/media/cma/base/decoder_buffer_base.h"

namespace chromecast {
namespace media {

typedef std::list<scoped_refptr<DecoderBufferBase> > BufferList;

class MediaComponentDeviceFeederForTest {
 public:
  MediaComponentDeviceFeederForTest(
      MediaComponentDevice *device,
      const BufferList& frames);

  virtual ~MediaComponentDeviceFeederForTest();

  void Initialize(const base::Closure& eos_cb);

  // Feeds one frame into the pipeline.
  void Feed();

 private:
  void OnFramePushed(MediaComponentDevice::FrameStatus status);

  void OnEos();

  MediaComponentDevice *media_component_device_;
  BufferList frames_;

  // Frame index where the audio device is switching to the kStateRunning.
  int rendering_frame_idx_;

  // Frame index where the clock device is switching to the kStateRunning.
  int clock_frame_idx_;

  // Timing pattern to feed the pipeline.
  std::vector<base::TimeDelta> delayed_feed_pattern_;
  size_t delayed_feed_pattern_idx_;

  base::Closure eos_cb_;

  bool feeding_completed_;

  DISALLOW_COPY_AND_ASSIGN(MediaComponentDeviceFeederForTest);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_TEST_MEDIA_COMPONENT_DEVICE_FEEDER_FOR_TEST_H_
