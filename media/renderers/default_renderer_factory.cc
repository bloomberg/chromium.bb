// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/default_renderer_factory.h"

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#if !defined(MEDIA_DISABLE_FFMPEG)
#include "media/filters/ffmpeg_audio_decoder.h"
#include "media/filters/ffmpeg_video_decoder.h"
#endif
#include "media/filters/gpu_video_decoder.h"
#include "media/filters/opus_audio_decoder.h"
#include "media/renderers/audio_renderer_impl.h"
#include "media/renderers/gpu_video_accelerator_factories.h"
#include "media/renderers/renderer_impl.h"
#include "media/renderers/video_renderer_impl.h"
#if !defined(MEDIA_DISABLE_LIBVPX)
#include "media/filters/vpx_video_decoder.h"
#endif

namespace media {

#if !defined(MEDIA_DISABLE_FFMPEG)
// TODO(xhwang): We are abusing CreateMediaSourceErrorEvent() in a lot of places
// that are not MediaSource related. Fix this in a separate CL.
static void LogError(const scoped_refptr<MediaLog>& media_log,
                     const std::string& error) {
  media_log->AddEvent(media_log->CreateMediaSourceErrorEvent(error));
}
#endif

DefaultRendererFactory::DefaultRendererFactory(
    const scoped_refptr<MediaLog>& media_log,
    const scoped_refptr<GpuVideoAcceleratorFactories>& gpu_factories,
    const AudioHardwareConfig& audio_hardware_config)
    : media_log_(media_log),
      gpu_factories_(gpu_factories),
      audio_hardware_config_(audio_hardware_config) {
}

DefaultRendererFactory::~DefaultRendererFactory() {
}

// TODO(xhwang): Use RendererConfig to customize what decoders we use.
scoped_ptr<Renderer> DefaultRendererFactory::CreateRenderer(
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    AudioRendererSink* audio_renderer_sink) {
  DCHECK(audio_renderer_sink);

  // Create our audio decoders and renderer.
  ScopedVector<AudioDecoder> audio_decoders;

#if !defined(MEDIA_DISABLE_FFMPEG)
  audio_decoders.push_back(new FFmpegAudioDecoder(
      media_task_runner, base::Bind(&LogError, media_log_)));
#endif

  audio_decoders.push_back(new OpusAudioDecoder(media_task_runner));

  scoped_ptr<AudioRenderer> audio_renderer(new AudioRendererImpl(
      media_task_runner, audio_renderer_sink, audio_decoders.Pass(),
      audio_hardware_config_, media_log_));

  // Create our video decoders and renderer.
  ScopedVector<VideoDecoder> video_decoders;

  // |gpu_factories_| requires that its entry points be called on its
  // |GetTaskRunner()|.  Since |pipeline_| will own decoders created from the
  // factories, require that their message loops are identical.
  DCHECK(!gpu_factories_.get() ||
         (gpu_factories_->GetTaskRunner() == media_task_runner.get()));

  if (gpu_factories_.get())
    video_decoders.push_back(new GpuVideoDecoder(gpu_factories_));

#if !defined(MEDIA_DISABLE_LIBVPX)
  video_decoders.push_back(new VpxVideoDecoder(media_task_runner));
#endif

#if !defined(MEDIA_DISABLE_FFMPEG)
  video_decoders.push_back(new FFmpegVideoDecoder(media_task_runner));
#endif

  scoped_ptr<VideoRenderer> video_renderer(new VideoRendererImpl(
      media_task_runner, video_decoders.Pass(), true, media_log_));

  // Create renderer.
  return scoped_ptr<Renderer>(new RendererImpl(
      media_task_runner, audio_renderer.Pass(), video_renderer.Pass()));
}

}  // namespace media
