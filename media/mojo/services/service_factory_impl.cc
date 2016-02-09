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
#include "mojo/shell/public/cpp/app_lifetime_helper.h"
#include "mojo/shell/public/interfaces/service_provider.mojom.h"

namespace media {

ServiceFactoryImpl::ServiceFactoryImpl(
    mojo::InterfaceRequest<interfaces::ServiceFactory> request,
    mojo::ServiceProvider* service_provider,
    scoped_refptr<MediaLog> media_log,
    scoped_ptr<mojo::AppRefCount> parent_app_refcount,
    MojoMediaClient* mojo_media_client)
    : binding_(this, std::move(request)),
      service_provider_(service_provider),
      media_log_(media_log),
      parent_app_refcount_(std::move(parent_app_refcount)),
      mojo_media_client_(mojo_media_client) {
  DVLOG(1) << __FUNCTION__;
}

ServiceFactoryImpl::~ServiceFactoryImpl() {
  DVLOG(1) << __FUNCTION__;
}

// interfaces::ServiceFactory implementation.
void ServiceFactoryImpl::CreateRenderer(
    mojo::InterfaceRequest<interfaces::Renderer> request) {
  // The created object is owned by the pipe.
  // The audio and video sinks are owned by the client.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner(
      base::MessageLoop::current()->task_runner());
  AudioRendererSink* audio_renderer_sink =
      mojo_media_client_->CreateAudioRendererSink();
  VideoRendererSink* video_renderer_sink =
      mojo_media_client_->CreateVideoRendererSink(task_runner);

  RendererFactory* renderer_factory = GetRendererFactory();
  if (!renderer_factory)
    return;

  scoped_ptr<Renderer> renderer = renderer_factory->CreateRenderer(
      task_runner, task_runner, audio_renderer_sink, video_renderer_sink);
  if (!renderer) {
    LOG(ERROR) << "Renderer creation failed.";
    return;
  }

  new MojoRendererService(cdm_service_context_.GetWeakPtr(),
                          std::move(renderer), std::move(request));
}

void ServiceFactoryImpl::CreateCdm(
    mojo::InterfaceRequest<interfaces::ContentDecryptionModule> request) {
  CdmFactory* cdm_factory = GetCdmFactory();
  if (!cdm_factory)
    return;

  // The created object is owned by the pipe.
  new MojoCdmService(cdm_service_context_.GetWeakPtr(), cdm_factory,
                     std::move(request));
}

RendererFactory* ServiceFactoryImpl::GetRendererFactory() {
  if (!renderer_factory_) {
    renderer_factory_ = mojo_media_client_->CreateRendererFactory(media_log_);
    LOG_IF(ERROR, !renderer_factory_) << "RendererFactory not available.";
  }
  return renderer_factory_.get();
}

CdmFactory* ServiceFactoryImpl::GetCdmFactory() {
  if (!cdm_factory_) {
    cdm_factory_ = mojo_media_client_->CreateCdmFactory(service_provider_);
    LOG_IF(ERROR, !cdm_factory_) << "CdmFactory not available.";
  }
  return cdm_factory_.get();
}

}  // namespace media
