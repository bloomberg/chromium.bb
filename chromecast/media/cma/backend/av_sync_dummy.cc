// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/av_sync.h"

#include "base/single_thread_task_runner.h"

namespace chromecast {
namespace media {

class AvSyncDummy : public AvSync {
 public:
  AvSyncDummy();

  // AvSync implementation:
  void NotifyAudioBufferPushed(
      int64_t buffer_timestamp,
      MediaPipelineBackend::AudioDecoder::RenderingDelay delay) override;
  void NotifyStart() override;
  void NotifyStop() override;
  void NotifyPause() override;
  void NotifyResume() override;
};

std::unique_ptr<AvSync> AvSync::Create(
    const scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    MediaPipelineBackendForMixer* const backend) {
  return std::make_unique<AvSyncDummy>();
}

AvSyncDummy::AvSyncDummy() {}

void AvSyncDummy::NotifyAudioBufferPushed(
    int64_t buffer_timestamp,
    MediaPipelineBackend::AudioDecoder::RenderingDelay delay) {}

void AvSyncDummy::NotifyStart() {}

void AvSyncDummy::NotifyStop() {}

void AvSyncDummy::NotifyPause() {}

void AvSyncDummy::NotifyResume() {}

}  // namespace media
}  // namespace chromecast
