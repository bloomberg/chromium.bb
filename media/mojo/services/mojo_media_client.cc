// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_media_client.h"

#include "base/single_thread_task_runner.h"
#include "media/base/audio_decoder.h"
#include "media/base/audio_renderer_sink.h"
#include "media/base/cdm_factory.h"
#include "media/base/media_log.h"
#include "media/base/renderer_factory.h"
#include "media/base/video_decoder.h"
#include "media/base/video_renderer_sink.h"

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
#include "media/cdm/cdm_proxy.h"
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

namespace media {

MojoMediaClient::MojoMediaClient() = default;

MojoMediaClient::~MojoMediaClient() = default;

void MojoMediaClient::Initialize(service_manager::Connector* connector) {}

void MojoMediaClient::EnsureSandboxed() {}

std::unique_ptr<AudioDecoder> MojoMediaClient::CreateAudioDecoder(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  return nullptr;
}

std::unique_ptr<VideoDecoder> MojoMediaClient::CreateVideoDecoder(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    MediaLog* media_log,
    mojom::CommandBufferIdPtr command_buffer_id,
    RequestOverlayInfoCB request_overlay_info_cb) {
  return nullptr;
}

scoped_refptr<AudioRendererSink> MojoMediaClient::CreateAudioRendererSink(
    const std::string& audio_device_id) {
  return nullptr;
}

std::unique_ptr<VideoRendererSink> MojoMediaClient::CreateVideoRendererSink(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) {
  return nullptr;
}

std::unique_ptr<RendererFactory> MojoMediaClient::CreateRendererFactory(
    MediaLog* media_log) {
  return nullptr;
}

std::unique_ptr<CdmFactory> MojoMediaClient::CreateCdmFactory(
    service_manager::mojom::InterfaceProvider* host_interfaces) {
  return nullptr;
}

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
std::unique_ptr<CdmProxy> MojoMediaClient::CreateCdmProxy(
    const std::string& cdm_guid) {
  return nullptr;
}
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

#if BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
void MojoMediaClient::AddCdmHostFilePaths(
    std::vector<media::CdmHostFilePath>* /* cdm_host_file_paths */) {}
#endif  // BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)

}  // namespace media
