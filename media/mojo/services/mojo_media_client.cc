// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_media_client.h"

namespace media {

namespace internal {
extern scoped_ptr<PlatformMojoMediaClient> CreatePlatformMojoMediaClient();
}  // namespace internal

static base::LazyInstance<MojoMediaClient>::Leaky g_mojo_media_client =
    LAZY_INSTANCE_INITIALIZER;

// static
MojoMediaClient* MojoMediaClient::Get() {
  return g_mojo_media_client.Pointer();
}

scoped_ptr<RendererFactory> MojoMediaClient::CreateRendererFactory(
    const scoped_refptr<MediaLog>& media_log) {
  return mojo_media_client_->CreateRendererFactory(media_log);
}

ScopedVector<AudioDecoder> MojoMediaClient::CreateAudioDecoders(
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    const scoped_refptr<MediaLog>& media_log) {
  return mojo_media_client_->CreateAudioDecoders(media_task_runner, media_log);
}

ScopedVector<VideoDecoder> MojoMediaClient::CreateVideoDecoders(
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    const scoped_refptr<MediaLog>& media_log) {
  return mojo_media_client_->CreateVideoDecoders(media_task_runner, media_log);
}

scoped_refptr<AudioRendererSink> MojoMediaClient::CreateAudioRendererSink() {
  return mojo_media_client_->CreateAudioRendererSink();
}

scoped_ptr<VideoRendererSink> MojoMediaClient::CreateVideoRendererSink(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) {
  return mojo_media_client_->CreateVideoRendererSink(task_runner);
}

const AudioHardwareConfig& MojoMediaClient::GetAudioHardwareConfig() {
  return mojo_media_client_->GetAudioHardwareConfig();
}

scoped_ptr<CdmFactory> MojoMediaClient::CreateCdmFactory() {
  return mojo_media_client_->CreateCdmFactory();
}

MojoMediaClient::MojoMediaClient()
    : mojo_media_client_(internal::CreatePlatformMojoMediaClient().Pass()) {
}

MojoMediaClient::~MojoMediaClient() {
}

}  // namespace media
