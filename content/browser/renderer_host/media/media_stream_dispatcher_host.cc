// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/media_stream_dispatcher_host.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/task_runner_util.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "url/origin.h"

namespace content {

namespace {

void BindMediaStreamDispatcherRequest(
    int render_process_id,
    int render_frame_id,
    mojom::MediaStreamDispatcherRequest request) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RenderFrameHost* render_frame_host =
      RenderFrameHost::FromID(render_process_id, render_frame_id);
  if (render_frame_host)
    render_frame_host->GetRemoteInterfaces()->GetInterface(std::move(request));
}

}  // namespace

MediaStreamDispatcherHost::MediaStreamDispatcherHost(
    int render_process_id,
    MediaStreamManager* media_stream_manager)
    : render_process_id_(render_process_id),
      media_stream_manager_(media_stream_manager),
      salt_and_origin_callback_(
          base::BindRepeating(&GetMediaDeviceSaltAndOrigin)),
      weak_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  bindings_.set_connection_error_handler(
      base::Bind(&MediaStreamDispatcherHost::CancelAllRequests,
                 weak_factory_.GetWeakPtr()));
}

MediaStreamDispatcherHost::~MediaStreamDispatcherHost() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  bindings_.CloseAllBindings();
  CancelAllRequests();
}

void MediaStreamDispatcherHost::BindRequest(
    mojom::MediaStreamDispatcherHostRequest request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  bindings_.AddBinding(this, std::move(request));
}

void MediaStreamDispatcherHost::StreamGenerated(
    int render_frame_id,
    int page_request_id,
    const std::string& label,
    const MediaStreamDevices& audio_devices,
    const MediaStreamDevices& video_devices) {
  DVLOG(1) << __func__ << " label= " << label;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  GetMediaStreamDispatcherForFrame(render_frame_id)
      ->OnStreamGenerated(page_request_id, label, audio_devices, video_devices);
}

void MediaStreamDispatcherHost::StreamGenerationFailed(
    int render_frame_id,
    int page_request_id,
    MediaStreamRequestResult result) {
  DVLOG(1) << __func__ << " page_request_id=" << page_request_id
           << " result=" << result;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  GetMediaStreamDispatcherForFrame(render_frame_id)
      ->OnStreamGenerationFailed(page_request_id, result);
}

void MediaStreamDispatcherHost::DeviceStopped(int render_frame_id,
                                              const std::string& label,
                                              const MediaStreamDevice& device) {
  DVLOG(1) << __func__ << " label=" << label << " type=" << device.type
           << " device_id=" << device.id;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  GetMediaStreamDispatcherForFrame(render_frame_id)
      ->OnDeviceStopped(label, device);
}

void MediaStreamDispatcherHost::DeviceOpened(int render_frame_id,
                                             int page_request_id,
                                             const std::string& label,
                                             const MediaStreamDevice& device) {
  DVLOG(1) << __func__ << " page_request_id=" << page_request_id;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  GetMediaStreamDispatcherForFrame(render_frame_id)
      ->OnDeviceOpened(page_request_id, label, device);
}

mojom::MediaStreamDispatcher*
MediaStreamDispatcherHost::GetMediaStreamDispatcherForFrame(
    int render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto it = dispatchers_.find(render_frame_id);
  if (it != dispatchers_.end())
    return it->second.get();

  mojom::MediaStreamDispatcherPtr dispatcher;
  auto dispatcher_request = mojo::MakeRequest(&dispatcher);
  dispatcher.set_connection_error_handler(base::BindOnce(
      &MediaStreamDispatcherHost::OnMediaStreamDispatcherConnectionError,
      weak_factory_.GetWeakPtr(), render_frame_id));
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&BindMediaStreamDispatcherRequest, render_process_id_,
                     render_frame_id, std::move(dispatcher_request)));
  dispatchers_[render_frame_id] = std::move(dispatcher);

  return dispatchers_[render_frame_id].get();
}

void MediaStreamDispatcherHost::OnMediaStreamDispatcherConnectionError(
    int render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  dispatchers_.erase(render_frame_id);
}

void MediaStreamDispatcherHost::CancelAllRequests() {
  if (!bindings_.empty())
    return;

  media_stream_manager_->CancelAllRequests(render_process_id_);
}

void MediaStreamDispatcherHost::DeviceOpenFailed(int render_frame_id,
                                                 int page_request_id) {
  DVLOG(1) << __func__ << " page_request_id=" << page_request_id;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  GetMediaStreamDispatcherForFrame(render_frame_id)
      ->OnDeviceOpenFailed(page_request_id);
}

