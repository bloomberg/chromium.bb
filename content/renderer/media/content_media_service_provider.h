// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_CONTENT_MEDIA_SERVICE_PROVIDER_H_
#define CONTENT_RENDERER_MEDIA_CONTENT_MEDIA_SERVICE_PROVIDER_H_

#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/public/renderer/render_frame_observer.h"
#include "media/mojo/services/media_service_provider.h"

namespace content {

class ServiceRegistry;

// content::ServiceRegistry based media::MediaServiceProvider implementation.
class CONTENT_EXPORT ContentMediaServiceProvider
    : public media::MediaServiceProvider,
      public RenderFrameObserver {
 public:
  ContentMediaServiceProvider(RenderFrame* render_frame,
                              ServiceRegistry* service_registry);
  ~ContentMediaServiceProvider() final;

  void ConnectToService(
      mojo::InterfacePtr<mojo::MediaRenderer>* media_renderer_ptr) final;

  void ConnectToService(
      mojo::InterfacePtr<mojo::ContentDecryptionModule>* cdm_ptr) final;

 private:
  ServiceRegistry* service_registry_;

  DISALLOW_COPY_AND_ASSIGN(ContentMediaServiceProvider);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_CONTENT_MEDIA_SERVICE_PROVIDER_H_
