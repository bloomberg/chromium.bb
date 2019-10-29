// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/android_mojo_util.h"

#include "media/mojo/services/mojo_media_drm_storage.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/service_manager/public/cpp/connect.h"

namespace media {
namespace android_mojo_util {

std::unique_ptr<ProvisionFetcher> CreateProvisionFetcher(
    service_manager::mojom::InterfaceProvider* host_interfaces) {
  DCHECK(host_interfaces);
  mojo::PendingRemote<mojom::ProvisionFetcher> provision_fetcher_remote;
  service_manager::GetInterface(
      host_interfaces,
      provision_fetcher_remote.InitWithNewPipeAndPassReceiver());
  return std::make_unique<MojoProvisionFetcher>(
      std::move(provision_fetcher_remote));
}

std::unique_ptr<MediaDrmStorage> CreateMediaDrmStorage(
    service_manager::mojom::InterfaceProvider* host_interfaces) {
  DCHECK(host_interfaces);
  mojo::PendingRemote<mojom::MediaDrmStorage> media_drm_storage_remote;
  service_manager::GetInterface(
      host_interfaces,
      media_drm_storage_remote.InitWithNewPipeAndPassReceiver());
  return std::make_unique<MojoMediaDrmStorage>(
      std::move(media_drm_storage_remote));
}

}  // namespace android_mojo_util
}  // namespace media
