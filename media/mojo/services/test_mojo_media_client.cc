// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/test_mojo_media_client.h"

#include <memory>

#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_output_stream_sink.h"
#include "media/audio/audio_thread_impl.h"
#include "media/base/cdm_factory.h"
#include "media/base/media.h"
#include "media/base/media_log.h"
#include "media/base/null_video_sink.h"
#include "media/base/renderer_factory.h"
#include "media/cdm/default_cdm_factory.h"
#include "media/renderers/default_renderer_factory.h"
#include "media/video/gpu_video_accelerator_factories.h"

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
#include "media/cdm/cdm_paths.h"  // nogncheck
#include "media/cdm/cdm_proxy.h"  // nogncheck
#include "media/cdm/library_cdm/clear_key_cdm/clear_key_cdm_proxy.h"  // nogncheck
#endif

namespace media {

TestMojoMediaClient::TestMojoMediaClient() = default;

TestMojoMediaClient::~TestMojoMediaClient() {
  DVLOG(1) << __func__;

  if (audio_manager_) {
    audio_manager_->Shutdown();
    audio_manager_.reset();
  }
}

void TestMojoMediaClient::Initialize(
    service_manager::Connector* /* connector */) {
  InitializeMediaLibrary();
  // TODO(dalecurtis): We should find a single owner per process for the audio
  // manager or make it a lazy instance.  It's not safe to call Get()/Create()
  // across multiple threads...
  AudioManager* audio_manager = AudioManager::Get();
  if (!audio_manager) {
    audio_manager_ = media::AudioManager::CreateForTesting(
        std::make_unique<AudioThreadImpl>());
    // Flush the message loop to ensure that the audio manager is initialized.
    base::RunLoop().RunUntilIdle();
  }
}

scoped_refptr<AudioRendererSink> TestMojoMediaClient::CreateAudioRendererSink(
    const std::string& /* audio_device_id */) {
  return new AudioOutputStreamSink();
}

std::unique_ptr<VideoRendererSink> TestMojoMediaClient::CreateVideoRendererSink(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) {
  return std::make_unique<NullVideoSink>(
      false, base::TimeDelta::FromSecondsD(1.0 / 60),
      NullVideoSink::NewFrameCB(), task_runner);
}

std::unique_ptr<RendererFactory> TestMojoMediaClient::CreateRendererFactory(
    MediaLog* media_log) {
  return std::make_unique<DefaultRendererFactory>(
      media_log, nullptr, DefaultRendererFactory::GetGpuFactoriesCB());
}

std::unique_ptr<CdmFactory> TestMojoMediaClient::CreateCdmFactory(
    service_manager::mojom::InterfaceProvider* /* host_interfaces */) {
  DVLOG(1) << __func__;
  return std::make_unique<DefaultCdmFactory>();
}

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
std::unique_ptr<CdmProxy> TestMojoMediaClient::CreateCdmProxy(
    const std::string& cdm_guid) {
  DVLOG(1) << __func__ << ": cdm_guid = " << cdm_guid;
  if (cdm_guid == kClearKeyCdmGuid)
    return std::make_unique<ClearKeyCdmProxy>();

  return nullptr;
}
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

}  // namespace media
