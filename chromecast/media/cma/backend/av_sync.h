// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_AV_SYNC_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_AV_SYNC_H_

#include <stdint.h>

#include "base/memory/scoped_refptr.h"
#include "chromecast/public/media/media_pipeline_backend.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace chromecast {
namespace media {

class MediaPipelineBackendForMixer;

// Interface to an AV sync module. This AV sync treats the audio as master and
// syncs the video to it, while attempting to minimize jitter in the video. It
// is typically owned by the audio decoder, but it may be owned by any
// component willing to notify it about the state of the audio playback as
// below.
//
// Whatever the owner of this component is, it should include and depend on
// this interface rather the implementation header file. It should be possible
// for someone in the future to provide their own implementation of this class
// by linking in their AvSync::Create method statically defined below.
class AvSync {
 public:
  static std::unique_ptr<AvSync> Create(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      MediaPipelineBackendForMixer* backend);

  virtual ~AvSync() = default;

  // Notify that an audio buffer has been pushed to the mixer, and what was the
  // rendering delay corresponding to this audio buffer. The AV sync code may
  // choose to use this information however it pleases, but typically it would
  // use it to understand what is the audio PTS at any moment, and use this
  // information to sync the video accordingly.
  virtual void NotifyAudioBufferPushed(
      int64_t buffer_timestamp,
      MediaPipelineBackend::AudioDecoder::RenderingDelay delay) = 0;

  // Notify that the audio playback has been started. The AV sync will typically
  // start upkeeping AV sync. The AV sync code is *not* responsible for
  // starting the video.
  // TODO(almasrymina): consider actually changing AV sync's responsibilities
  // to pause/resume/stop/start playback.
  virtual void NotifyStart() = 0;

  // Notify that the audio playback has been stopped. The AV sync will typically
  // stop upkeeping AV sync. The AV sync code is *not* responsible for stopping
  // the video.
  virtual void NotifyStop() = 0;

  // Notify that the audio playback has been paused. The AV sync code will
  // typically stop upkeeping AV sync until the audio playback is resumed again.
  // The AV sync code is *not* responsible for pausing the video.
  virtual void NotifyPause() = 0;

  // Notify that the audio playback has been resumed. The AV sync code will
  // typically start upkeeping AV sync again after this is called. The AV sync
  // code is *not* responsible for resuming the video.
  virtual void NotifyResume() = 0;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_AV_SYNC_H_
