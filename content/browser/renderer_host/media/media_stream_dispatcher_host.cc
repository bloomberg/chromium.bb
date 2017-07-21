// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/media_stream_dispatcher_host.h"

#include "base/logging.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/common/media/media_stream_messages.h"
#include "content/public/browser/browser_thread.h"
#include "url/origin.h"

namespace content {

MediaStreamDispatcherHost::MediaStreamDispatcherHost(
    int render_process_id,
    const std::string& salt,
    MediaStreamManager* media_stream_manager)
    : BrowserMessageFilter(MediaStreamMsgStart),
      BrowserAssociatedInterface<mojom::MediaStreamDispatcherHost>(this, this),
      render_process_id_(render_process_id),
      salt_(salt),
      media_stream_manager_(media_stream_manager) {}

void MediaStreamDispatcherHost::StreamGenerated(
    int render_frame_id,
    int page_request_id,
    const std::string& label,
    const StreamDeviceInfoArray& audio_devices,
    const StreamDeviceInfoArray& video_devices) {
  DVLOG(1) << __func__ << " label= " << label;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  Send(new MediaStreamMsg_StreamGenerated(
      render_frame_id, page_request_id, label, audio_devices, video_devices));
}

void MediaStreamDispatcherHost::StreamGenerationFailed(
    int render_frame_id,
    int page_request_id,
    MediaStreamRequestResult result) {
  DVLOG(1) << __func__ << " page_request_id=" << page_request_id
           << " result=" << result;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  Send(new MediaStreamMsg_StreamGenerationFailed(render_frame_id,
                                                 page_request_id,
                                                 result));
}

void MediaStreamDispatcherHost::DeviceStopped(int render_frame_id,
                                              const std::string& label,
                                              const StreamDeviceInfo& device) {
  DVLOG(1) << __func__ << " label=" << label << " type=" << device.device.type
           << " device_id=" << device.device.id;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  Send(new MediaStreamMsg_DeviceStopped(render_frame_id, label, device));
}

void MediaStreamDispatcherHost::DeviceOpened(
    int render_frame_id,
    int page_request_id,
    const std::string& label,
    const StreamDeviceInfo& video_device) {
  DVLOG(1) << __func__ << " page_request_id=" << page_request_id;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  Send(new MediaStreamMsg_DeviceOpened(
      render_frame_id, page_request_id, label, video_device));
}

bool MediaStreamDispatcherHost::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(MediaStreamDispatcherHost, message)
    IPC_MESSAGE_HANDLER(MediaStreamHostMsg_GenerateStream, OnGenerateStream)
    IPC_MESSAGE_HANDLER(MediaStreamHostMsg_OpenDevice,
                        OnOpenDevice)
    IPC_MESSAGE_HANDLER(MediaStreamHostMsg_SetCapturingLinkSecured,
                        OnSetCapturingLinkSecured)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void MediaStreamDispatcherHost::OnChannelClosing() {
  DVLOG(1) << __func__;

  // Since the IPC sender is gone, close all requesting/requested streams.
  media_stream_manager_->CancelAllRequests(render_process_id_);
}

MediaStreamDispatcherHost::~MediaStreamDispatcherHost() {
}

void MediaStreamDispatcherHost::OnGenerateStream(
    int render_frame_id,
    int page_request_id,
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

  if (!MediaStreamManager::IsOriginAllowed(render_process_id_, security_origin))
    return;

  media_stream_manager_->GenerateStream(
      this, render_process_id_, render_frame_id, salt_, page_request_id,
      controls, security_origin, user_gesture);
}

void MediaStreamDispatcherHost::OnOpenDevice(
    int render_frame_id,
    int page_request_id,
    const std::string& device_id,
    MediaStreamType type,
    const url::Origin& security_origin) {
  DVLOG(1) << __func__ << " render_frame_id=" << render_frame_id
           << " page_request_id=" << page_request_id
           << " device_id=" << device_id << " type=" << type
           << " security_origin=" << security_origin;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!MediaStreamManager::IsOriginAllowed(render_process_id_, security_origin))
    return;

  media_stream_manager_->OpenDevice(this, render_process_id_, render_frame_id,
                                    salt_, page_request_id, device_id, type,
                                    security_origin);
}

void MediaStreamDispatcherHost::OnSetCapturingLinkSecured(int session_id,
                                                          MediaStreamType type,
                                                          bool is_secure) {
  DVLOG(1) << __func__ << " session_id=" << session_id << " type=" << type
           << " is_secure=" << is_secure;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  media_stream_manager_->SetCapturingLinkSecured(render_process_id_, session_id,
                                                 type, is_secure);
}

void MediaStreamDispatcherHost::CancelGenerateStream(int32_t render_frame_id,
                                                     int32_t page_request_id) {
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

void MediaStreamDispatcherHost::CloseDevice(const std::string& label) {
  DVLOG(1) << __func__ << " label = " << label;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  media_stream_manager_->CancelRequest(label);
}

void MediaStreamDispatcherHost::StreamStarted(const std::string& label) {
  DVLOG(1) << __func__ << " label = " << label;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  media_stream_manager_->OnStreamStarted(label);
}

}  // namespace content