void MediaStreamDispatcherHost::GenerateStream(
    int32_t render_frame_id,
    int32_t page_request_id,
    const StreamControls& controls,
    bool user_gesture) {
  DVLOG(1) << __func__ << " render_frame_id=" << render_frame_id
           << " page_request_id=" << page_request_id
           << " audio=" << controls.audio.requested
           << " video=" << controls.video.requested
           << " user_gesture=" << user_gesture;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  base::PostTaskAndReplyWithResult(
      BrowserThread::GetTaskRunnerForThread(BrowserThread::UI).get(), FROM_HERE,
      base::BindOnce(salt_and_origin_callback_, render_process_id_,
                     render_frame_id),
      base::BindOnce(&MediaStreamDispatcherHost::DoGenerateStream,
                     weak_factory_.GetWeakPtr(), render_frame_id,
                     page_request_id, controls, user_gesture));
}

void MediaStreamDispatcherHost::DoGenerateStream(
    int32_t render_frame_id,
    int32_t page_request_id,
    const StreamControls& controls,
    bool user_gesture,
    const std::pair<std::string, url::Origin>& salt_and_origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!MediaStreamManager::IsOriginAllowed(render_process_id_,
                                           salt_and_origin.second)) {
    StreamGenerationFailed(render_frame_id, page_request_id,
                           MEDIA_DEVICE_INVALID_SECURITY_ORIGIN_DEPRECATED);
    return;
  }

  media_stream_manager_->GenerateStream(
      weak_factory_.GetWeakPtr(), render_process_id_, render_frame_id,
      salt_and_origin.first, page_request_id, controls, salt_and_origin.second,
      user_gesture);
}

void MediaStreamDispatcherHost::CancelGenerateStream(int render_frame_id,
                                                     int page_request_id) {
  DVLOG(1) << __func__ << " render_frame_id=" << render_frame_id
           << " page_request_id=" << page_request_id;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  media_stream_manager_->CancelRequest(render_process_id_, render_frame_id,
                                       page_request_id);
}

void MediaStreamDispatcherHost::StopStreamDevice(int32_t render_frame_id,
                                                 const std::string& device_id) {
  DVLOG(1) << __func__ << " render_frame_id=" << render_frame_id
           << " device_id=" << device_id;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  media_stream_manager_->StopStreamDevice(render_process_id_, render_frame_id,
                                          device_id);
}

void MediaStreamDispatcherHost::OpenDevice(int32_t render_frame_id,
                                           int32_t page_request_id,
                                           const std::string& device_id,
                                           MediaStreamType type) {
  DVLOG(1) << __func__ << " render_frame_id=" << render_frame_id
           << " page_request_id=" << page_request_id
           << " device_id=" << device_id << " type=" << type;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  base::PostTaskAndReplyWithResult(
      BrowserThread::GetTaskRunnerForThread(BrowserThread::UI).get(), FROM_HERE,
      base::BindOnce(salt_and_origin_callback_, render_process_id_,
                     render_frame_id),
      base::BindOnce(&MediaStreamDispatcherHost::DoOpenDevice,
                     weak_factory_.GetWeakPtr(), render_frame_id,
                     page_request_id, device_id, type));
}

void MediaStreamDispatcherHost::DoOpenDevice(
    int32_t render_frame_id,
    int32_t page_request_id,
    const std::string& device_id,
    MediaStreamType type,
    const std::pair<std::string, url::Origin>& salt_and_origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!MediaStreamManager::IsOriginAllowed(render_process_id_,
                                           salt_and_origin.second)) {
    DeviceOpenFailed(render_frame_id, page_request_id);
    return;
  }

  media_stream_manager_->OpenDevice(weak_factory_.GetWeakPtr(),
                                    render_process_id_, render_frame_id,
                                    salt_and_origin.first, page_request_id,
                                    device_id, type, salt_and_origin.second);
}

void MediaStreamDispatcherHost::CloseDevice(const std::string& label) {
  DVLOG(1) << __func__ << " label= " << label;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  media_stream_manager_->CancelRequest(label);
}

void MediaStreamDispatcherHost::SetCapturingLinkSecured(int32_t session_id,
                                                        MediaStreamType type,
                                                        bool is_secure) {
  DVLOG(1) << __func__ << " session_id=" << session_id << " type=" << type
           << " is_secure=" << is_secure;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  media_stream_manager_->SetCapturingLinkSecured(render_process_id_, session_id,
                                                 type, is_secure);
}

void MediaStreamDispatcherHost::StreamStarted(const std::string& label) {
  DVLOG(1) << __func__ << " label= " << label;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  media_stream_manager_->OnStreamStarted(label);
}

}  // namespace content
