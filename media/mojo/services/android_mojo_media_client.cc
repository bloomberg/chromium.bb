// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/android_mojo_media_client.h"

#include "media/base/android/android_cdm_factory.h"
#include "media/mojo/interfaces/provision_fetcher.mojom.h"
#include "media/mojo/services/mojo_provision_fetcher.h"
#include "mojo/shell/public/cpp/connect.h"

namespace media {

namespace {

scoped_ptr<ProvisionFetcher> CreateProvisionFetcher(
    mojo::shell::mojom::InterfaceProvider* service_provider) {
  interfaces::ProvisionFetcherPtr provision_fetcher_ptr;
  mojo::GetInterface(service_provider, &provision_fetcher_ptr);
  return make_scoped_ptr(
      new MojoProvisionFetcher(std::move(provision_fetcher_ptr)));
}

}  // namespace

AndroidMojoMediaClient::AndroidMojoMediaClient() {}

AndroidMojoMediaClient::~AndroidMojoMediaClient() {}

// MojoMediaClient overrides.
scoped_ptr<CdmFactory> AndroidMojoMediaClient::CreateCdmFactory(
    mojo::shell::mojom::InterfaceProvider* service_provider) {
  return make_scoped_ptr(new AndroidCdmFactory(
      base::Bind(&CreateProvisionFetcher, service_provider)));
}

}  // namespace media
