// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_media_application.h"

#include "media/mojo/services/mojo_media_client.h"

namespace {
class CastMojoMediaClient : public ::media::MojoMediaClient {
 public:
  CastMojoMediaClient() {}
  ~CastMojoMediaClient() override {}

  // MojoMediaClient overrides.
  void Initialize() override {}
  scoped_ptr<::media::RendererFactory> CreateRendererFactory(
      const scoped_refptr<::media::MediaLog>& media_log) override {
    return scoped_ptr<::media::RendererFactory>();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CastMojoMediaClient);
};
}  // namespace

namespace media {
// static
scoped_ptr<MojoMediaClient> MojoMediaClient::Create() {
  return make_scoped_ptr(new CastMojoMediaClient());
}
}  // namespace media
