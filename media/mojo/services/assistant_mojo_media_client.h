// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_ASSISTANT_MOJO_MEDIA_CLIENT_H_
#define MEDIA_MOJO_SERVICES_ASSISTANT_MOJO_MEDIA_CLIENT_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "media/base/media_log.h"
#include "media/mojo/services/mojo_media_client.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace media {

class MediaLog;

class AssistantMojoMediaClient : public MojoMediaClient {
 public:
  AssistantMojoMediaClient();
  ~AssistantMojoMediaClient() final;

  // MojoMediaClient implementation.
  std::unique_ptr<AudioDecoder> CreateAudioDecoder(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      MediaLog* media_log) final;

 private:
  DISALLOW_COPY_AND_ASSIGN(AssistantMojoMediaClient);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_ASSISTANT_MOJO_MEDIA_CLIENT_H_
