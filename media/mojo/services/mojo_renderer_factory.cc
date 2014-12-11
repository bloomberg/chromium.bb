// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_renderer_factory.h"

#include "base/single_thread_task_runner.h"
#include "media/mojo/interfaces/media_renderer.mojom.h"
#include "media/mojo/services/mojo_renderer_impl.h"
#include "mojo/public/cpp/application/connect.h"

namespace media {

MojoRendererFactory::MojoRendererFactory(
    mojo::ServiceProviderPtr renderer_service_provider)
    : renderer_service_provider_(renderer_service_provider.Pass()) {
}

MojoRendererFactory::~MojoRendererFactory() {
}

scoped_ptr<Renderer> MojoRendererFactory::CreateRenderer(
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    AudioRendererSink* /* audio_renderer_sink */) {
  DCHECK(renderer_service_provider_.get());
  mojo::MediaRendererPtr mojo_media_renderer;
  mojo::ConnectToService(renderer_service_provider_.get(),
                         &mojo_media_renderer);
  return scoped_ptr<Renderer>(
      new MojoRendererImpl(media_task_runner, mojo_media_renderer.Pass()));
}

}  // namespace media
