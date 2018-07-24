// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/android_mojo_media_client.h"

#include <utility>

#include <memory>

#include "media/base/android/android_cdm_factory.h"
#include "media/base/audio_decoder.h"
#include "media/base/cdm_factory.h"
#include "media/filters/android/media_codec_audio_decoder.h"
#include "media/mojo/interfaces/media_drm_storage.mojom.h"
#include "media/mojo/interfaces/provision_fetcher.mojom.h"
#include "media/mojo/services/mojo_media_drm_storage.h"
#include "media/mojo/services/mojo_provision_fetcher.h"
#include "services/service_manager/public/cpp/connect.h"

namespace media {

namespace {

std::unique_ptr<ProvisionFetcher> CreateProvisionFetcher(
    service_manager::mojom::InterfaceProvider* host_interfaces) {
  DCHECK(host_interfaces);
  mojom::ProvisionFetcherPtr provision_fetcher_ptr;
  service_manager::GetInterface(host_interfaces, &provision_fetcher_ptr);
  return std::make_unique<MojoProvisionFetcher>(
      std::move(provision_fetcher_ptr));
}

std::unique_ptr<MediaDrmStorage> CreateMediaDrmStorage(
    service_manager::mojom::InterfaceProvider* host_interfaces) {
  DCHECK(host_interfaces);
  mojom::MediaDrmStoragePtr media_drm_storage_ptr;
  service_manager::GetInterface(host_interfaces, &media_drm_storage_ptr);
  return std::make_unique<MojoMediaDrmStorage>(
      std::move(media_drm_storage_ptr));
}

}  // namespace

AndroidMojoMediaClient::AndroidMojoMediaClient() {}

AndroidMojoMediaClient::~AndroidMojoMediaClient() {}

// MojoMediaClient overrides.

std::unique_ptr<AudioDecoder> AndroidMojoMediaClient::CreateAudioDecoder(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    MediaLog* media_log) {
  return std::make_unique<MediaCodecAudioDecoder>(task_runner);
}

std::unique_ptr<CdmFactory> AndroidMojoMediaClient::CreateCdmFactory(
    service_manager::mojom::InterfaceProvider* host_interfaces) {
  if (!host_interfaces) {
    NOTREACHED() << "Host interfaces should be provided when using CDM with "
                 << "AndroidMojoMediaClient";
    return nullptr;
  }

  return std::make_unique<AndroidCdmFactory>(
      base::Bind(&CreateProvisionFetcher, host_interfaces),
      base::Bind(&CreateMediaDrmStorage, host_interfaces));
}

}  // namespace media
