// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/media_stream_dispatcher_host.h"

#include "base/bind_helpers.h"
#include "base/logging.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "url/origin.h"

namespace content {

namespace {

mojom::MediaStreamDispatcherPtrInfo GetMediaStreamDispatcherPtrInfo(
    int render_process_id,
    int render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RenderFrameHost* render_frame_host =
      RenderFrameHost::FromID(render_process_id, render_frame_id);
  if (!render_frame_host)
    return mojom::MediaStreamDispatcherPtrInfo();

  mojom::MediaStreamDispatcherPtr dispatcher;
  render_frame_host->GetRemoteInterfaces()->GetInterface(&dispatcher);
  return dispatcher.PassInterface();
}

}  // namespace

MediaStreamDispatcherHost::MediaStreamDispatcherHost(
    int render_process_id,
    const std::string& salt,
    MediaStreamManager* media_stream_manager)
    : render_process_id_(render_process_id),
      salt_(salt),
      media_stream_manager_(media_stream_manager) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  bindings_.set_connection_error_handler(base::Bind(
      &MediaStreamDispatcherHost::CancelAllRequests, base::Unretained(this)));
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
    const StreamDeviceInfoArray& audio_devices,
    const StreamDeviceInfoArray& video_devices) {
  DVLOG(1) << __func__ << " label= " << label;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto it = dispatchers_.find(render_frame_id);
  if (it != dispatchers_.end()) {
    it->second->OnStreamGenerated(page_request_id, label, audio_devices,
                                  video_devices);
    return;
  }

  // TODO(c.padhi): Avoid this hop between threads if possible, see
  // https://crbug.com/742682.
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&GetMediaStreamDispatcherPtrInfo, render_process_id_,
                 render_frame_id),
      base::Bind(&MediaStreamDispatcherHost::OnStreamGenerated,
                 base::Unretained(this), render_frame_id, page_request_id,
                 label, audio_devices, video_devices));
}

void MediaStreamDispatcherHost::StreamGenerationFailed(
    int render_frame_id,
    int page_request_id,
    MediaStreamRequestResult result) {
  DVLOG(1) << __func__ << " page_request_id=" << page_request_id
           << " result=" << result;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto it = dispatchers_.find(render_frame_id);
  if (it != dispatchers_.end()) {
    it->second->OnStreamGenerationFailed(page_request_id, result);
    return;
  }

  // TODO(c.padhi): Avoid this hop between threads if possible, see
  // https://crbug.com/742682.
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&GetMediaStreamDispatcherPtrInfo, render_process_id_,
                 render_frame_id),
      base::Bind(&MediaStreamDispatcherHost::OnStreamGenerationFailed,
                 base::Unretained(this), render_frame_id, page_request_id,
                 result));
}

void MediaStreamDispatcherHost::DeviceStopped(int render_frame_id,
                                              const std::string& label,
                                              const StreamDeviceInfo& device) {
  DVLOG(1) << __func__ << " label=" << label << " type=" << device.device.type
           << " device_id=" << device.device.id;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto it = dispatchers_.find(render_frame_id);
  if (it != dispatchers_.end()) {
    it->second->OnDeviceStopped(label, device);
    return;
  }

  // TODO(c.padhi): Avoid this hop between threads if possible, see
  // https://crbug.com/742682.
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&GetMediaStreamDispatcherPtrInfo, render_process_id_,
                 render_frame_id),
      base::Bind(&MediaStreamDispatcherHost::OnDeviceStopped,
                 base::Unretained(this), render_frame_id, label, device));
}

void MediaStreamDispatcherHost::DeviceOpened(
    int render_frame_id,
    int page_request_id,
    const std::string& label,
    const StreamDeviceInfo& video_device) {
  DVLOG(1) << __func__ << " page_request_id=" << page_request_id;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto it = dispatchers_.find(render_frame_id);
  if (it != dispatchers_.end()) {
    it->second->OnDeviceOpened(page_request_id, label, video_device);
    return;
  }

  // TODO(c.padhi): Avoid this hop between threads if possible, see
  // https://crbug.com/742682.
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&GetMediaStreamDispatcherPtrInfo, render_process_id_,
                 render_frame_id),
      base::Bind(&MediaStreamDispatcherHost::OnDeviceOpened,
                 base::Unretained(this), render_frame_id, page_request_id,
                 label, video_device));
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

  auto it = dispatchers_.find(render_frame_id);
  if (it != dispatchers_.end()) {
    it->second->OnDeviceOpenFailed(page_request_id);
    return;
  }

  // TODO(c.padhi): Avoid this hop between threads if possible, see
  // https://crbug.com/742682.
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&GetMediaStreamDispatcherPtrInfo, render_process_id_,
                 render_frame_id),
      base::Bind(&MediaStreamDispatcherHost::OnDeviceOpenFailed,
                 base::Unretained(this), render_frame_id, page_request_id));
}

