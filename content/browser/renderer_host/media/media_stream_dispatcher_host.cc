// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/media_stream_dispatcher_host.h"

#include "content/browser/browser_main_loop.h"
#include "content/browser/renderer_host/media/web_contents_capture_util.h"
#include "content/common/media/media_stream_messages.h"
#include "content/common/media/media_stream_options.h"
#include "url/gurl.h"

namespace content {

MediaStreamDispatcherHost::MediaStreamDispatcherHost(
    int render_process_id,
    MediaStreamManager* media_stream_manager)
    : render_process_id_(render_process_id),
      media_stream_manager_(media_stream_manager) {
}

void MediaStreamDispatcherHost::StreamGenerated(
    const std::string& label,
    const StreamDeviceInfoArray& audio_devices,
    const StreamDeviceInfoArray& video_devices) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DVLOG(1) << "MediaStreamDispatcherHost::StreamGenerated("
           << ", {label = " << label <<  "})";

  StreamRequest request = PopRequest(label);

  Send(new MediaStreamMsg_StreamGenerated(
      request.render_view_id, request.page_request_id, label, audio_devices,
      video_devices));
}

void MediaStreamDispatcherHost::StreamGenerationFailed(
    const std::string& label) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DVLOG(1) << "MediaStreamDispatcherHost::StreamGenerationFailed("
           << ", {label = " << label <<  "})";

  StreamRequest request = PopRequest(label);

  Send(new MediaStreamMsg_StreamGenerationFailed(request.render_view_id,
                                                 request.page_request_id));
}

void MediaStreamDispatcherHost::StopGeneratedStream(
    int render_view_id,
    const std::string& label) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DVLOG(1) << "MediaStreamDispatcherHost::StopGeneratedStream("
           << ", {label = " << label <<  "})";

  Send(new MediaStreamMsg_StopGeneratedStream(render_view_id, label));
}

void MediaStreamDispatcherHost::DevicesEnumerated(
    const std::string& label,
    const StreamDeviceInfoArray& devices) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DVLOG(1) << "MediaStreamDispatcherHost::DevicesEnumerated("
           << ", {label = " << label <<  "})";

  StreamMap::iterator it = streams_.find(label);
  DCHECK(it != streams_.end());
  StreamRequest request = it->second;

  Send(new MediaStreamMsg_DevicesEnumerated(
      request.render_view_id, request.page_request_id, label, devices));
}

void MediaStreamDispatcherHost::DeviceOpened(
    const std::string& label,
    const StreamDeviceInfo& video_device) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DVLOG(1) << "MediaStreamDispatcherHost::DeviceOpened("
           << ", {label = " << label <<  "})";

  StreamRequest request = PopRequest(label);

  Send(new MediaStreamMsg_DeviceOpened(
      request.render_view_id, request.page_request_id, label, video_device));
}

