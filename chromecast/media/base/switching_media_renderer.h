// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_BASE_SWITCHING_MEDIA_RENDERER_H_
#define CHROMECAST_MEDIA_BASE_SWITCHING_MEDIA_RENDERER_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/renderer.h"

namespace media {
class DemuxerStreamProvider;
}

namespace chromecast {
namespace media {

// Chromecast's custom media renderer which is capable of selecting an
// appropriate media renderer at runtime depending on the content.
// We'll look at media types of media streams present in DemuxerStreamProvider
// and will use either CMA-based renderer (for media types that are supported by
// our hardware decoder - H264 and VP8 video, AAC and Vorbis audio) or the
// default Chrome media renderer (this will allow us to support audio codecs
// like FLAC and Opus, which are decoded in software).
class SwitchingMediaRenderer : public ::media::Renderer {
 public:
  SwitchingMediaRenderer(
      scoped_ptr<::media::Renderer> default_renderer,
      scoped_ptr<::media::Renderer> cma_renderer);
  ~SwitchingMediaRenderer() override;

  // ::media::Renderer implementation:
  void Initialize(
      ::media::DemuxerStreamProvider* demuxer_stream_provider,
      const base::Closure& init_cb,
      const ::media::StatisticsCB& statistics_cb,
      const ::media::BufferingStateCB& buffering_state_cb,
      const ::media::Renderer::PaintCB& paint_cb,
      const base::Closure& ended_cb,
      const ::media::PipelineStatusCB& error_cb,
      const base::Closure& waiting_for_decryption_key_cb) override;
  void SetCdm(::media::CdmContext* cdm_context,
              const ::media::CdmAttachedCB& cdm_attached_cb) override;
  void Flush(const base::Closure& flush_cb) override;
  void StartPlayingFrom(base::TimeDelta time) override;
  void SetPlaybackRate(float playback_rate) override;
  void SetVolume(float volume) override;
  base::TimeDelta GetMediaTime() override;
  bool HasAudio() override;
  bool HasVideo() override;

 private:
  // Returns the pointer to the actual renderer being used
  ::media::Renderer* GetRenderer() const;

  ::media::DemuxerStreamProvider* demuxer_stream_provider_;
  scoped_ptr<::media::Renderer> default_renderer_;
  scoped_ptr<::media::Renderer> cma_renderer_;

  DISALLOW_COPY_AND_ASSIGN(SwitchingMediaRenderer);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_BASE_SWITCHING_MEDIA_RENDERER_H_
