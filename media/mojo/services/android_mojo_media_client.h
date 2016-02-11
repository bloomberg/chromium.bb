// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_ANDROID_MOJO_MEDIA_CLIENT_H_
#define MEDIA_MOJO_SERVICES_ANDROID_MOJO_MEDIA_CLIENT_H_

#include "base/macros.h"
#include "media/base/media_export.h"
#include "media/mojo/services/mojo_media_client.h"

namespace media {

class MEDIA_EXPORT AndroidMojoMediaClient : public MojoMediaClient {
 public:
  AndroidMojoMediaClient();
  ~AndroidMojoMediaClient() final;

  // MojoMediaClient implementation.
  scoped_ptr<CdmFactory> CreateCdmFactory(
      mojo::shell::mojom::InterfaceProvider* service_provider) final;

 private:
  DISALLOW_COPY_AND_ASSIGN(AndroidMojoMediaClient);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_ANDROID_MOJO_MEDIA_CLIENT_H_
