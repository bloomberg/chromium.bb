// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/mojo/cast_mojo_media_client.h"

namespace chromecast {
namespace media {

CastMojoMediaClient::CastMojoMediaClient() {}

CastMojoMediaClient::~CastMojoMediaClient() {}

// static
scoped_ptr<::media::MojoMediaClient> CastMojoMediaClient::Create() {
  return scoped_ptr<::media::MojoMediaClient>(new CastMojoMediaClient());
}

void CastMojoMediaClient::Initialize() {}

scoped_ptr<::media::RendererFactory> CastMojoMediaClient::CreateRendererFactory(
    const scoped_refptr<::media::MediaLog>& /* media_log */) {
  return scoped_ptr<::media::RendererFactory>();
}

}  // namespace media
}  // namespace chromecast
