// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_renderer_factory.h"

#include <utility>

#include "base/single_thread_task_runner.h"
#include "media/mojo/interfaces/service_factory.mojom.h"
#include "media/mojo/services/mojo_renderer_impl.h"
#include "mojo/public/cpp/bindings/interface_request.h"

namespace media {

MojoRendererFactory::MojoRendererFactory(
    interfaces::ServiceFactory* service_factory)
    : service_factory_(service_factory) {
  DCHECK(service_factory_);
}

MojoRendererFactory::~MojoRendererFactory() {
}

scoped_ptr<Renderer> MojoRendererFactory::CreateRenderer(
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    const scoped_refptr<base::TaskRunner>& /* worker_task_runner */,
    AudioRendererSink* /* audio_renderer_sink */,
    VideoRendererSink* /* video_renderer_sink */) {
  DCHECK(service_factory_);

  interfaces::RendererPtr mojo_renderer;
  service_factory_->CreateRenderer(mojo::GetProxy(&mojo_renderer));

  return scoped_ptr<Renderer>(
      new MojoRendererImpl(media_task_runner, std::move(mojo_renderer)));
}

}  // namespace media
