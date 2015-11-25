// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_media_client.h"

#include "base/memory/scoped_ptr.h"
#include "media/base/android/android_cdm_factory.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/media.h"

namespace media {
namespace internal {
namespace {

// A temporary solution until we pass the proper CDM provision fetcher.
class DummyProvisionFetcher : public ProvisionFetcher {
 public:
  DummyProvisionFetcher() {}
  ~DummyProvisionFetcher() final {}

  // Retrieve() always fails for this fetcher.
  void Retrieve(const std::string& default_url,
                const std::string& request_data,
                const ResponseCB& response_cb) final {
    BindToCurrentLoop(response_cb).Run(false, "");
  };
};

scoped_ptr<ProvisionFetcher> CreateDummyProvisionFetcher() {
  return make_scoped_ptr(new DummyProvisionFetcher());
}

}  // namespace (anonymous)

class AndroidMojoMediaClient : public PlatformMojoMediaClient {
 public:
  AndroidMojoMediaClient() {}

  scoped_ptr<CdmFactory> CreateCdmFactory() override {
    return make_scoped_ptr(
        new AndroidCdmFactory(base::Bind(&CreateDummyProvisionFetcher)));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AndroidMojoMediaClient);
};

scoped_ptr<PlatformMojoMediaClient> CreatePlatformMojoMediaClient() {
  return make_scoped_ptr(new AndroidMojoMediaClient());
}

}  // namespace internal
}  // namespace media