void MediaStreamDispatcherHost::GenerateStream(
    int32_t render_frame_id,
    int32_t page_request_id,
    const StreamControls& controls,
    const url::Origin& security_origin,
    bool user_gesture) {
  DVLOG(1) << __func__ << " render_frame_id=" << render_frame_id
           << " page_request_id=" << page_request_id
           << " audio=" << controls.audio.requested
           << " video=" << controls.video.requested
           << " security_origin=" << security_origin
           << " user_gesture=" << user_gesture;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!MediaStreamManager::IsOriginAllowed(render_process_id_,
                                           security_origin)) {
    StreamGenerationFailed(render_frame_id, page_request_id,
                           MEDIA_DEVICE_INVALID_SECURITY_ORIGIN_DEPRECATED);
    return;
  }

  media_stream_manager_->GenerateStream(
      this, render_process_id_, render_frame_id, salt_, page_request_id,
      controls, security_origin, user_gesture);
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
                                           MediaStreamType type,
                                           const url::Origin& security_origin) {
  DVLOG(1) << __func__ << " render_frame_id=" << render_frame_id
           << " page_request_id=" << page_request_id
           << " device_id=" << device_id << " type=" << type
           << " security_origin=" << security_origin;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!MediaStreamManager::IsOriginAllowed(render_process_id_,
                                           security_origin)) {
    DeviceOpenFailed(render_frame_id, page_request_id);
    return;
  }

  media_stream_manager_->OpenDevice(this, render_process_id_, render_frame_id,
                                    salt_, page_request_id, device_id, type,
                                    security_origin);
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

void MediaStreamDispatcherHost::OnStreamGenerated(
    int render_frame_id,
    int page_request_id,
    const std::string& label,
    const StreamDeviceInfoArray& audio_devices,
    const StreamDeviceInfoArray& video_devices,
    mojom::MediaStreamDispatcherPtrInfo dispatcher_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  mojom::MediaStreamDispatcherPtr dispatcher =
      mojo::MakeProxy(std::move(dispatcher_info));
  if (!dispatcher || !dispatcher.is_bound())
    return;

  dispatcher->OnStreamGenerated(page_request_id, label, audio_devices,
                                video_devices);
  dispatchers_[render_frame_id] = std::move(dispatcher);
}

void MediaStreamDispatcherHost::OnStreamGenerationFailed(
    int render_frame_id,
    int page_request_id,
    MediaStreamRequestResult result,
    mojom::MediaStreamDispatcherPtrInfo dispatcher_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  mojom::MediaStreamDispatcherPtr dispatcher =
      mojo::MakeProxy(std::move(dispatcher_info));
  if (!dispatcher || !dispatcher.is_bound())
    return;

  dispatcher->OnStreamGenerationFailed(page_request_id, result);
  dispatchers_[render_frame_id] = std::move(dispatcher);
}

void MediaStreamDispatcherHost::OnDeviceOpened(
    int render_frame_id,
    int page_request_id,
    const std::string& label,
    const StreamDeviceInfo& video_device,
    mojom::MediaStreamDispatcherPtrInfo dispatcher_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  mojom::MediaStreamDispatcherPtr dispatcher =
      mojo::MakeProxy(std::move(dispatcher_info));
  if (!dispatcher || !dispatcher.is_bound())
    return;

  dispatcher->OnDeviceOpened(page_request_id, label, video_device);
  dispatchers_[render_frame_id] = std::move(dispatcher);
}

void MediaStreamDispatcherHost::OnDeviceOpenFailed(
    int render_frame_id,
    int page_request_id,
    mojom::MediaStreamDispatcherPtrInfo dispatcher_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  mojom::MediaStreamDispatcherPtr dispatcher =
      mojo::MakeProxy(std::move(dispatcher_info));
  if (!dispatcher || !dispatcher.is_bound())
    return;

  dispatcher->OnDeviceOpenFailed(page_request_id);
  dispatchers_[render_frame_id] = std::move(dispatcher);
}

void MediaStreamDispatcherHost::OnDeviceStopped(
    int render_frame_id,
    const std::string& label,
    const StreamDeviceInfo& device,
    mojom::MediaStreamDispatcherPtrInfo dispatcher_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  mojom::MediaStreamDispatcherPtr dispatcher =
      mojo::MakeProxy(std::move(dispatcher_info));
  if (!dispatcher || !dispatcher.is_bound())
    return;

  dispatcher->OnDeviceStopped(label, device);
  dispatchers_[render_frame_id] = std::move(dispatcher);
}

}  // namespace content
