// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/base/switching_media_renderer.h"

#include "base/logging.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/demuxer_stream.h"
#include "media/base/demuxer_stream_provider.h"

namespace chromecast {
namespace media {

SwitchingMediaRenderer::SwitchingMediaRenderer(
    scoped_ptr<::media::Renderer> default_renderer,
    scoped_ptr<::media::Renderer> cma_renderer)
    : default_renderer_(default_renderer.Pass()),
    cma_renderer_(cma_renderer.Pass()) {
      DCHECK(default_renderer_);
      DCHECK(cma_renderer_);
}

SwitchingMediaRenderer::~SwitchingMediaRenderer() {
}

void SwitchingMediaRenderer::Initialize(
    ::media::DemuxerStreamProvider* demuxer_stream_provider,
    const base::Closure& init_cb,
    const ::media::StatisticsCB& statistics_cb,
    const ::media::BufferingStateCB& buffering_state_cb,
    const ::media::Renderer::PaintCB& paint_cb,
    const base::Closure& ended_cb,
    const ::media::PipelineStatusCB& error_cb) {
  // At this point the DemuxerStreamProvider should be fully initialized, so we
  // have enough information to decide which renderer to use.
  demuxer_stream_provider_ = demuxer_stream_provider;
  DCHECK(demuxer_stream_provider_);
  ::media::DemuxerStream* audio_stream =
      demuxer_stream_provider_->GetStream(::media::DemuxerStream::AUDIO);
  ::media::DemuxerStream* video_stream =
      demuxer_stream_provider_->GetStream(::media::DemuxerStream::VIDEO);
  if (audio_stream && !video_stream &&
      audio_stream->audio_decoder_config().codec() != ::media::kCodecAAC &&
      audio_stream->audio_decoder_config().codec() != ::media::kCodecVorbis) {
    // We'll use the default Chrome media renderer with software audio decoding
    cma_renderer_.reset();
  } else {
    // We'll use the CMA-based rendering with hardware decoding
    default_renderer_.reset();
  }

  return GetRenderer()->Initialize(demuxer_stream_provider,
                                   init_cb, statistics_cb, buffering_state_cb,
                                   paint_cb, ended_cb, error_cb);
}

::media::Renderer* SwitchingMediaRenderer::GetRenderer() const {
  DCHECK(default_renderer_ || cma_renderer_);
  if (cma_renderer_)
    return cma_renderer_.get();

  DCHECK(default_renderer_);
  return default_renderer_.get();
}

void SwitchingMediaRenderer::SetCdm(
    ::media::CdmContext* cdm_context,
    const ::media::CdmAttachedCB& cdm_attached_cb) {
  GetRenderer()->SetCdm(cdm_context, cdm_attached_cb);
}

void SwitchingMediaRenderer::Flush(const base::Closure& flush_cb) {
  GetRenderer()->Flush(flush_cb);
}

void SwitchingMediaRenderer::StartPlayingFrom(base::TimeDelta time) {
  GetRenderer()->StartPlayingFrom(time);
}

void SwitchingMediaRenderer::SetPlaybackRate(float playback_rate)  {
  GetRenderer()->SetPlaybackRate(playback_rate);
}

void SwitchingMediaRenderer::SetVolume(float volume)  {
  GetRenderer()->SetVolume(volume);
}

base::TimeDelta SwitchingMediaRenderer::GetMediaTime()  {
  return GetRenderer()->GetMediaTime();
}

bool SwitchingMediaRenderer::HasAudio() {
  return GetRenderer()->HasAudio();
}

bool SwitchingMediaRenderer::HasVideo() {
  return GetRenderer()->HasVideo();
}

}  // namespace media
}  // namespace chromecast
