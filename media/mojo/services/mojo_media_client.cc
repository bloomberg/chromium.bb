// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_media_client.h"

namespace media {

MojoMediaClient::MojoMediaClient() {}

MojoMediaClient::~MojoMediaClient() {}

void MojoMediaClient::Initialize() {}

scoped_ptr<RendererFactory> MojoMediaClient::CreateRendererFactory(
    const scoped_refptr<MediaLog>& media_log) {
  return nullptr;
}

AudioRendererSink* MojoMediaClient::CreateAudioRendererSink() {
  return nullptr;
}

VideoRendererSink* MojoMediaClient::CreateVideoRendererSink(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) {
  return nullptr;
}

scoped_ptr<CdmFactory> MojoMediaClient::CreateCdmFactory(
    mojo::shell::mojom::InterfaceProvider* service_provider) {
  return nullptr;
}

}  // namespace media
