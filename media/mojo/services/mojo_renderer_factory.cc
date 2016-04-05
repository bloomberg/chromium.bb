// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_renderer_factory.h"

#include "base/single_thread_task_runner.h"
#include "media/mojo/services/mojo_renderer_impl.h"
#include "mojo/shell/public/cpp/connect.h"
#include "mojo/shell/public/interfaces/interface_provider.mojom.h"

namespace media {

MojoRendererFactory::MojoRendererFactory(
    mojo::shell::mojom::InterfaceProvider* interface_provider)
    : interface_provider_(interface_provider) {
  DCHECK(interface_provider_);
}

MojoRendererFactory::~MojoRendererFactory() {
}

std::unique_ptr<Renderer> MojoRendererFactory::CreateRenderer(
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    const scoped_refptr<base::TaskRunner>& /* worker_task_runner */,
    AudioRendererSink* /* audio_renderer_sink */,
    VideoRendererSink* /* video_renderer_sink */,
    const RequestSurfaceCB& /* request_surface_cb */) {
  interfaces::RendererPtr renderer_ptr;
  mojo::GetInterface<interfaces::Renderer>(interface_provider_, &renderer_ptr);

  return std::unique_ptr<Renderer>(
      new MojoRendererImpl(media_task_runner, std::move(renderer_ptr)));
}

}  // namespace media
