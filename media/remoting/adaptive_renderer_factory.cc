// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/adaptive_renderer_factory.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "media/remoting/courier_renderer.h"

namespace media {
namespace remoting {

AdaptiveRendererFactory::AdaptiveRendererFactory(
    std::unique_ptr<RendererFactory> default_renderer_factory,
    std::unique_ptr<RendererController> controller)
    : default_renderer_factory_(std::move(default_renderer_factory)),
      controller_(std::move(controller)) {}

AdaptiveRendererFactory::~AdaptiveRendererFactory() {}

std::unique_ptr<Renderer> AdaptiveRendererFactory::CreateRenderer(
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    const scoped_refptr<base::TaskRunner>& worker_task_runner,
    AudioRendererSink* audio_renderer_sink,
    VideoRendererSink* video_renderer_sink,
    const RequestSurfaceCB& request_surface_cb) {
  if (controller_ && controller_->remote_rendering_started()) {
    VLOG(1) << "Create Remoting renderer.";
    return base::MakeUnique<CourierRenderer>(
        media_task_runner, controller_->GetWeakPtr(), video_renderer_sink);
  } else {
    VLOG(1) << "Create Local playback renderer.";
    return default_renderer_factory_->CreateRenderer(
        media_task_runner, worker_task_runner, audio_renderer_sink,
        video_renderer_sink, request_surface_cb);
  }
}

}  // namespace remoting
}  // namespace media
