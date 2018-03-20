// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/default_renderer_factory.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/single_thread_task_runner.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "media/base/decoder_factory.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/filters/gpu_memory_buffer_decoder_wrapper.h"
#include "media/filters/gpu_video_decoder.h"
#include "media/media_features.h"
#include "media/renderers/audio_renderer_impl.h"
#include "media/renderers/renderer_impl.h"
#include "media/renderers/video_renderer_impl.h"
#include "media/video/gpu_memory_buffer_video_frame_pool.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "third_party/libaom/av1_features.h"

#if !defined(OS_ANDROID)
#include "media/filters/decrypting_audio_decoder.h"
#include "media/filters/decrypting_video_decoder.h"
#endif

#if BUILDFLAG(ENABLE_AV1_DECODER)
#include "media/filters/aom_video_decoder.h"
#endif

#if BUILDFLAG(ENABLE_FFMPEG)
#include "media/filters/ffmpeg_audio_decoder.h"
#endif

#if BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
#include "media/filters/ffmpeg_video_decoder.h"
#endif

#if BUILDFLAG(ENABLE_LIBVPX)
#include "media/filters/vpx_video_decoder.h"
#endif

namespace media {

// TODO(dalecurtis): Remove this flag once committed to using either the
// GpuMemoryBufferDecoderWrapper or VideoRendererImpl + DecoderStream prepare.
// http://crbug.com/801245
#define USE_VIDEO_RENDERER_GMB

static std::unique_ptr<VideoDecoder> MaybeUseGpuMemoryBufferWrapper(
    GpuVideoAcceleratorFactories* gpu_factories,
    scoped_refptr<base::SingleThreadTaskRunner> media_task_runner,
    scoped_refptr<base::TaskRunner> worker_task_runner,
    std::unique_ptr<VideoDecoder> decoder) {
#if defined(USE_VIDEO_RENDERER_GMB)
  return decoder;
#else
  if (!gpu_factories ||
      !gpu_factories->ShouldUseGpuMemoryBuffersForVideoFrames()) {
    return decoder;
  }

  return std::make_unique<GpuMemoryBufferDecoderWrapper>(
      std::make_unique<GpuMemoryBufferVideoFramePool>(
          std::move(media_task_runner), std::move(worker_task_runner),
          gpu_factories),
      std::move(decoder));
#endif
}

DefaultRendererFactory::DefaultRendererFactory(
    MediaLog* media_log,
    DecoderFactory* decoder_factory,
    const GetGpuFactoriesCB& get_gpu_factories_cb)
    : media_log_(media_log),
      decoder_factory_(decoder_factory),
      get_gpu_factories_cb_(get_gpu_factories_cb) {}

DefaultRendererFactory::~DefaultRendererFactory() = default;

std::vector<std::unique_ptr<AudioDecoder>>
DefaultRendererFactory::CreateAudioDecoders(
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner) {
  // Create our audio decoders and renderer.
  std::vector<std::unique_ptr<AudioDecoder>> audio_decoders;

#if !defined(OS_ANDROID)
  audio_decoders.push_back(
      std::make_unique<DecryptingAudioDecoder>(media_task_runner, media_log_));
#endif

#if BUILDFLAG(ENABLE_FFMPEG)
  audio_decoders.push_back(
      std::make_unique<FFmpegAudioDecoder>(media_task_runner, media_log_));
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
    const scoped_refptr<base::TaskRunner>& worker_task_runner,
    const RequestOverlayInfoCB& request_overlay_info_cb,
    const gfx::ColorSpace& target_color_space,
    GpuVideoAcceleratorFactories* gpu_factories) {
  // TODO(crbug.com/789597): Move this (and CreateAudioDecoders) into a decoder
  // factory, and just call |decoder_factory_| here.

  // Create our video decoders and renderer.
  std::vector<std::unique_ptr<VideoDecoder>> video_decoders;

#if !defined(OS_ANDROID)
  video_decoders.push_back(MaybeUseGpuMemoryBufferWrapper(
      gpu_factories, media_task_runner, worker_task_runner,
      std::make_unique<DecryptingVideoDecoder>(media_task_runner, media_log_)));
#endif

  // Prefer an external decoder since one will only exist if it is hardware
  // accelerated.
  if (gpu_factories) {
    // |gpu_factories_| requires that its entry points be called on its
    // |GetTaskRunner()|.  Since |pipeline_| will own decoders created from the
    // factories, require that their message loops are identical.
    DCHECK(gpu_factories->GetTaskRunner() == media_task_runner.get());

    if (decoder_factory_) {
      decoder_factory_->CreateVideoDecoders(media_task_runner, gpu_factories,
                                            media_log_, request_overlay_info_cb,
                                            &video_decoders);
    }

    // MojoVideoDecoder replaces any VDA for this platform when it's enabled.
    if (!base::FeatureList::IsEnabled(media::kMojoVideoDecoder)) {
      video_decoders.push_back(std::make_unique<GpuVideoDecoder>(
          gpu_factories, request_overlay_info_cb, target_color_space,
          media_log_));
    }
  }

#if BUILDFLAG(ENABLE_LIBVPX)
  video_decoders.push_back(MaybeUseGpuMemoryBufferWrapper(
      gpu_factories, media_task_runner, worker_task_runner,
      std::make_unique<OffloadingVpxVideoDecoder>()));
#endif

#if BUILDFLAG(ENABLE_AV1_DECODER)
  if (base::FeatureList::IsEnabled(kAv1Decoder))
    video_decoders.push_back(MaybeUseGpuMemoryBufferWrapper(
        gpu_factories, media_task_runner, worker_task_runner,
        std::make_unique<AomVideoDecoder>(media_log_)));
#endif

#if BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
  video_decoders.push_back(MaybeUseGpuMemoryBufferWrapper(
      gpu_factories, media_task_runner, worker_task_runner,
      std::make_unique<FFmpegVideoDecoder>(media_log_)));
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

  std::unique_ptr<GpuMemoryBufferVideoFramePool> gmb_pool;
#if defined(USE_VIDEO_RENDERER_GMB)
  if (gpu_factories &&
      gpu_factories->ShouldUseGpuMemoryBuffersForVideoFrames()) {
    gmb_pool = std::make_unique<GpuMemoryBufferVideoFramePool>(
        std::move(media_task_runner), std::move(worker_task_runner),
        gpu_factories);
  }
#endif

  std::unique_ptr<VideoRenderer> video_renderer(new VideoRendererImpl(
      media_task_runner, video_renderer_sink,
      // Unretained is safe here, because the RendererFactory is guaranteed to
      // outlive the RendererImpl. The RendererImpl is destroyed when WMPI
      // destructor calls pipeline_controller_.Stop() -> PipelineImpl::Stop() ->
      // RendererWrapper::Stop -> RendererWrapper::DestroyRenderer(). And the
      // RendererFactory is owned by WMPI and gets called after WMPI destructor
      // finishes.
      base::Bind(&DefaultRendererFactory::CreateVideoDecoders,
                 base::Unretained(this), media_task_runner, worker_task_runner,
                 request_overlay_info_cb, target_color_space, gpu_factories),
      true, media_log_, std::move(gmb_pool)));

  return std::make_unique<RendererImpl>(
      media_task_runner, std::move(audio_renderer), std::move(video_renderer));
}

}  // namespace media
