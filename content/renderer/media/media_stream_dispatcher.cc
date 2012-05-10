// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_dispatcher.h"

#include "base/logging.h"
#include "content/common/media/media_stream_messages.h"
#include "content/renderer/media/media_stream_dispatcher_eventhandler.h"
#include "content/renderer/render_view_impl.h"

struct MediaStreamDispatcher::Request {
  Request(const base::WeakPtr<MediaStreamDispatcherEventHandler>& handler,
          int request_id,
          int ipc_request)
      : handler(handler),
        request_id(request_id),
        ipc_request(ipc_request) {
  }
  base::WeakPtr<MediaStreamDispatcherEventHandler> handler;
  int request_id;
  int ipc_request;
};

struct MediaStreamDispatcher::Stream {
  Stream() {}
  ~Stream() {}
  base::WeakPtr<MediaStreamDispatcherEventHandler> handler;
  media_stream::StreamDeviceInfoArray audio_array;
  media_stream::StreamDeviceInfoArray video_array;
};

MediaStreamDispatcher::MediaStreamDispatcher(RenderViewImpl* render_view)
    : content::RenderViewObserver(render_view),
      next_ipc_id_(0) {
}

MediaStreamDispatcher::~MediaStreamDispatcher() {}

void MediaStreamDispatcher::GenerateStream(
    int request_id,
    const base::WeakPtr<MediaStreamDispatcherEventHandler>& event_handler,
    media_stream::StreamOptions components,
    const std::string& security_origin) {
  DVLOG(1) << "MediaStreamDispatcher::GenerateStream(" << request_id << ")";

  requests_.push_back(Request(event_handler, request_id, next_ipc_id_));
  Send(new MediaStreamHostMsg_GenerateStream(routing_id(),
                                             next_ipc_id_++,
                                             components,
                                             security_origin));
}

void MediaStreamDispatcher::CancelGenerateStream(int request_id) {
  DVLOG(1) << "MediaStreamDispatcher::CancelGenerateStream"
           << ", {request_id = " << request_id << "}";

  RequestList::iterator it = requests_.begin();
  for (; it != requests_.end(); ++it) {
    Request& request = *it;
    if (request.request_id == request_id) {
      requests_.erase(it);
      Send(new MediaStreamHostMsg_CancelGenerateStream(routing_id(),
                                                       request_id));
      break;
    }
  }
}

void MediaStreamDispatcher::StopStream(const std::string& label) {
  DVLOG(1) << "MediaStreamDispatcher::StopStream"
           << ", {label = " << label << "}";

  LabelStreamMap::iterator it = label_stream_map_.find(label);
  if (it == label_stream_map_.end())
    return;

  Send(new MediaStreamHostMsg_StopGeneratedStream(routing_id(), label));
  label_stream_map_.erase(it);
}

void MediaStreamDispatcher::EnumerateDevices(
    int request_id,
    const base::WeakPtr<MediaStreamDispatcherEventHandler>& event_handler,
    media_stream::MediaStreamType type,
    const std::string& security_origin) {
  DVLOG(1) << "MediaStreamDispatcher::EnumerateDevices("
           << request_id << ")";

  requests_.push_back(Request(event_handler, request_id, next_ipc_id_));
  Send(new MediaStreamHostMsg_EnumerateDevices(routing_id(),
                                               next_ipc_id_++,
                                               type,
                                               security_origin));
}

void MediaStreamDispatcher::OpenDevice(
    int request_id,
    const base::WeakPtr<MediaStreamDispatcherEventHandler>& event_handler,
    const std::string& device_id,
    media_stream::MediaStreamType type,
    const std::string& security_origin) {
  DVLOG(1) << "MediaStreamDispatcher::OpenDevice(" << request_id << ")";

  requests_.push_back(Request(event_handler, request_id, next_ipc_id_));
  Send(new MediaStreamHostMsg_OpenDevice(routing_id(),
                                         next_ipc_id_++,
                                         device_id,
                                         type,
                                         security_origin));
}

