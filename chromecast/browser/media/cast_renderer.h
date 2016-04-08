// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/media/media_pipeline_backend_factory.h"
#include "media/base/renderer.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace media {
class MediaLog;
}  // namespace media

namespace chromecast {
class TaskRunnerImpl;

namespace media {
class BalancedMediaTaskRunnerFactory;
class MediaPipelineImpl;

class CastRenderer : public ::media::Renderer {
 public:
  CastRenderer(const CreateMediaPipelineBackendCB& create_backend_cb,
               const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);
  ~CastRenderer() final;

  // ::media::Renderer implementation.
  void Initialize(::media::DemuxerStreamProvider* demuxer_stream_provider,
                  const ::media::PipelineStatusCB& init_cb,
                  const ::media::StatisticsCB& statistics_cb,
                  const ::media::BufferingStateCB& buffering_state_cb,
                  const base::Closure& ended_cb,
                  const ::media::PipelineStatusCB& error_cb,
                  const base::Closure& waiting_for_decryption_key_cb) final;
  void SetCdm(::media::CdmContext* cdm_context,
              const ::media::CdmAttachedCB& cdm_attached_cb) final;
  void Flush(const base::Closure& flush_cb) final;
  void StartPlayingFrom(base::TimeDelta time) final;
  void SetPlaybackRate(double playback_rate) final;
  void SetVolume(float volume) final;
  base::TimeDelta GetMediaTime() final;
  bool HasAudio() final;
  bool HasVideo() final;

 private:
  enum Stream { STREAM_AUDIO, STREAM_VIDEO };
  void OnEos(Stream stream);

  const CreateMediaPipelineBackendCB create_backend_cb_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_refptr<BalancedMediaTaskRunnerFactory> media_task_runner_factory_;
  std::unique_ptr<TaskRunnerImpl> backend_task_runner_;
  std::unique_ptr<MediaPipelineImpl> pipeline_;
  bool eos_[2];
  base::Closure ended_cb_;
  DISALLOW_COPY_AND_ASSIGN(CastRenderer);
};

}  // namespace media
}  // namespace chromecast
