// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/renderer/media/chromecast_media_renderer_factory.h"

#include "base/command_line.h"
#include "chromecast/media/base/switching_media_renderer.h"
#include "chromecast/renderer/media/cma_renderer.h"
#include "chromecast/renderer/media/media_pipeline_proxy.h"
#include "content/public/renderer/render_thread.h"
#include "media/base/audio_hardware_config.h"
#include "media/base/media_log.h"
#include "media/renderers/default_renderer_factory.h"
#include "media/renderers/gpu_video_accelerator_factories.h"

namespace chromecast {
namespace media {

ChromecastMediaRendererFactory::ChromecastMediaRendererFactory(
    const scoped_refptr<::media::GpuVideoAcceleratorFactories>& gpu_factories,
    const scoped_refptr<::media::MediaLog>& media_log,
    int render_frame_id)
    : render_frame_id_(render_frame_id),
      gpu_factories_(gpu_factories),
      media_log_(media_log) {
}

ChromecastMediaRendererFactory::~ChromecastMediaRendererFactory() {
}

scoped_ptr<::media::Renderer> ChromecastMediaRendererFactory::CreateRenderer(
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    const scoped_refptr<base::TaskRunner>& worker_task_runner,
    ::media::AudioRendererSink* audio_renderer_sink,
    ::media::VideoRendererSink* video_renderer_sink) {
  if (!default_renderer_factory_) {
    // Chromecast doesn't have input audio devices, so leave this uninitialized
    ::media::AudioParameters input_audio_params;
    // TODO(servolk): Audio parameters are hardcoded for now, but in the future
    // either we need to obtain AudioHardwareConfig from RenderThreadImpl,
    // or media renderer needs to figure out optimal audio parameters itself.
    const int kDefaultSamplingRate = 48000;
    const int kDefaultBitsPerSample = 16;
    // About 20ms of stereo (2 channels) 16bit (2 byte) audio
    int buffer_size = kDefaultSamplingRate * 20 * 2 * 2 / 1000;
    ::media::AudioParameters output_audio_params(
        ::media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
        ::media::CHANNEL_LAYOUT_STEREO, kDefaultSamplingRate,
        kDefaultBitsPerSample, buffer_size);

    audio_config_.reset(new ::media::AudioHardwareConfig(input_audio_params,
                                                         output_audio_params));

    default_renderer_factory_.reset(new ::media::DefaultRendererFactory(
        media_log_, /*gpu_factories*/ nullptr, *audio_config_));
  }

  DCHECK(default_renderer_factory_);
  // TODO(erickung): crbug.com/443956. Need to provide right LoadType.
  LoadType cma_load_type = kLoadTypeMediaSource;
  scoped_ptr<MediaPipelineProxy> cma_media_pipeline(new MediaPipelineProxy(
      render_frame_id_,
      content::RenderThread::Get()->GetIOMessageLoopProxy(),
      cma_load_type));
  scoped_ptr<CmaRenderer> cma_renderer(new CmaRenderer(
      cma_media_pipeline.Pass(), video_renderer_sink, gpu_factories_));
  scoped_ptr<::media::Renderer> default_media_renderer(
      default_renderer_factory_->CreateRenderer(media_task_runner,
                                                media_task_runner,
                                                audio_renderer_sink,
                                                video_renderer_sink));
  scoped_ptr<SwitchingMediaRenderer> media_renderer(new SwitchingMediaRenderer(
      default_media_renderer.Pass(), cma_renderer.Pass()));
  return media_renderer.Pass();
}

}  // namespace media
}  // namespace chromecast
