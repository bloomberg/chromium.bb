// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/media/cast_mojo_media_client.h"

#include "base/memory/ptr_util.h"
#include "chromecast/browser/media/cast_renderer.h"
#include "media/base/cdm_factory.h"
#include "media/base/media_log.h"
#include "media/base/renderer_factory.h"

namespace chromecast {
namespace media {

CastMojoMediaClient::CastMojoMediaClient(
    const CreateMediaPipelineBackendCB& create_backend_cb,
    const CreateCdmFactoryCB& create_cdm_factory_cb)
    : create_backend_cb_(create_backend_cb),
      create_cdm_factory_cb_(create_cdm_factory_cb) {}

CastMojoMediaClient::~CastMojoMediaClient() {}

std::unique_ptr<::media::Renderer> CastMojoMediaClient::CreateRenderer(
    scoped_refptr<base::SingleThreadTaskRunner> media_task_runner,
    scoped_refptr<::media::MediaLog> media_log,
    const std::string& audio_device_id) {
  return base::MakeUnique<chromecast::media::CastRenderer>(
      create_backend_cb_, std::move(media_task_runner), audio_device_id);
}

std::unique_ptr<::media::CdmFactory> CastMojoMediaClient::CreateCdmFactory(
    ::shell::mojom::InterfaceProvider* interface_provider) {
  return create_cdm_factory_cb_.Run();
}

}  // namespace media
}  // namespace chromecast
