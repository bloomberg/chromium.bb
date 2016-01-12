// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_media_client.h"

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/android/android_cdm_factory.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/media.h"
#include "media/mojo/interfaces/provision_fetcher.mojom.h"
#include "media/mojo/services/mojo_provision_fetcher.h"
#include "mojo/shell/public/cpp/connect.h"

namespace media {

namespace {
scoped_ptr<ProvisionFetcher> CreateProvisionFetcher(
    mojo::ServiceProvider* service_provider) {
  interfaces::ProvisionFetcherPtr provision_fetcher_ptr;
  mojo::ConnectToService(service_provider, &provision_fetcher_ptr);
  return make_scoped_ptr(
      new MojoProvisionFetcher(std::move(provision_fetcher_ptr)));
}

class AndroidMojoMediaClient : public MojoMediaClient {
 public:
  AndroidMojoMediaClient() {}

  // MojoMediaClient overrides.
  scoped_ptr<CdmFactory> CreateCdmFactory(
      mojo::ServiceProvider* service_provider) override {
    return make_scoped_ptr(new AndroidCdmFactory(
        base::Bind(&CreateProvisionFetcher, service_provider)));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AndroidMojoMediaClient);
};
}  // namespace (anonymous)

scoped_ptr<MojoMediaClient> MojoMediaClient::Create() {
  return make_scoped_ptr(new AndroidMojoMediaClient());
}

}  // namespace media
