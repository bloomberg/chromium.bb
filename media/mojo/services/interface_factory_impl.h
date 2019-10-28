// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_INTERFACE_FACTORY_IMPL_H_
#define MEDIA_MOJO_SERVICES_INTERFACE_FACTORY_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "media/base/media_util.h"
#include "media/mojo/buildflags.h"
#include "media/mojo/mojom/audio_decoder.mojom.h"
#include "media/mojo/mojom/cdm_proxy.mojom.h"
#include "media/mojo/mojom/content_decryption_module.mojom.h"
#include "media/mojo/mojom/decryptor.mojom.h"
#include "media/mojo/mojom/interface_factory.mojom.h"
#include "media/mojo/mojom/renderer.mojom.h"
#include "media/mojo/mojom/video_decoder.mojom.h"
#include "media/mojo/services/deferred_destroy_strong_binding_set.h"
#include "media/mojo/services/mojo_cdm_service_context.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service_keepalive.h"

namespace media {

class CdmFactory;
class MojoMediaClient;

class InterfaceFactoryImpl : public DeferredDestroy<mojom::InterfaceFactory> {
 public:
  InterfaceFactoryImpl(
      mojo::PendingRemote<service_manager::mojom::InterfaceProvider>
          host_interfaces,
      std::unique_ptr<service_manager::ServiceKeepaliveRef> keepalive_ref,
      MojoMediaClient* mojo_media_client);
  ~InterfaceFactoryImpl() final;

  // mojom::InterfaceFactory implementation.
  void CreateAudioDecoder(
      mojo::PendingReceiver<mojom::AudioDecoder> receiver) final;
  void CreateVideoDecoder(
      mojo::PendingReceiver<mojom::VideoDecoder> receiver) final;
  void CreateDefaultRenderer(const std::string& audio_device_id,
                             mojom::RendererRequest request) final;
#if BUILDFLAG(ENABLE_CAST_RENDERER)
  void CreateCastRenderer(
      const base::UnguessableToken& overlay_plane_id,
      mojo::PendingReceiver<media::mojom::Renderer> receiver) final;
#endif
#if defined(OS_ANDROID)
  void CreateMediaPlayerRenderer(
      mojo::PendingRemote<mojom::MediaPlayerRendererClientExtension>
          client_extension_remote,
      mojo::PendingReceiver<mojom::Renderer> receiver,
      mojo::PendingReceiver<mojom::MediaPlayerRendererExtension>
          renderer_extension_receiver) final;
  void CreateFlingingRenderer(
      const std::string& presentation_id,
      mojo::PendingRemote<mojom::FlingingRendererClientExtension>
          client_extension,
      mojo::PendingReceiver<mojom::Renderer> receiver) final;
#endif  // defined(OS_ANDROID)
  void CreateCdm(const std::string& key_system,
                 mojom::ContentDecryptionModuleRequest request) final;
  void CreateDecryptor(int cdm_id,
                       mojo::PendingReceiver<mojom::Decryptor> receiver) final;
  void CreateCdmProxy(const base::Token& cdm_guid,
                      mojo::PendingReceiver<mojom::CdmProxy> receiver) final;

  // DeferredDestroy<mojom::InterfaceFactory> implemenation.
  void OnDestroyPending(base::OnceClosure destroy_cb) final;

 private:
  // Returns true when there is no media component (audio/video decoder,
  // renderer, cdm and cdm proxy) bindings exist.
  bool IsEmpty();

  void SetBindingConnectionErrorHandler();
  void OnBindingConnectionError();

#if BUILDFLAG(ENABLE_MOJO_CDM)
  CdmFactory* GetCdmFactory();
#endif  // BUILDFLAG(ENABLE_MOJO_CDM)

  // Must be declared before the bindings below because the bound objects might
  // take a raw pointer of |cdm_service_context_| and assume it's always
  // available.
  MojoCdmServiceContext cdm_service_context_;

#if BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER)
  mojo::UniqueReceiverSet<mojom::AudioDecoder> audio_decoder_receivers_;
#endif  // BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER)

#if BUILDFLAG(ENABLE_MOJO_VIDEO_DECODER)
  mojo::UniqueReceiverSet<mojom::VideoDecoder> video_decoder_receivers_;
#endif  // BUILDFLAG(ENABLE_MOJO_VIDEO_DECODER)

#if BUILDFLAG(ENABLE_MOJO_RENDERER) || BUILDFLAG(ENABLE_CAST_RENDERER)
  // TODO(xhwang): Use MojoMediaLog for Renderer.
  NullMediaLog media_log_;
  mojo::StrongBindingSet<mojom::Renderer> renderer_bindings_;
#endif  // BUILDFLAG(ENABLE_MOJO_RENDERER) || BUILDFLAG(ENABLE_CAST_RENDERER)

#if BUILDFLAG(ENABLE_MOJO_CDM)
  std::unique_ptr<CdmFactory> cdm_factory_;
  mojo::StrongBindingSet<mojom::ContentDecryptionModule> cdm_bindings_;
#endif  // BUILDFLAG(ENABLE_MOJO_CDM)

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
  mojo::UniqueReceiverSet<mojom::CdmProxy> cdm_proxy_receivers_;
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

  mojo::Remote<service_manager::mojom::InterfaceProvider> host_interfaces_;

  mojo::UniqueReceiverSet<mojom::Decryptor> decryptor_receivers_;

  std::unique_ptr<service_manager::ServiceKeepaliveRef> keepalive_ref_;
  MojoMediaClient* mojo_media_client_;
  base::OnceClosure destroy_cb_;

  DISALLOW_COPY_AND_ASSIGN(InterfaceFactoryImpl);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_INTERFACE_FACTORY_IMPL_H_
