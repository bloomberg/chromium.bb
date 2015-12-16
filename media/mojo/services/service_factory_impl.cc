// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/service_factory_impl.h"

#include "base/logging.h"
#include "media/base/cdm_factory.h"
#include "media/base/media_log.h"
#include "media/base/renderer_factory.h"
#include "media/mojo/services/mojo_cdm_service.h"
#include "media/mojo/services/mojo_media_client.h"
#include "media/mojo/services/mojo_renderer_service.h"
#include "mojo/application/public/cpp/app_lifetime_helper.h"
#include "mojo/application/public/interfaces/service_provider.mojom.h"

namespace media {

ServiceFactoryImpl::ServiceFactoryImpl(
    mojo::InterfaceRequest<interfaces::ServiceFactory> request,
    mojo::ServiceProvider* service_provider,
    scoped_refptr<MediaLog> media_log,
    scoped_ptr<mojo::AppRefCount> parent_app_refcount)
    : binding_(this, std::move(request)),
      service_provider_(service_provider),
      media_log_(media_log),
      parent_app_refcount_(std::move(parent_app_refcount)) {
  DVLOG(1) << __FUNCTION__;
}

ServiceFactoryImpl::~ServiceFactoryImpl() {
  DVLOG(1) << __FUNCTION__;
}

// interfaces::ServiceFactory implementation.
void ServiceFactoryImpl::CreateRenderer(
    mojo::InterfaceRequest<interfaces::Renderer> request) {
  // The created object is owned by the pipe.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner(
      base::MessageLoop::current()->task_runner());
  MojoMediaClient* mojo_media_client = MojoMediaClient::Get();
  scoped_refptr<AudioRendererSink> audio_renderer_sink =
      mojo_media_client->CreateAudioRendererSink();
  scoped_ptr<VideoRendererSink> video_renderer_sink =
      mojo_media_client->CreateVideoRendererSink(task_runner);
  scoped_ptr<Renderer> renderer = GetRendererFactory()->CreateRenderer(
      task_runner, task_runner, audio_renderer_sink.get(),
      video_renderer_sink.get());

  new MojoRendererService(cdm_service_context_.GetWeakPtr(),
                          std::move(renderer), std::move(request));
}

void ServiceFactoryImpl::CreateCdm(
    mojo::InterfaceRequest<interfaces::ContentDecryptionModule> request) {
  // The created object is owned by the pipe.
  new MojoCdmService(cdm_service_context_.GetWeakPtr(), service_provider_,
                     GetCdmFactory(), std::move(request));
}

RendererFactory* ServiceFactoryImpl::GetRendererFactory() {
  if (!renderer_factory_)
    renderer_factory_ =
        MojoMediaClient::Get()->CreateRendererFactory(media_log_);
  return renderer_factory_.get();
}

CdmFactory* ServiceFactoryImpl::GetCdmFactory() {
  if (!cdm_factory_)
    cdm_factory_ = MojoMediaClient::Get()->CreateCdmFactory(service_provider_);
  return cdm_factory_.get();
}

}  // namespace media
