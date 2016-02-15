// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_RENDERER_FACTORY_H_
#define MEDIA_MOJO_SERVICES_MOJO_RENDERER_FACTORY_H_

#include "base/macros.h"
#include "media/base/media_export.h"
#include "media/base/renderer_factory.h"
#include "media/mojo/interfaces/renderer.mojom.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"

namespace media {

namespace interfaces {
class ServiceFactory;
}

// The default factory class for creating MojoRendererImpl.
class MojoRendererFactory : public RendererFactory {
 public:
  explicit MojoRendererFactory(interfaces::ServiceFactory* service_factory);
  ~MojoRendererFactory() final;

  scoped_ptr<Renderer> CreateRenderer(
      const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
      const scoped_refptr<base::TaskRunner>& worker_task_runner,
      AudioRendererSink* audio_renderer_sink,
      VideoRendererSink* video_renderer_sink) final;

 private:
  interfaces::ServiceFactory* service_factory_;

  DISALLOW_COPY_AND_ASSIGN(MojoRendererFactory);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_RENDERER_FACTORY_H_
