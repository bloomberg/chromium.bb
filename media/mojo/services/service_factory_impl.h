// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_SERVICE_FACTORY_IMPL_H_
#define MEDIA_MOJO_SERVICES_SERVICE_FACTORY_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "media/mojo/interfaces/service_factory.mojom.h"
#include "media/mojo/services/mojo_cdm_service_context.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/shell/public/cpp/connector.h"
#include "services/shell/public/cpp/shell_connection_ref.h"

namespace shell {
namespace mojom {
class InterfaceProvider;
}
}

namespace media {

class CdmFactory;
class MediaLog;
class MojoMediaClient;
class RendererFactory;

class ServiceFactoryImpl : public interfaces::ServiceFactory {
 public:
  ServiceFactoryImpl(mojo::InterfaceRequest<interfaces::ServiceFactory> request,
                     shell::mojom::InterfaceProvider* interfaces,
                     scoped_refptr<MediaLog> media_log,
                     std::unique_ptr<shell::ShellConnectionRef> connection_ref,
                     MojoMediaClient* mojo_media_client);
  ~ServiceFactoryImpl() final;

  // interfaces::ServiceFactory implementation.
  void CreateAudioDecoder(
      mojo::InterfaceRequest<interfaces::AudioDecoder> audio_decoder) final;
  void CreateRenderer(
      mojo::InterfaceRequest<interfaces::Renderer> renderer) final;
  void CreateCdm(
      mojo::InterfaceRequest<interfaces::ContentDecryptionModule> cdm) final;

 private:
#if defined(ENABLE_MOJO_RENDERER)
  RendererFactory* GetRendererFactory();

  std::unique_ptr<RendererFactory> renderer_factory_;
#endif  // defined(ENABLE_MOJO_RENDERER)

#if defined(ENABLE_MOJO_CDM)
  CdmFactory* GetCdmFactory();

  std::unique_ptr<CdmFactory> cdm_factory_;
#endif  // defined(ENABLE_MOJO_CDM)

  MojoCdmServiceContext cdm_service_context_;
  mojo::StrongBinding<interfaces::ServiceFactory> binding_;
  shell::mojom::InterfaceProvider* interfaces_;
  scoped_refptr<MediaLog> media_log_;
  std::unique_ptr<shell::ShellConnectionRef> connection_ref_;
  MojoMediaClient* mojo_media_client_;

  DISALLOW_COPY_AND_ASSIGN(ServiceFactoryImpl);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_SERVICE_FACTORY_IMPL_H_
