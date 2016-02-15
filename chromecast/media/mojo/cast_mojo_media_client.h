// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_MOJO_CAST_MOJO_MEDIA_CLIENT_H_
#define CHROMECAST_MEDIA_MOJO_CAST_MOJO_MEDIA_CLIENT_H_

#include "media/mojo/services/mojo_media_client.h"

namespace chromecast {
namespace media {

class CastMojoMediaClient : public ::media::MojoMediaClient {
 public:
  CastMojoMediaClient();
  ~CastMojoMediaClient() override;

  static scoped_ptr<::media::MojoMediaClient> Create();

  // MojoMediaClient overrides.
  void Initialize() override;
  scoped_ptr<::media::RendererFactory> CreateRendererFactory(
      const scoped_refptr<::media::MediaLog>& media_log) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CastMojoMediaClient);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_MOJO_CAST_MOJO_MEDIA_CLIENT_H_
