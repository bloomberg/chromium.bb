// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/clients/mojo_renderer_factory.h"

#include <memory>

#include "base/single_thread_task_runner.h"
#include "media/mojo/clients/mojo_renderer.h"
#include "media/mojo/interfaces/interface_factory.mojom.h"
#include "media/renderers/video_overlay_factory.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/service_manager/public/cpp/connect.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace media {

MojoRendererFactory::MojoRendererFactory(
    const GetGpuFactoriesCB& get_gpu_factories_cb,
    media::mojom::InterfaceFactory* interface_factory)
    : get_gpu_factories_cb_(get_gpu_factories_cb),
      interface_factory_(interface_factory) {
  DCHECK(interface_factory_);
  DCHECK(!interface_provider_);
}

MojoRendererFactory::MojoRendererFactory(
    const GetGpuFactoriesCB& get_gpu_factories_cb,
    service_manager::InterfaceProvider* interface_provider)
    : get_gpu_factories_cb_(get_gpu_factories_cb),
      interface_provider_(interface_provider) {
  DCHECK(interface_provider_);
  DCHECK(!interface_factory_);
}

MojoRendererFactory::~MojoRendererFactory() = default;

std::unique_ptr<Renderer> MojoRendererFactory::CreateRenderer(
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    const scoped_refptr<base::TaskRunner>& /* worker_task_runner */,
    AudioRendererSink* /* audio_renderer_sink */,
    VideoRendererSink* video_renderer_sink,
    const RequestOverlayInfoCB& /* request_overlay_info_cb */,
    const gfx::ColorSpace& /* target_color_space */) {
  std::unique_ptr<VideoOverlayFactory> overlay_factory;

  // |get_gpu_factories_cb_| can be null in the HLS/MediaPlayerRenderer case,
  // when we do not need to create video overlays.
  if (!get_gpu_factories_cb_.is_null()) {
    overlay_factory =
        std::make_unique<VideoOverlayFactory>(get_gpu_factories_cb_.Run());
  }

  return std::unique_ptr<Renderer>(
      new MojoRenderer(media_task_runner, std::move(overlay_factory),
                       video_renderer_sink, GetRendererPtr()));
}

mojom::RendererPtr MojoRendererFactory::GetRendererPtr() {
  mojom::RendererPtr renderer_ptr;

  if (interface_factory_) {
    interface_factory_->CreateRenderer(std::string(),
                                       mojo::MakeRequest(&renderer_ptr));
  } else if (interface_provider_) {
    interface_provider_->GetInterface(&renderer_ptr);
  } else {
    NOTREACHED();
  }

  return renderer_ptr;
}

}  // namespace media
