// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_SERVICE_FACTORY_IMPL_H_
#define MEDIA_MOJO_SERVICES_SERVICE_FACTORY_IMPL_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "media/mojo/interfaces/service_factory.mojom.h"
#include "media/mojo/services/mojo_cdm_service_context.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace mojo {
class AppRefCount;
class InterfaceProvider;
}

namespace media {

class CdmFactory;
class MediaLog;
class MojoMediaClient;
class RendererFactory;

class ServiceFactoryImpl : public interfaces::ServiceFactory {
 public:
  ServiceFactoryImpl(mojo::InterfaceRequest<interfaces::ServiceFactory> request,
                     mojo::InterfaceProvider* interfaces,
                     scoped_refptr<MediaLog> media_log,
                     scoped_ptr<mojo::AppRefCount> parent_app_refcount,
                     MojoMediaClient* mojo_media_client);
  ~ServiceFactoryImpl() final;

  // interfaces::ServiceFactory implementation.
  void CreateRenderer(
      mojo::InterfaceRequest<interfaces::Renderer> renderer) final;
  void CreateCdm(
      mojo::InterfaceRequest<interfaces::ContentDecryptionModule> cdm) final;

 private:
  RendererFactory* GetRendererFactory();
  CdmFactory* GetCdmFactory();

  MojoCdmServiceContext cdm_service_context_;

  mojo::StrongBinding<interfaces::ServiceFactory> binding_;
  mojo::InterfaceProvider* interfaces_;
  scoped_refptr<MediaLog> media_log_;
  scoped_ptr<mojo::AppRefCount> parent_app_refcount_;
  MojoMediaClient* mojo_media_client_;

  scoped_ptr<RendererFactory> renderer_factory_;
  scoped_ptr<CdmFactory> cdm_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceFactoryImpl);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_SERVICE_FACTORY_IMPL_H_
