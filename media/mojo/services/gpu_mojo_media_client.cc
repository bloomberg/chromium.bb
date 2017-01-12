// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/gpu_mojo_media_client.h"

#include "media/base/audio_decoder.h"
#include "media/base/cdm_factory.h"
#include "media/base/video_decoder.h"

#if defined(OS_ANDROID)
#include "base/memory/ptr_util.h"
#include "media/base/android/android_cdm_factory.h"
#include "media/filters/android/media_codec_audio_decoder.h"
#include "media/mojo/interfaces/provision_fetcher.mojom.h"
#include "media/mojo/services/mojo_provision_fetcher.h"
#include "services/service_manager/public/cpp/connect.h"
#endif  // defined(OS_ANDROID)

namespace media {

namespace {

#if defined(OS_ANDROID)
std::unique_ptr<ProvisionFetcher> CreateProvisionFetcher(
    service_manager::mojom::InterfaceProvider* interface_provider) {
  mojom::ProvisionFetcherPtr provision_fetcher_ptr;
  service_manager::GetInterface(interface_provider, &provision_fetcher_ptr);
  return base::MakeUnique<MojoProvisionFetcher>(
      std::move(provision_fetcher_ptr));
}
#endif  // defined(OS_ANDROID)

}  // namespace

GpuMojoMediaClient::GpuMojoMediaClient(
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
    base::WeakPtr<MediaGpuChannelManager> media_gpu_channel_manager)
    : gpu_task_runner_(std::move(gpu_task_runner)),
      media_gpu_channel_manager_(std::move(media_gpu_channel_manager)) {}

GpuMojoMediaClient::~GpuMojoMediaClient() {}

std::unique_ptr<AudioDecoder> GpuMojoMediaClient::CreateAudioDecoder(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
#if defined(OS_ANDROID)
  return base::MakeUnique<MediaCodecAudioDecoder>(task_runner);
#else
  return nullptr;
#endif  // defined(OS_ANDROID)
}

std::unique_ptr<VideoDecoder> GpuMojoMediaClient::CreateVideoDecoder(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    mojom::CommandBufferIdPtr command_buffer_id) {
  static_cast<void>(media_gpu_channel_manager_);

  // TODO(sandersd): Factory for VideoDecoders.
  return nullptr;
}

std::unique_ptr<CdmFactory> GpuMojoMediaClient::CreateCdmFactory(
    service_manager::mojom::InterfaceProvider* interface_provider) {
#if defined(OS_ANDROID)
  return base::MakeUnique<AndroidCdmFactory>(
      base::Bind(&CreateProvisionFetcher, interface_provider));
#else
  return nullptr;
#endif  // defined(OS_ANDROID)
}

}  // namespace media
