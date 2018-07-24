// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_ANDROID_MOJO_MEDIA_CLIENT_H_
#define MEDIA_MOJO_SERVICES_ANDROID_MOJO_MEDIA_CLIENT_H_

#include <memory>

#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "media/mojo/services/mojo_media_client.h"

namespace media {

class AndroidMojoMediaClient : public MojoMediaClient {
 public:
  AndroidMojoMediaClient();
  ~AndroidMojoMediaClient() final;

  // MojoMediaClient implementation.
  std::unique_ptr<AudioDecoder> CreateAudioDecoder(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      MediaLog* media_log) final;

  std::unique_ptr<CdmFactory> CreateCdmFactory(
      service_manager::mojom::InterfaceProvider* host_interfaces) final;

 private:
  DISALLOW_COPY_AND_ASSIGN(AndroidMojoMediaClient);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_ANDROID_MOJO_MEDIA_CLIENT_H_
