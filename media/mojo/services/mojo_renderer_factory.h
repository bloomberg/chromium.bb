// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_RENDERER_FACTORY_H_
#define MEDIA_MOJO_SERVICES_MOJO_RENDERER_FACTORY_H_

#include "media/base/media_export.h"
#include "media/base/renderer_factory.h"
#include "media/mojo/interfaces/media_renderer.mojom.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/interface_ptr.h"

namespace media {

// The default factory class for creating MojoRendererImpl.
class MEDIA_EXPORT MojoRendererFactory : public RendererFactory {
 public:
  // A class that can help get a mojo::MediaRenderer service for
  // MojoRendererFactory.
  class ServiceProvider {
   public:
    ServiceProvider() {};
    virtual ~ServiceProvider() {};
    virtual void ConnectToService(
        mojo::InterfacePtr<mojo::MediaRenderer>* media_renderer_ptr) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(ServiceProvider);
  };

  explicit MojoRendererFactory(scoped_ptr<ServiceProvider> service_provider);
  ~MojoRendererFactory() final;

  scoped_ptr<Renderer> CreateRenderer(
      const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
      AudioRendererSink* audio_renderer_sink) final;

 private:
  scoped_ptr<ServiceProvider> service_provider_;

  DISALLOW_COPY_AND_ASSIGN(MojoRendererFactory);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_RENDERER_FACTORY_H_
