// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/default_renderer_factory.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "build/build_config.h"
#include "media/base/decoder_factory.h"
#include "media/base/media_log.h"
#include "media/filters/gpu_video_decoder.h"
#include "media/renderers/audio_renderer_impl.h"
#include "media/renderers/gpu_video_accelerator_factories.h"
#include "media/renderers/renderer_impl.h"
#include "media/renderers/video_renderer_impl.h"

#if !defined(MEDIA_DISABLE_FFMPEG)
#include "media/filters/ffmpeg_audio_decoder.h"
#if !defined(DISABLE_FFMPEG_VIDEO_DECODERS)
#include "media/filters/ffmpeg_video_decoder.h"
#endif
#endif

#if !defined(MEDIA_DISABLE_LIBVPX)
#include "media/filters/vpx_video_decoder.h"
#endif

namespace media {

DefaultRendererFactory::DefaultRendererFactory(
    MediaLog* media_log,
    DecoderFactory* decoder_factory,
    const GetGpuFactoriesCB& get_gpu_factories_cb)
    : media_log_(media_log),
      decoder_factory_(decoder_factory),
      get_gpu_factories_cb_(get_gpu_factories_cb) {}

DefaultRendererFactory::~DefaultRendererFactory() {
}

std::vector<std::unique_ptr<AudioDecoder>>
DefaultRendererFactory::CreateAudioDecoders(
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner) {
  // Create our audio decoders and renderer.
  std::vector<std::unique_ptr<AudioDecoder>> audio_decoders;

#if !defined(MEDIA_DISABLE_FFMPEG)
  audio_decoders.push_back(
      base::MakeUnique<FFmpegAudioDecoder>(media_task_runner, media_log_));
#endif

  // Use an external decoder only if we cannot otherwise decode in the
  // renderer.
  if (decoder_factory_)
    decoder_factory_->CreateAudioDecoders(media_task_runner, &audio_decoders);

  return audio_decoders;
}

std::vector<std::unique_ptr<VideoDecoder>>
DefaultRendererFactory::CreateVideoDecoders(
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    const RequestOverlayInfoCB& request_overlay_info_cb,
    const gfx::ColorSpace& target_color_space,
    GpuVideoAcceleratorFactories* gpu_factories) {
  // Create our video decoders and renderer.
  std::vector<std::unique_ptr<VideoDecoder>> video_decoders;

  // Prefer an external decoder since one will only exist if it is hardware
  // accelerated.
  if (gpu_factories) {
    // |gpu_factories_| requires that its entry points be called on its
    // |GetTaskRunner()|.  Since |pipeline_| will own decoders created from the
    // factories, require that their message loops are identical.
    DCHECK(gpu_factories->GetTaskRunner() == media_task_runner.get());

    if (decoder_factory_) {
      decoder_factory_->CreateVideoDecoders(media_task_runner, gpu_factories,
                                            media_log_, &video_decoders);
    }
    video_decoders.push_back(base::MakeUnique<GpuVideoDecoder>(
        gpu_factories, request_overlay_info_cb, target_color_space,
        media_log_));
  }

#if !defined(MEDIA_DISABLE_LIBVPX)
  video_decoders.push_back(base::MakeUnique<VpxVideoDecoder>());
#endif

#if !defined(MEDIA_DISABLE_FFMPEG) && !defined(DISABLE_FFMPEG_VIDEO_DECODERS)
  video_decoders.push_back(base::MakeUnique<FFmpegVideoDecoder>(media_log_));
#endif

  return video_decoders;
}

std::unique_ptr<Renderer> DefaultRendererFactory::CreateRenderer(
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    const scoped_refptr<base::TaskRunner>& worker_task_runner,
    AudioRendererSink* audio_renderer_sink,
    VideoRendererSink* video_renderer_sink,
    const RequestOverlayInfoCB& request_overlay_info_cb,
    const gfx::ColorSpace& target_color_space) {
  DCHECK(audio_renderer_sink);

  std::unique_ptr<AudioRenderer> audio_renderer(new AudioRendererImpl(
      media_task_runner, audio_renderer_sink,
      // Unretained is safe here, because the RendererFactory is guaranteed to
      // outlive the RendererImpl. The RendererImpl is destroyed when WMPI
      // destructor calls pipeline_controller_.Stop() -> PipelineImpl::Stop() ->
      // RendererWrapper::Stop -> RendererWrapper::DestroyRenderer(). And the
      // RendererFactory is owned by WMPI and gets called after WMPI destructor
      // finishes.
      base::Bind(&DefaultRendererFactory::CreateAudioDecoders,
                 base::Unretained(this), media_task_runner),
      media_log_));

  GpuVideoAcceleratorFactories* gpu_factories = nullptr;
  if (!get_gpu_factories_cb_.is_null())
    gpu_factories = get_gpu_factories_cb_.Run();

  std::unique_ptr<VideoRenderer> video_renderer(new VideoRendererImpl(
      media_task_runner, worker_task_runner, video_renderer_sink,
      // Unretained is safe here, because the RendererFactory is guaranteed to
      // outlive the RendererImpl. The RendererImpl is destroyed when WMPI
      // destructor calls pipeline_controller_.Stop() -> PipelineImpl::Stop() ->
      // RendererWrapper::Stop -> RendererWrapper::DestroyRenderer(). And the
      // RendererFactory is owned by WMPI and gets called after WMPI destructor
      // finishes.
      base::Bind(&DefaultRendererFactory::CreateVideoDecoders,
                 base::Unretained(this), media_task_runner,
                 request_overlay_info_cb, target_color_space, gpu_factories),
      true, gpu_factories, media_log_));

  return base::MakeUnique<RendererImpl>(
      media_task_runner, std::move(audio_renderer), std::move(video_renderer));
}

}  // namespace media
