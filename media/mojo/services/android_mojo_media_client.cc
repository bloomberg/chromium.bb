// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_media_client.h"

#include "base/memory/scoped_ptr.h"
#include "media/base/android/android_cdm_factory.h"
#include "media/base/media.h"

namespace media {
namespace internal {

class AndroidMojoMediaClient : public PlatformMojoMediaClient {
 public:
  AndroidMojoMediaClient() {}

  scoped_ptr<CdmFactory> CreateCdmFactory() override {
    return make_scoped_ptr(new AndroidCdmFactory());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AndroidMojoMediaClient);
};

scoped_ptr<PlatformMojoMediaClient> CreatePlatformMojoMediaClient() {
  return make_scoped_ptr(new AndroidMojoMediaClient());
}

}  // namespace internal
}  // namespace media
