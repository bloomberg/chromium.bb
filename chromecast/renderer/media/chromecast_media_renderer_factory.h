// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_RENDERER_MEDIA_CHROMECAST_MEDIA_RENDERER_FACTORY_H_
#define CHROMECAST_RENDERER_MEDIA_CHROMECAST_MEDIA_RENDERER_FACTORY_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "media/base/renderer_factory.h"

namespace media {
class AudioHardwareConfig;
class GpuVideoAcceleratorFactories;
class MediaLog;
class DefaultRendererFactory;
}

namespace chromecast {
namespace media {

class ChromecastMediaRendererFactory : public ::media::RendererFactory {
 public:
  ChromecastMediaRendererFactory(
      const scoped_refptr<::media::GpuVideoAcceleratorFactories>& gpu_factories,
      const scoped_refptr<::media::MediaLog>& media_log,
      int render_frame_id);
  ~ChromecastMediaRendererFactory() final;

  // ::media::RendererFactory implementation.
  scoped_ptr<::media::Renderer> CreateRenderer(
      const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
      const scoped_refptr<base::TaskRunner>& worker_task_runner,
      ::media::AudioRendererSink* audio_renderer_sink,
      ::media::VideoRendererSink* video_renderer_sink) final;

 private:
  int render_frame_id_;
  scoped_refptr<::media::GpuVideoAcceleratorFactories> gpu_factories_;
  scoped_refptr<::media::MediaLog> media_log_;
  scoped_ptr<::media::DefaultRendererFactory> default_renderer_factory_;

  // Audio config for the default media renderer.
  scoped_ptr<::media::AudioHardwareConfig> audio_config_;

  DISALLOW_COPY_AND_ASSIGN(ChromecastMediaRendererFactory);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_RENDERER_MEDIA_CHROMECAST_MEDIA_RENDERER_FACTORY_H_
