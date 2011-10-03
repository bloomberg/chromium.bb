// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_dispatcher.h"

#include "base/logging.h"
#include "content/common/media/media_stream_messages.h"
#include "content/renderer/media/media_stream_dispatcher_eventhandler.h"

MediaStreamDispatcherEventHandler::~MediaStreamDispatcherEventHandler() {}

MediaStreamDispatcher::MediaStreamDispatcher(RenderView* render_view)
    : content::RenderViewObserver(render_view),
      next_ipc_id_(0) {
}

MediaStreamDispatcher::~MediaStreamDispatcher() {}

void MediaStreamDispatcher::GenerateStream(
    int request_id,
    MediaStreamDispatcherEventHandler* event_handler,
    media_stream::StreamOptions components,
    const std::string& security_origin) {
  VLOG(1) << "MediaStreamDispatcher::GenerateStream(" << request_id << ")";

  requests_.push_back(Request(event_handler, request_id, next_ipc_id_));
  Send(new MediaStreamHostMsg_GenerateStream(routing_id(),
                                             next_ipc_id_++,
                                             components,
                                             security_origin));
}

void MediaStreamDispatcher::StopStream(const std::string& label) {
  VLOG(1) << "MediaStreamDispatcher::StopStream"
          << ", {label = " << label << "}";

  LabelStreamMap::iterator it = label_stream_map_.find(label);
  if (it == label_stream_map_.end())
    return;

  Send(new MediaStreamHostMsg_StopGeneratedStream(routing_id(), label));
  label_stream_map_.erase(it);
}

bool MediaStreamDispatcher::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(MediaStreamDispatcher, message)
    IPC_MESSAGE_HANDLER(MediaStreamMsg_StreamGenerated,
                        OnStreamGenerated)
    IPC_MESSAGE_HANDLER(MediaStreamMsg_StreamGenerationFailed,
                        OnStreamGenerationFailed)
    IPC_MESSAGE_HANDLER(MediaStreamHostMsg_VideoDeviceFailed,
                        OnVideoDeviceFailed)
    IPC_MESSAGE_HANDLER(MediaStreamHostMsg_AudioDeviceFailed,
                        OnAudioDeviceFailed)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void MediaStreamDispatcher::OnStreamGenerated(
    int request_id,
    const std::string& label,
    const media_stream::StreamDeviceInfoArray& audio_array,
    const media_stream::StreamDeviceInfoArray& video_array) {

  for (RequestList::iterator it = requests_.begin();
       it != requests_.end(); ++it) {
    Request& request = *it;
    if (request.ipc_request == request_id) {
      Stream new_stream;
      new_stream.handler = request.handler;
      new_stream.audio_array = audio_array;
      new_stream.video_array = video_array;
      label_stream_map_[label] = new_stream;
      request.handler->OnStreamGenerated(request.request_id, label,
                                         audio_array, video_array);
      VLOG(1) << "MediaStreamDispatcher::OnStreamGenerated("
              << request.request_id << ", " << label << ")";
      requests_.erase(it);
      break;
    }
  }
}

void MediaStreamDispatcher::OnStreamGenerationFailed(int request_id) {
  for (RequestList::iterator it = requests_.begin();
       it != requests_.end(); ++it) {
    Request& request = *it;
    if (request.ipc_request == request_id) {
      request.handler->OnStreamGenerationFailed(request.request_id);
      VLOG(1) << "MediaStreamDispatcher::OnStreamGenerationFailed("
              << request.request_id << ")\n";
      requests_.erase(it);
      break;
    }
  }
}

void MediaStreamDispatcher::OnVideoDeviceFailed(const std::string& label,
                                                int index) {
  LabelStreamMap::iterator it = label_stream_map_.find(label);
  if (it == label_stream_map_.end())
    return;

  // index is the index in the video_array that has failed.
  DCHECK_GT(it->second.video_array.size(), static_cast<size_t>(index));
  media_stream::StreamDeviceInfoArray::iterator device_it =
      it->second.video_array.begin();
  it->second.video_array.erase(device_it + index);
  it->second.handler->OnVideoDeviceFailed(label, index);
}

void MediaStreamDispatcher::OnAudioDeviceFailed(const std::string& label,
                                                int index) {
  LabelStreamMap::iterator it = label_stream_map_.find(label);
  if (it == label_stream_map_.end())
    return;

  // index is the index in the audio_array that has failed.
  DCHECK_GT(it->second.audio_array.size(), static_cast<size_t>(index));
  media_stream::StreamDeviceInfoArray::iterator device_it =
      it->second.audio_array.begin();
  it->second.audio_array.erase(device_it + index);
  it->second.handler->OnAudioDeviceFailed(label, index);
}

int MediaStreamDispatcher::audio_session_id(const std::string& label,
                                            int index) {
  LabelStreamMap::iterator it = label_stream_map_.find(label);
  if (it == label_stream_map_.end())
    return media_stream::StreamDeviceInfo::kNoId;

  DCHECK_GT(it->second.audio_array.size(), static_cast<size_t>(index));
  return it->second.audio_array[index].session_id;
}

bool MediaStreamDispatcher::IsStream(const std::string& label) {
  return label_stream_map_.find(label) != label_stream_map_.end();
}

int MediaStreamDispatcher::video_session_id(const std::string& label,
                                            int index) {
  LabelStreamMap::iterator it = label_stream_map_.find(label);
  if (it == label_stream_map_.end())
    return media_stream::StreamDeviceInfo::kNoId;

  DCHECK_GT(it->second.video_array.size(), static_cast<size_t>(index));
  return it->second.video_array[index].session_id;
}

MediaStreamDispatcher::Stream::Stream()
    : handler(NULL) {}

MediaStreamDispatcher::Stream::~Stream() {}

