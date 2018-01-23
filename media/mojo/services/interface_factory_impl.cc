// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/interface_factory_impl.h"

#include <memory>
#include "base/guid.h"

#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/base/media_log.h"
#include "media/mojo/services/mojo_media_client.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/service_manager/public/interfaces/interface_provider.mojom.h"

#if BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER)
#include "media/mojo/services/mojo_audio_decoder_service.h"
#endif  // BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER)

#if BUILDFLAG(ENABLE_MOJO_VIDEO_DECODER)
#include "media/mojo/services/mojo_video_decoder_service.h"
#endif  // BUILDFLAG(ENABLE_MOJO_VIDEO_DECODER)

#if BUILDFLAG(ENABLE_MOJO_RENDERER)
#include "base/bind_helpers.h"
#include "media/base/audio_renderer_sink.h"
#include "media/base/renderer_factory.h"
#include "media/base/video_renderer_sink.h"
#include "media/mojo/services/mojo_renderer_service.h"
#endif  // BUILDFLAG(ENABLE_MOJO_RENDERER)

#if BUILDFLAG(ENABLE_MOJO_CDM)
#include "media/base/cdm_factory.h"
#include "media/mojo/services/mojo_cdm_service.h"
#endif  // BUILDFLAG(ENABLE_MOJO_CDM)

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
#include "media/mojo/services/mojo_cdm_proxy_service.h"
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

