// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/media_stream_dispatcher_host.h"

#include "content/browser/resource_context.h"
#include "content/common/media/media_stream_messages.h"
#include "content/common/media/media_stream_options.h"

namespace media_stream {

MediaStreamDispatcherHost::MediaStreamDispatcherHost(
    const content::ResourceContext* resource_context, int render_process_id)
    : resource_context_(resource_context),
      render_process_id_(render_process_id) {
}

MediaStreamDispatcherHost::~MediaStreamDispatcherHost() {
}

MediaStreamManager* MediaStreamDispatcherHost::manager() {
  return resource_context_->media_stream_manager();
}

bool MediaStreamDispatcherHost::OnMessageReceived(
    const IPC::Message& message, bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(MediaStreamDispatcherHost, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(MediaStreamHostMsg_GenerateStream, OnGenerateStream)
    IPC_MESSAGE_HANDLER(MediaStreamHostMsg_StopGeneratedStream,
                        OnStopGeneratedStream)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  return handled;
}

void MediaStreamDispatcherHost::OnChannelClosing() {
  BrowserMessageFilter::OnChannelClosing();
  VLOG(1) << "MediaStreamDispatcherHost::OnChannelClosing";

  // TODO(mflodman) Remove this temporary solution when shut-down issue is
  // resolved, i.e. uncomment the code below.
  // Since the IPC channel is gone, close all requested VideCaptureDevices and
  // cancel pending requests.
//  manager()->CancelRequests(this);
//  for (StreamMap::iterator it = streams_.begin();
//       it != streams_.end();
//       it++) {
//    std::string label = it->first;
//    manager()->StopGeneratedStream(label);
//  }
}

void MediaStreamDispatcherHost::OnGenerateStream(
    int render_view_id,
    int page_request_id,
    const media_stream::StreamOptions& components,
    const std::string& security_origin) {
  VLOG(1) << "MediaStreamDispatcherHost::OnGenerateStream("
          << render_view_id << ", "
          << page_request_id << ", [ "
          << (components.audio ? "audio " : "")
          << ((components.video_option &
              StreamOptions::kFacingUser) ?
              "video_facing_user " : "")
          << ((components.video_option &
               StreamOptions::kFacingEnvironment) ?
              "video_facing_environment " : "")
          << "], "
          << security_origin << ")";

  std::string label;
  manager()->GenerateStream(this, render_process_id_, render_view_id,
                            components, security_origin, &label);
  DCHECK(!label.empty());
  streams_[label] = StreamRequest(render_view_id, page_request_id);
}

void MediaStreamDispatcherHost::OnStopGeneratedStream(
    int render_view_id, const std::string& label) {
  VLOG(1) << "MediaStreamDispatcherHost::OnStopGeneratedStream("
          << ", {label = " << label <<  "})";

  StreamMap::iterator it = streams_.find(label);
  DCHECK(it != streams_.end());
  manager()->StopGeneratedStream(label);
  streams_.erase(it);
}

void MediaStreamDispatcherHost::StreamGenerated(
    const std::string& label,
    const StreamDeviceInfoArray& audio_devices,
    const StreamDeviceInfoArray& video_devices) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  VLOG(1) << "MediaStreamDispatcherHost::StreamGenerated("
          << ", {label = " << label <<  "})";

  StreamMap::iterator it = streams_.find(label);
  DCHECK(it != streams_.end());
  StreamRequest request = it->second;

  Send(new MediaStreamMsg_StreamGenerated(
      request.render_view_id, request.page_request_id, label, audio_devices,
      video_devices));
}

void MediaStreamDispatcherHost::StreamGenerationFailed(
    const std::string& label) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  VLOG(1) << "MediaStreamDispatcherHost::StreamGenerationFailed("
          << ", {label = " << label <<  "})";

  StreamMap::iterator it = streams_.find(label);
  DCHECK(it != streams_.end());
  StreamRequest request = it->second;
  streams_.erase(it);

  Send(new MediaStreamMsg_StreamGenerationFailed(request.render_view_id,
                                                 request.page_request_id));
}

void MediaStreamDispatcherHost::AudioDeviceFailed(const std::string& label,
                                                  int index) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  VLOG(1) << "MediaStreamDispatcherHost::AudioDeviceFailed("
          << ", {label = " << label <<  "})";

  StreamMap::iterator it = streams_.find(label);
  DCHECK(it != streams_.end());
  StreamRequest request = it->second;
  Send(new MediaStreamHostMsg_AudioDeviceFailed(request.render_view_id,
                                                label,
                                                index));
}

void MediaStreamDispatcherHost::VideoDeviceFailed(const std::string& label,
                                                  int index) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  VLOG(1) << "MediaStreamDispatcherHost::VideoDeviceFailed("
          << ", {label = " << label <<  "})";

  StreamMap::iterator it = streams_.find(label);
  DCHECK(it != streams_.end());
  StreamRequest request = it->second;
  Send(new MediaStreamHostMsg_VideoDeviceFailed(request.render_view_id,
                                                label,
                                                index));
}

}  // namespace media_stream
