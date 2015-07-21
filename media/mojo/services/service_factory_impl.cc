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

namespace media {

ServiceFactoryImpl::ServiceFactoryImpl(
    mojo::InterfaceRequest<interfaces::ServiceFactory> request,
    mojo::ServiceProvider* service_provider,
    scoped_refptr<MediaLog> media_log)
    : binding_(this, request.Pass()),
      service_provider_(service_provider),
      media_log_(media_log) {
  DVLOG(1) << __FUNCTION__;
}

ServiceFactoryImpl::~ServiceFactoryImpl() {
  DVLOG(1) << __FUNCTION__;
}

// interfaces::ServiceFactory implementation.
void ServiceFactoryImpl::CreateRenderer(
    mojo::InterfaceRequest<interfaces::MediaRenderer> request) {
  // The created object is owned by the pipe.
  new MojoRendererService(cdm_service_context_.GetWeakPtr(),
                          GetRendererFactory(), media_log_, request.Pass());
}

void ServiceFactoryImpl::CreateCdm(
    mojo::InterfaceRequest<interfaces::ContentDecryptionModule> request) {
  // The created object is owned by the pipe.
  new MojoCdmService(cdm_service_context_.GetWeakPtr(), service_provider_,
                     GetCdmFactory(), request.Pass());
}

RendererFactory* ServiceFactoryImpl::GetRendererFactory() {
  if (!renderer_factory_)
    renderer_factory_ =
        MojoMediaClient::Get()->CreateRendererFactory(media_log_);
  return renderer_factory_.get();
}

CdmFactory* ServiceFactoryImpl::GetCdmFactory() {
  if (!cdm_factory_)
    cdm_factory_ = MojoMediaClient::Get()->CreateCdmFactory();
  return cdm_factory_.get();
}

}  // namespace media
