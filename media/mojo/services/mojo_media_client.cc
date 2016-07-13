// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_media_client.h"

#include "base/single_thread_task_runner.h"
#include "media/base/audio_decoder.h"
#include "media/base/cdm_factory.h"
#include "media/base/media_log.h"
#include "media/base/renderer.h"
#include "media/base/video_decoder.h"

namespace media {

MojoMediaClient::MojoMediaClient() {}

MojoMediaClient::~MojoMediaClient() {}

void MojoMediaClient::Initialize() {}

void MojoMediaClient::WillQuit() {}

std::unique_ptr<AudioDecoder> MojoMediaClient::CreateAudioDecoder(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  return nullptr;
}

std::unique_ptr<VideoDecoder> MojoMediaClient::CreateVideoDecoder(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  return nullptr;
}

std::unique_ptr<Renderer> MojoMediaClient::CreateRenderer(
    scoped_refptr<base::SingleThreadTaskRunner> media_task_runner,
    scoped_refptr<MediaLog> media_log,
    const std::string& audio_device_id) {
  return nullptr;
}

std::unique_ptr<CdmFactory> MojoMediaClient::CreateCdmFactory(
    shell::mojom::InterfaceProvider* interface_provider) {
  return nullptr;
}

}  // namespace media
