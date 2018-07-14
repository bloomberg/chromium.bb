// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/cast_mirroring_service_host.h"

#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/task_scheduler/post_task.h"
#include "components/mirroring/browser/single_client_video_capture_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/video_capture_device_launcher.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

using content::BrowserThread;

namespace mirroring {

namespace {

void CreateVideoCaptureHostOnIO(const std::string& device_id,
                                content::MediaStreamType type,
                                media::mojom::VideoCaptureHostRequest request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  scoped_refptr<base::SingleThreadTaskRunner> device_task_runner =
      base::CreateSingleThreadTaskRunnerWithTraits(
          {base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
          base::SingleThreadTaskRunnerThreadMode::DEDICATED);
  mojo::MakeStrongBinding(
      std::make_unique<SingleClientVideoCaptureHost>(
          device_id, type,
          base::BindRepeating(&content::VideoCaptureDeviceLauncher::
                                  CreateInProcessVideoCaptureDeviceLauncher,
                              std::move(device_task_runner))),
      std::move(request));
}

content::MediaStreamType ConvertVideoStreamType(
    content::DesktopMediaID::Type type) {
  switch (type) {
    case content::DesktopMediaID::TYPE_NONE:
      return content::MediaStreamType::MEDIA_NO_SERVICE;
    case content::DesktopMediaID::TYPE_WEB_CONTENTS:
      return content::MediaStreamType::MEDIA_TAB_VIDEO_CAPTURE;
    case content::DesktopMediaID::TYPE_SCREEN:
    case content::DesktopMediaID::TYPE_WINDOW:
      return content::MediaStreamType::MEDIA_DESKTOP_VIDEO_CAPTURE;
  }

  // To suppress compiler warning on Windows.
  return content::MediaStreamType::MEDIA_NO_SERVICE;
}

}  // namespace

CastMirroringServiceHost::CastMirroringServiceHost(
    content::DesktopMediaID source_media_id)
    : source_media_id_(source_media_id) {
  DCHECK(source_media_id_.type != content::DesktopMediaID::TYPE_NONE);
}

CastMirroringServiceHost::~CastMirroringServiceHost() {
  // TODO(xjz): Stop the mirroring if connected to the MirroringService.
  // Implementation will be added in a later CL.
}

void CastMirroringServiceHost::Start(int32_t session_id,
                                     const CastSinkInfo& sink_info,
                                     CastMessageChannel* outbound_channel,
                                     SessionObserver* observer) {
  // TODO(xjz): Connect to the Mirroring Service and start a mirroring session.
  // Implementation will be added in a later CL.
}

void CastMirroringServiceHost::GetVideoCaptureHost(
    media::mojom::VideoCaptureHostRequest request) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&CreateVideoCaptureHostOnIO, source_media_id_.ToString(),
                     ConvertVideoStreamType(source_media_id_.type),
                     std::move(request)));
}

void CastMirroringServiceHost::GetNetworkContext(
    network::mojom::NetworkContextRequest request) {
  // TODO(xjz): Implemenation will be added in a later CL.
}

void CastMirroringServiceHost::CreateAudioStream(
    AudioStreamCreatorClient* client,
    const media::AudioParameters& params,
    uint32_t total_segments) {
  // TODO(xjz): Implementation will be added in a later CL.
}

void CastMirroringServiceHost::ConnectToRemotingSource(
    media::mojom::RemoterPtr remoter,
    media::mojom::RemotingSourceRequest request) {
  // TODO(xjz): Implementation will be added in a later CL.
}

}  // namespace mirroring