namespace media {

InterfaceFactoryImpl::InterfaceFactoryImpl(
    service_manager::mojom::InterfaceProviderPtr interfaces,
    MediaLog* media_log,
    std::unique_ptr<service_manager::ServiceContextRef> connection_ref,
    MojoMediaClient* mojo_media_client)
    :
#if BUILDFLAG(ENABLE_MOJO_RENDERER)
      media_log_(media_log),
#endif
#if BUILDFLAG(ENABLE_MOJO_CDM)
      interfaces_(std::move(interfaces)),
#endif
      connection_ref_(std::move(connection_ref)),
      mojo_media_client_(mojo_media_client) {
  DVLOG(1) << __func__;
  DCHECK(mojo_media_client_);
}

InterfaceFactoryImpl::~InterfaceFactoryImpl() {
  DVLOG(1) << __func__;
}

// mojom::InterfaceFactory implementation.

void InterfaceFactoryImpl::CreateAudioDecoder(
    mojo::InterfaceRequest<mojom::AudioDecoder> request) {
#if BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER)
  scoped_refptr<base::SingleThreadTaskRunner> task_runner(
      base::ThreadTaskRunnerHandle::Get());

  std::unique_ptr<AudioDecoder> audio_decoder =
      mojo_media_client_->CreateAudioDecoder(task_runner);
  if (!audio_decoder) {
    DLOG(ERROR) << "AudioDecoder creation failed.";
    return;
  }

  audio_decoder_bindings_.AddBinding(
      std::make_unique<MojoAudioDecoderService>(&cdm_service_context_,
                                                std::move(audio_decoder)),
      std::move(request));
#endif  // BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER)
}

void InterfaceFactoryImpl::CreateVideoDecoder(
    mojom::VideoDecoderRequest request) {
#if BUILDFLAG(ENABLE_MOJO_VIDEO_DECODER)
  video_decoder_bindings_.AddBinding(
      std::make_unique<MojoVideoDecoderService>(mojo_media_client_,
                                                &cdm_service_context_),
      std::move(request));
#endif  // BUILDFLAG(ENABLE_MOJO_VIDEO_DECODER)
}

void InterfaceFactoryImpl::CreateRenderer(
    const std::string& audio_device_id,
    mojo::InterfaceRequest<mojom::Renderer> request) {
#if BUILDFLAG(ENABLE_MOJO_RENDERER)
  RendererFactory* renderer_factory = GetRendererFactory();
  if (!renderer_factory)
    return;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner(
      base::ThreadTaskRunnerHandle::Get());
  auto audio_sink =
      mojo_media_client_->CreateAudioRendererSink(audio_device_id);
  auto video_sink = mojo_media_client_->CreateVideoRendererSink(task_runner);
  // TODO(hubbe): Find out if gfx::ColorSpace() is correct for the
  // target_color_space.
  auto renderer = renderer_factory->CreateRenderer(
      task_runner, task_runner, audio_sink.get(), video_sink.get(),
      RequestOverlayInfoCB(), gfx::ColorSpace());
  if (!renderer) {
    DLOG(ERROR) << "Renderer creation failed.";
    return;
  }

  std::unique_ptr<MojoRendererService> mojo_renderer_service =
      std::make_unique<MojoRendererService>(
          &cdm_service_context_, std::move(audio_sink), std::move(video_sink),
          std::move(renderer), MojoRendererService::InitiateSurfaceRequestCB());

  MojoRendererService* mojo_renderer_service_ptr = mojo_renderer_service.get();

  mojo::BindingId binding_id = renderer_bindings_.AddBinding(
      std::move(mojo_renderer_service), std::move(request));

  // base::Unretained() is safe because the callback will be fired by
  // |mojo_renderer_service|, which is owned by |renderer_bindings_|.
  mojo_renderer_service_ptr->set_bad_message_cb(
      base::Bind(base::IgnoreResult(
                     &mojo::StrongBindingSet<mojom::Renderer>::RemoveBinding),
                 base::Unretained(&renderer_bindings_), binding_id));
#endif  // BUILDFLAG(ENABLE_MOJO_RENDERER)
}

void InterfaceFactoryImpl::CreateCdm(
    const std::string& /* key_system */,
    mojo::InterfaceRequest<mojom::ContentDecryptionModule> request) {
#if BUILDFLAG(ENABLE_MOJO_CDM)
  CdmFactory* cdm_factory = GetCdmFactory();
  if (!cdm_factory)
    return;

  cdm_bindings_.AddBinding(
      std::make_unique<MojoCdmService>(&cdm_service_context_, cdm_factory),
      std::move(request));
#endif  // BUILDFLAG(ENABLE_MOJO_CDM)
}

void InterfaceFactoryImpl::CreateCdmProxy(const std::string& cdm_guid,
                                          mojom::CdmProxyRequest request) {
#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
  if (!base::IsValidGUID(cdm_guid)) {
    DLOG(ERROR) << "Invalid CDM GUID: " << cdm_guid;
    return;
  }

  auto cdm_proxy = mojo_media_client_->CreateCdmProxy(cdm_guid);
  if (!cdm_proxy) {
    DLOG(ERROR) << "CdmProxy creation failed.";
    return;
  }

  cdm_proxy_bindings_.AddBinding(
      std::make_unique<MojoCdmProxyService>(std::move(cdm_proxy),
                                            &cdm_service_context_),
      std::move(request));
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)
}

#if BUILDFLAG(ENABLE_MOJO_RENDERER)
RendererFactory* InterfaceFactoryImpl::GetRendererFactory() {
  if (!renderer_factory_) {
    renderer_factory_ = mojo_media_client_->CreateRendererFactory(media_log_);
    LOG_IF(ERROR, !renderer_factory_) << "RendererFactory not available.";
  }
  return renderer_factory_.get();
}
#endif  // BUILDFLAG(ENABLE_MOJO_RENDERER)

#if BUILDFLAG(ENABLE_MOJO_CDM)
CdmFactory* InterfaceFactoryImpl::GetCdmFactory() {
  if (!cdm_factory_) {
    cdm_factory_ = mojo_media_client_->CreateCdmFactory(interfaces_.get());
    LOG_IF(ERROR, !cdm_factory_) << "CdmFactory not available.";
  }
  return cdm_factory_.get();
}
#endif  // BUILDFLAG(ENABLE_MOJO_CDM)

}  // namespace media
