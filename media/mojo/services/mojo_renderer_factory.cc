// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_renderer_factory.h"

#include "base/single_thread_task_runner.h"
#include "media/mojo/services/media_service_provider.h"
#include "media/mojo/services/mojo_renderer_impl.h"

namespace media {

MojoRendererFactory::MojoRendererFactory(
    MediaServiceProvider* media_service_provider)
    : media_service_provider_(media_service_provider) {
  DCHECK(media_service_provider_);
}

MojoRendererFactory::~MojoRendererFactory() {
}

scoped_ptr<Renderer> MojoRendererFactory::CreateRenderer(
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    AudioRendererSink* /* audio_renderer_sink */,
    VideoRendererSink* /* video_renderer_sink */) {
  mojo::MediaRendererPtr mojo_media_renderer;
  DCHECK(media_service_provider_);
  media_service_provider_->ConnectToService(&mojo_media_renderer);
  return scoped_ptr<Renderer>(
      new MojoRendererImpl(media_task_runner, mojo_media_renderer.Pass()));
}

}  // namespace media
