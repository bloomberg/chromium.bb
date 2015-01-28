// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_RENDERER_SERVICE_PROVIDER_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_RENDERER_SERVICE_PROVIDER_H_

#include "base/macros.h"
#include "content/common/content_export.h"
#include "media/mojo/services/mojo_renderer_factory.h"

namespace content {

class ServiceRegistry;

// ServiceRegistry based media::MojoRendererFactory::ServiceProvider
// implementation.
class CONTENT_EXPORT MediaRendererServiceProvider
    : public media::MojoRendererFactory::ServiceProvider {
 public:
  explicit MediaRendererServiceProvider(ServiceRegistry* service_registry);
  ~MediaRendererServiceProvider() final;

  void ConnectToService(
      mojo::InterfacePtr<mojo::MediaRenderer>* media_renderer_ptr) final;

 private:
  ServiceRegistry* service_registry_;

  DISALLOW_COPY_AND_ASSIGN(MediaRendererServiceProvider);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_RENDERER_SERVICE_PROVIDER_H_
