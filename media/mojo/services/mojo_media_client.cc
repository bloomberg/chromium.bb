// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_media_client.h"

namespace media {

// PlatformMojoMediaClient default implementations.

PlatformMojoMediaClient::~PlatformMojoMediaClient(){};

void PlatformMojoMediaClient::Initialize() {}

scoped_ptr<RendererFactory> PlatformMojoMediaClient::CreateRendererFactory(
    const scoped_refptr<MediaLog>& media_log) {
  return nullptr;
};

scoped_refptr<AudioRendererSink>
PlatformMojoMediaClient::CreateAudioRendererSink() {
  return nullptr;
};

scoped_ptr<VideoRendererSink> PlatformMojoMediaClient::CreateVideoRendererSink(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) {
  return nullptr;
}

scoped_ptr<CdmFactory> PlatformMojoMediaClient::CreateCdmFactory(
    mojo::ServiceProvider* service_provider) {
  return nullptr;
}

namespace internal {
extern scoped_ptr<PlatformMojoMediaClient> CreatePlatformMojoMediaClient();
}  // namespace internal

// static
scoped_ptr<MojoMediaClient> MojoMediaClient::Create() {
  return make_scoped_ptr(
      new MojoMediaClient(internal::CreatePlatformMojoMediaClient()));
}

void MojoMediaClient::Initialize() {
  platform_client_->Initialize();
}

scoped_ptr<RendererFactory> MojoMediaClient::CreateRendererFactory(
    const scoped_refptr<MediaLog>& media_log) {
  return platform_client_->CreateRendererFactory(media_log);
}

scoped_refptr<AudioRendererSink> MojoMediaClient::CreateAudioRendererSink() {
  return platform_client_->CreateAudioRendererSink();
}

scoped_ptr<VideoRendererSink> MojoMediaClient::CreateVideoRendererSink(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) {
  return platform_client_->CreateVideoRendererSink(task_runner);
}

scoped_ptr<CdmFactory> MojoMediaClient::CreateCdmFactory(
    mojo::ServiceProvider* service_provider) {
  return platform_client_->CreateCdmFactory(service_provider);
}

MojoMediaClient::MojoMediaClient(
    scoped_ptr<PlatformMojoMediaClient> platform_client)
    : platform_client_(std::move(platform_client)) {
  DCHECK(platform_client_);
}

MojoMediaClient::~MojoMediaClient() {
}

}  // namespace media
