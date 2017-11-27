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

void BindMediaStreamDeviceObserverRequest(
    int render_process_id,
    int render_frame_id,
    mojom::MediaStreamDeviceObserverRequest request) {
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

void MediaStreamDispatcherHost::OnDeviceStopped(
    int render_frame_id,
    const std::string& label,
    const MediaStreamDevice& device) {
  DVLOG(1) << __func__ << " label=" << label << " type=" << device.type
           << " device_id=" << device.id;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  GetMediaStreamDeviceObserverForFrame(render_frame_id)
      ->OnDeviceStopped(label, device);
}

mojom::MediaStreamDeviceObserver*
MediaStreamDispatcherHost::GetMediaStreamDeviceObserverForFrame(
    int render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto it = observers_.find(render_frame_id);
  if (it != observers_.end())
    return it->second.get();

  mojom::MediaStreamDeviceObserverPtr observer;
  auto dispatcher_request = mojo::MakeRequest(&observer);
  observer.set_connection_error_handler(base::BindOnce(
      &MediaStreamDispatcherHost::OnMediaStreamDeviceObserverConnectionError,
      weak_factory_.GetWeakPtr(), render_frame_id));
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&BindMediaStreamDeviceObserverRequest, render_process_id_,
                     render_frame_id, std::move(dispatcher_request)));
  observers_[render_frame_id] = std::move(observer);

  return observers_[render_frame_id].get();
}

void MediaStreamDispatcherHost::OnMediaStreamDeviceObserverConnectionError(
    int render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  observers_.erase(render_frame_id);
}

void MediaStreamDispatcherHost::CancelAllRequests() {
  if (!bindings_.empty())
    return;

  media_stream_manager_->CancelAllRequests(render_process_id_);
}

void MediaStreamDispatcherHost::GenerateStream(
    int32_t render_frame_id,
    int32_t page_request_id,
    const StreamControls& controls,
    bool user_gesture,
    GenerateStreamCallback callback) {
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
                     page_request_id, controls, user_gesture,
                     base::Passed(&callback)));
}

void MediaStreamDispatcherHost::DoGenerateStream(
    int32_t render_frame_id,
    int32_t page_request_id,
    const StreamControls& controls,
    bool user_gesture,
    GenerateStreamCallback callback,
    const std::pair<std::string, url::Origin>& salt_and_origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!MediaStreamManager::IsOriginAllowed(render_process_id_,
                                           salt_and_origin.second)) {
    std::move(callback).Run(MEDIA_DEVICE_INVALID_SECURITY_ORIGIN_DEPRECATED,
                            std::string(), MediaStreamDevices(),
                            MediaStreamDevices());
    return;
  }

  media_stream_manager_->GenerateStream(
      render_process_id_, render_frame_id, salt_and_origin.first,
      page_request_id, controls, salt_and_origin.second, user_gesture,
      std::move(callback),
      base::BindRepeating(&MediaStreamDispatcherHost::OnDeviceStopped,
                          weak_factory_.GetWeakPtr()));
}

void MediaStreamDispatcherHost::CancelRequest(int render_frame_id,
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
                                           MediaStreamType type,
                                           OpenDeviceCallback callback) {
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
                     page_request_id, device_id, type,
                     base::Passed(&callback)));
}

void MediaStreamDispatcherHost::DoOpenDevice(
    int32_t render_frame_id,
    int32_t page_request_id,
    const std::string& device_id,
    MediaStreamType type,
    OpenDeviceCallback callback,
    const std::pair<std::string, url::Origin>& salt_and_origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!MediaStreamManager::IsOriginAllowed(render_process_id_,
                                           salt_and_origin.second)) {
    std::move(callback).Run(false /* success */, std::string(),
                            MediaStreamDevice());
    return;
  }

  media_stream_manager_->OpenDevice(
      render_process_id_, render_frame_id, salt_and_origin.first,
      page_request_id, device_id, type, salt_and_origin.second,
      std::move(callback),
      base::BindRepeating(&MediaStreamDispatcherHost::OnDeviceStopped,
                          weak_factory_.GetWeakPtr()));
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

void MediaStreamDispatcherHost::OnStreamStarted(const std::string& label) {
  DVLOG(1) << __func__ << " label= " << label;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  media_stream_manager_->OnStreamStarted(label);
}

}  // namespace content