void MediaStreamDispatcher::CloseDevice(const std::string& label) {
  DVLOG(1) << "MediaStreamDispatcher::CloseDevice"
           << ", {label = " << label << "}";

  StopStream(label);
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
    IPC_MESSAGE_HANDLER(MediaStreamMsg_DevicesEnumerated,
                        OnDevicesEnumerated)
    IPC_MESSAGE_HANDLER(MediaStreamMsg_DevicesEnumerationFailed,
                        OnDevicesEnumerationFailed)
    IPC_MESSAGE_HANDLER(MediaStreamMsg_DeviceOpened,
                        OnDeviceOpened)
    IPC_MESSAGE_HANDLER(MediaStreamMsg_DeviceOpenFailed,
                        OnDeviceOpenFailed)
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
      if (request.handler) {
        request.handler->OnStreamGenerated(request.request_id, label,
                                           audio_array, video_array);
        DVLOG(1) << "MediaStreamDispatcher::OnStreamGenerated("
                 << request.request_id << ", " << label << ")";
      }
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
      if (request.handler) {
        request.handler->OnStreamGenerationFailed(request.request_id);
        DVLOG(1) << "MediaStreamDispatcher::OnStreamGenerationFailed("
                 << request.request_id << ")\n";
      }
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
  if (it->second.handler)
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
  if (it->second.handler)
    it->second.handler->OnAudioDeviceFailed(label, index);
}

void MediaStreamDispatcher::OnDevicesEnumerated(
    int request_id,
    const media_stream::StreamDeviceInfoArray& device_array) {

  for (RequestList::iterator it = requests_.begin();
       it != requests_.end(); ++it) {
    Request& request = *it;
    if (request.ipc_request == request_id) {
      if (request.handler) {
        request.handler->OnDevicesEnumerated(request.request_id, device_array);
        DVLOG(1) << "MediaStreamDispatcher::OnDevicesEnumerated("
                 << request.request_id << ")";
      }
      requests_.erase(it);
      break;
    }
  }
}

void MediaStreamDispatcher::OnDevicesEnumerationFailed(int request_id) {
  for (RequestList::iterator it = requests_.begin();
       it != requests_.end(); ++it) {
    Request& request = *it;
    if (request.ipc_request == request_id) {
      if (request.handler) {
        request.handler->OnStreamGenerationFailed(request.request_id);
        DVLOG(1) << "MediaStreamDispatcher::OnDevicesEnumerationFailed("
                 << request.request_id << ")\n";
      }
      requests_.erase(it);
      break;
    }
  }
}

void MediaStreamDispatcher::OnDeviceOpened(
    int request_id,
    const std::string& label,
    const media_stream::StreamDeviceInfo& device_info) {
  for (RequestList::iterator it = requests_.begin();
       it != requests_.end(); ++it) {
    Request& request = *it;
    if (request.ipc_request == request_id) {
      Stream new_stream;
      new_stream.handler = request.handler;
      if (device_info.stream_type ==
              content::MEDIA_STREAM_DEVICE_TYPE_VIDEO_CAPTURE) {
        new_stream.video_array.push_back(device_info);
      } else {
        new_stream.audio_array.push_back(device_info);
      }
      label_stream_map_[label] = new_stream;
      if (request.handler) {
        request.handler->OnDeviceOpened(request.request_id, label,
                                        device_info);
        DVLOG(1) << "MediaStreamDispatcher::OnDeviceOpened("
                 << request.request_id << ", " << label << ")";
      }
      requests_.erase(it);
      break;
    }
  }
}

void MediaStreamDispatcher::OnDeviceOpenFailed(int request_id) {
  for (RequestList::iterator it = requests_.begin();
       it != requests_.end(); ++it) {
    Request& request = *it;
    if (request.ipc_request == request_id) {
      if (request.handler) {
        request.handler->OnDeviceOpenFailed(request.request_id);
        DVLOG(1) << "MediaStreamDispatcher::OnDeviceOpenFailed("
                 << request.request_id << ")\n";
      }
      requests_.erase(it);
      break;
    }
  }
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
