// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/renderer/media/cma_media_renderer_factory.h"

#include "base/command_line.h"
#include "chromecast/media/cma/filters/cma_renderer.h"
#include "chromecast/renderer/media/media_pipeline_proxy.h"
#include "content/public/renderer/render_thread.h"

namespace chromecast {
namespace media {

CmaMediaRendererFactory::CmaMediaRendererFactory(int render_frame_id)
    : render_frame_id_(render_frame_id) {
}

CmaMediaRendererFactory::~CmaMediaRendererFactory() {
}

scoped_ptr< ::media::Renderer> CmaMediaRendererFactory::CreateRenderer(
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    ::media::AudioRendererSink* audio_renderer_sink) {
  // TODO(erickung): crbug.com/443956. Need to provide right LoadType.
  LoadType cma_load_type = kLoadTypeMediaSource;
  scoped_ptr<MediaPipeline> cma_media_pipeline(
      new MediaPipelineProxy(
          render_frame_id_,
          content::RenderThread::Get()->GetIOMessageLoopProxy(),
          cma_load_type));
  return scoped_ptr< ::media::Renderer>(
      new CmaRenderer(cma_media_pipeline.Pass()));
}

}  // namespace media
}  // namespace chromecast