bool MediaStreamDispatcherHost::OnMessageReceived(
    const IPC::Message& message, bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(MediaStreamDispatcherHost, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(MediaStreamHostMsg_GenerateStream, OnGenerateStream)
    IPC_MESSAGE_HANDLER(MediaStreamHostMsg_CancelGenerateStream,
                        OnCancelGenerateStream)
    IPC_MESSAGE_HANDLER(MediaStreamHostMsg_StopStreamDevice,
                        OnStopStreamDevice)
    IPC_MESSAGE_HANDLER(MediaStreamHostMsg_EnumerateDevices,
                        OnEnumerateDevices)
    IPC_MESSAGE_HANDLER(MediaStreamHostMsg_OpenDevice,
                        OnOpenDevice)
    IPC_MESSAGE_HANDLER(MediaStreamHostMsg_CloseDevice,
                        OnCloseDevice)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  return handled;
}

void MediaStreamDispatcherHost::OnChannelClosing() {
  DVLOG(1) << "MediaStreamDispatcherHost::OnChannelClosing";

  // Since the IPC channel is gone, close all requesting/requested streams.
  media_stream_manager_->CancelAllRequests(render_process_id_);
  // Clear the map after we have stopped all the streams.
  streams_.clear();
}

MediaStreamDispatcherHost::~MediaStreamDispatcherHost() {
  DCHECK(streams_.empty());
}

void MediaStreamDispatcherHost::OnGenerateStream(
    int render_view_id,
    int page_request_id,
    const StreamOptions& components,
    const GURL& security_origin) {
  DVLOG(1) << "MediaStreamDispatcherHost::OnGenerateStream("
           << render_view_id << ", "
           << page_request_id << ", ["
           << " audio:" << components.audio_type
           << " video:" << components.video_type
           << " ], "
           << security_origin.spec() << ")";

  const std::string& label = media_stream_manager_->GenerateStream(
      this, render_process_id_, render_view_id, page_request_id,
      components, security_origin);
  if (label.empty()) {
    Send(new MediaStreamMsg_StreamGenerationFailed(render_view_id,
                                                   page_request_id));
  } else {
    StoreRequest(render_view_id, page_request_id, label);
  }
}

void MediaStreamDispatcherHost::OnCancelGenerateStream(int render_view_id,
                                                       int page_request_id) {
  DVLOG(1) << "MediaStreamDispatcherHost::OnCancelGenerateStream("
           << render_view_id << ", "
           << page_request_id << ")";

  for (StreamMap::iterator it = streams_.begin(); it != streams_.end(); ++it) {
    if (it->second.render_view_id == render_view_id &&
        it->second.page_request_id == page_request_id) {
      const std::string& label = it->first;
      media_stream_manager_->CancelRequest(label);
      PopRequest(label);
      break;
    }
  }
}

void MediaStreamDispatcherHost::OnStopStreamDevice(
    int render_view_id,
    const std::string& device_id) {
  DVLOG(1) << "MediaStreamDispatcherHost::OnStopStreamDevice("
           << render_view_id << ", "
           << device_id << ")";
  media_stream_manager_->StopStreamDevice(render_process_id_, render_view_id,
                                          device_id);
}

void MediaStreamDispatcherHost::OnEnumerateDevices(
    int render_view_id,
    int page_request_id,
    MediaStreamType type,
    const GURL& security_origin) {
  DVLOG(1) << "MediaStreamDispatcherHost::OnEnumerateDevices("
           << render_view_id << ", "
           << page_request_id << ", "
           << type << ", "
           << security_origin.spec() << ")";

  const std::string& label = media_stream_manager_->EnumerateDevices(
      this, render_process_id_, render_view_id, page_request_id,
      type, security_origin);
  StoreRequest(render_view_id, page_request_id, label);
}

void MediaStreamDispatcherHost::OnCancelEnumerateDevices(
    int render_view_id,
    const std::string& label) {
  DVLOG(1) << "MediaStreamDispatcherHost::OnCancelEnumerateDevices("
           << render_view_id << ", "
           << label << ")";

  media_stream_manager_->CancelRequest(label);
  PopRequest(label);
}

void MediaStreamDispatcherHost::OnOpenDevice(
    int render_view_id,
    int page_request_id,
    const std::string& device_id,
    MediaStreamType type,
    const GURL& security_origin) {
  DVLOG(1) << "MediaStreamDispatcherHost::OnOpenDevice("
           << render_view_id << ", "
           << page_request_id << ", device_id: "
           << device_id.c_str() << ", type: "
           << type << ", "
           << security_origin.spec() << ")";

  const std::string& label = media_stream_manager_->OpenDevice(
      this, render_process_id_, render_view_id, page_request_id,
      device_id, type, security_origin);
  StoreRequest(render_view_id, page_request_id, label);
}

void MediaStreamDispatcherHost::OnCloseDevice(
    int render_view_id,
    const std::string& label) {
  DVLOG(1) << "MediaStreamDispatcherHost::OnCloseDevice("
           << render_view_id << ", "
           << label << ")";

  media_stream_manager_->CancelRequest(label);
}

void MediaStreamDispatcherHost::StoreRequest(int render_view_id,
                                             int page_request_id,
                                             const std::string& label) {
  DCHECK(!label.empty());
  DCHECK(streams_.find(label) == streams_.end());

  streams_[label] = StreamRequest(render_view_id, page_request_id);
}

MediaStreamDispatcherHost::StreamRequest
MediaStreamDispatcherHost::PopRequest(const std::string& label) {
  StreamMap::iterator it = streams_.find(label);
  CHECK(it != streams_.end());
  StreamRequest request = it->second;
  streams_.erase(it);
  return request;
}

}  // namespace content
