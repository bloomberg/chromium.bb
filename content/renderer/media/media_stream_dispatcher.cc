// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_dispatcher.h"

#include <stddef.h>

#include "base/bind_helpers.h"
#include "base/logging.h"
#include "content/child/child_thread_impl.h"
#include "content/common/media/media_stream_messages.h"
#include "content/public/renderer/render_frame.h"
#include "content/renderer/media/media_stream_dispatcher_eventhandler.h"
#include "ipc/ipc_sync_channel.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "url/origin.h"

namespace content {

namespace {

bool RemoveStreamDeviceFromArray(const StreamDeviceInfo device_info,
                                 StreamDeviceInfoArray* array) {
  for (StreamDeviceInfoArray::iterator device_it = array->begin();
       device_it != array->end(); ++device_it) {
    if (StreamDeviceInfo::IsEqual(*device_it, device_info)) {
      array->erase(device_it);
      return true;
    }
  }
  return false;
}

}  // namespace

// A request is identified by pair (request_id, handler), or ipc_request.
// There could be multiple clients making requests and each has its own
// request_id sequence.
// The ipc_request is garanteed to be unique when it's created in
// MediaStreamDispatcher.
struct MediaStreamDispatcher::Request {
  Request(const base::WeakPtr<MediaStreamDispatcherEventHandler>& handler,
          int request_id,
          int ipc_request)
      : handler(handler),
        request_id(request_id),
        ipc_request(ipc_request) {
  }
  bool IsThisRequest(
      int request_id1,
      const base::WeakPtr<MediaStreamDispatcherEventHandler>& handler1) {
    return (request_id1 == request_id && handler1.get() == handler.get());
  }
  base::WeakPtr<MediaStreamDispatcherEventHandler> handler;
  int request_id;
  int ipc_request;
};

struct MediaStreamDispatcher::Stream {
  Stream() {}
  ~Stream() {}
  base::WeakPtr<MediaStreamDispatcherEventHandler> handler;
  StreamDeviceInfoArray audio_array;
  StreamDeviceInfoArray video_array;
};

MediaStreamDispatcher::MediaStreamDispatcher(RenderFrame* render_frame)
    : RenderFrameObserver(render_frame),
      dispatcher_host_(nullptr),
      binding_(this),
      next_ipc_id_(0) {
  registry_.AddInterface(
      base::Bind(&MediaStreamDispatcher::BindMediaStreamDispatcherRequest,
                 base::Unretained(this)));
}

MediaStreamDispatcher::~MediaStreamDispatcher() {}

void MediaStreamDispatcher::GenerateStream(
    int request_id,
    const base::WeakPtr<MediaStreamDispatcherEventHandler>& event_handler,
    const StreamControls& controls,
    const url::Origin& security_origin,
    bool is_processing_user_gesture) {
  DVLOG(1) << __func__ << " request_id= " << request_id;
  DCHECK(thread_checker_.CalledOnValidThread());

  requests_.push_back(Request(event_handler, request_id, next_ipc_id_));
  GetMediaStreamDispatcherHost()->GenerateStream(routing_id(), next_ipc_id_++,
                                                 controls, security_origin,
                                                 is_processing_user_gesture);
}

void MediaStreamDispatcher::CancelGenerateStream(
    int request_id,
    const base::WeakPtr<MediaStreamDispatcherEventHandler>& event_handler) {
  DVLOG(1) << __func__ << " request_id= " << request_id;
  DCHECK(thread_checker_.CalledOnValidThread());

  for (auto it = requests_.begin(); it != requests_.end(); ++it) {
    if (it->IsThisRequest(request_id, event_handler)) {
      int ipc_request = it->ipc_request;
      requests_.erase(it);
      GetMediaStreamDispatcherHost()->CancelGenerateStream(routing_id(),
                                                           ipc_request);
      break;
    }
  }
}

void MediaStreamDispatcher::StopStreamDevice(
    const StreamDeviceInfo& device_info) {
  DVLOG(1) << __func__ << " device_id= " << device_info.device.id;
  DCHECK(thread_checker_.CalledOnValidThread());

  // Remove |device_info| from all streams in |label_stream_map_|.
  bool device_found = false;
  LabelStreamMap::iterator stream_it = label_stream_map_.begin();
  while (stream_it != label_stream_map_.end()) {
    StreamDeviceInfoArray& audio_array = stream_it->second.audio_array;
    StreamDeviceInfoArray& video_array = stream_it->second.video_array;

    if (RemoveStreamDeviceFromArray(device_info, &audio_array) ||
        RemoveStreamDeviceFromArray(device_info, &video_array)) {
      device_found = true;
      if (audio_array.empty() && video_array.empty()) {
        label_stream_map_.erase(stream_it++);
        continue;
      }
    }
    ++stream_it;
  }
  DCHECK(device_found);

  GetMediaStreamDispatcherHost()->StopStreamDevice(routing_id(),
                                                   device_info.device.id);
}

void MediaStreamDispatcher::OpenDevice(
    int request_id,
    const base::WeakPtr<MediaStreamDispatcherEventHandler>& event_handler,
    const std::string& device_id,
    MediaStreamType type,
    const url::Origin& security_origin) {
  DVLOG(1) << __func__ << " request_id= " << request_id;
  DCHECK(thread_checker_.CalledOnValidThread());

  requests_.push_back(Request(event_handler, request_id, next_ipc_id_));
  GetMediaStreamDispatcherHost()->OpenDevice(routing_id(), next_ipc_id_++,
                                             device_id, type, security_origin);
}

void MediaStreamDispatcher::CancelOpenDevice(
    int request_id,
    const base::WeakPtr<MediaStreamDispatcherEventHandler>& event_handler) {
  DVLOG(1) << __func__ << " request_id= " << request_id;
  DCHECK(thread_checker_.CalledOnValidThread());

  CancelGenerateStream(request_id, event_handler);
}

void MediaStreamDispatcher::CloseDevice(const std::string& label) {
  DVLOG(1) << __func__ << " label= " << label;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!label.empty());

  LabelStreamMap::iterator it = label_stream_map_.find(label);
  if (it == label_stream_map_.end())
    return;
  label_stream_map_.erase(it);

  GetMediaStreamDispatcherHost()->CloseDevice(label);
}

void MediaStreamDispatcher::OnStreamStarted(const std::string& label) {
  DVLOG(1) << __func__ << " label= " << label;
  DCHECK(thread_checker_.CalledOnValidThread());

  GetMediaStreamDispatcherHost()->StreamStarted(label);
}

StreamDeviceInfoArray MediaStreamDispatcher::GetNonScreenCaptureDevices() {
  StreamDeviceInfoArray video_array;
  for (const auto& stream_it : label_stream_map_) {
    for (const auto& video_device : stream_it.second.video_array) {
      if (!IsScreenCaptureMediaType(video_device.device.type))
        video_array.push_back(video_device);
    }
  }
  return video_array;
}

void MediaStreamDispatcher::OnInterfaceRequestForFrame(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle* interface_pipe) {
  registry_.TryBindInterface(interface_name, interface_pipe);
}

void MediaStreamDispatcher::OnDestruct() {
  // Do not self-destruct. UserMediaClientImpl owns |this|.
}

bool MediaStreamDispatcher::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(MediaStreamDispatcher, message)
    IPC_MESSAGE_HANDLER(MediaStreamMsg_StreamGenerated,
                        OnStreamGenerated)
    IPC_MESSAGE_HANDLER(MediaStreamMsg_DeviceStopped,
                        OnDeviceStopped)
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
    const StreamDeviceInfoArray& audio_array,
    const StreamDeviceInfoArray& video_array) {
  DCHECK(thread_checker_.CalledOnValidThread());

  for (auto it = requests_.begin(); it != requests_.end(); ++it) {
    Request& request = *it;
    if (request.ipc_request != request_id)
      continue;
    Stream new_stream;
    new_stream.handler = request.handler;
    new_stream.audio_array = audio_array;
    new_stream.video_array = video_array;
    label_stream_map_[label] = new_stream;
    if (request.handler.get()) {
      request.handler->OnStreamGenerated(request.request_id, label, audio_array,
                                         video_array);
      DVLOG(1) << __func__ << " request_id=" << request.request_id
               << " label=" << label;
    }
    requests_.erase(it);
    break;
  }
}

void MediaStreamDispatcher::OnDeviceStopped(
    const std::string& label,
    const StreamDeviceInfo& device_info) {
  DVLOG(1) << __func__ << " label=" << label
           << " device_id=" << device_info.device.id;
  DCHECK(thread_checker_.CalledOnValidThread());

  LabelStreamMap::iterator it = label_stream_map_.find(label);
  if (it == label_stream_map_.end()) {
    // This can happen if a user happen stop a the device from JS at the same
    // time as the underlying media device is unplugged from the system.
    return;
  }
  Stream* stream = &it->second;
  if (IsAudioInputMediaType(device_info.device.type))
    RemoveStreamDeviceFromArray(device_info, &stream->audio_array);
  else
    RemoveStreamDeviceFromArray(device_info, &stream->video_array);

  if (stream->handler.get())
    stream->handler->OnDeviceStopped(label, device_info);

  // |it| could have already been invalidated in the function call above. So we
  // need to check if |label| is still in |label_stream_map_| again.
  // Note: this is a quick fix to the crash caused by erasing the invalidated
  // iterator from |label_stream_map_| (crbug.com/616884). Future work needs to
  // be done to resolve this re-entrancy issue.
  it = label_stream_map_.find(label);
  if (it == label_stream_map_.end())
    return;
  stream = &it->second;
  if (stream->audio_array.empty() && stream->video_array.empty())
    label_stream_map_.erase(it);
}

void MediaStreamDispatcher::OnDeviceOpened(
    int request_id,
    const std::string& label,
    const StreamDeviceInfo& device_info) {
  DCHECK(thread_checker_.CalledOnValidThread());

  for (auto it = requests_.begin(); it != requests_.end(); ++it) {
    Request& request = *it;
    if (request.ipc_request != request_id)
      continue;
    Stream new_stream;
    new_stream.handler = request.handler;
    if (IsAudioInputMediaType(device_info.device.type))
      new_stream.audio_array.push_back(device_info);
    else if (IsVideoMediaType(device_info.device.type))
      new_stream.video_array.push_back(device_info);
    else
      NOTREACHED();

    label_stream_map_[label] = new_stream;
    if (request.handler.get()) {
      request.handler->OnDeviceOpened(request.request_id, label, device_info);
      DVLOG(1) << __func__ << " request_id=" << request.request_id
               << " label=" << label;
    }
    requests_.erase(it);
    break;
  }
}

void MediaStreamDispatcher::OnDeviceOpenFailed(int request_id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  for (auto it = requests_.begin(); it != requests_.end(); ++it) {
    Request& request = *it;
    if (request.ipc_request != request_id)
      continue;
    if (request.handler.get()) {
      request.handler->OnDeviceOpenFailed(request.request_id);
      DVLOG(1) << __func__ << " request_id=" << request.request_id;
    }
    requests_.erase(it);
    break;
  }
}

void MediaStreamDispatcher::OnStreamGenerationFailed(
    int32_t request_id,
    MediaStreamRequestResult result) {
  DCHECK(thread_checker_.CalledOnValidThread());

  for (auto it = requests_.begin(); it != requests_.end(); ++it) {
    Request& request = *it;
    if (request.ipc_request != request_id)
      continue;
    if (request.handler.get()) {
      request.handler->OnStreamGenerationFailed(request.request_id, result);
      DVLOG(1) << __func__ << " request_id=" << request.request_id;
    }
    requests_.erase(it);
    break;
  }
}

void MediaStreamDispatcher::BindMediaStreamDispatcherRequest(
    mojom::MediaStreamDispatcherRequest request) {
  binding_.Bind(std::move(request));
}

mojom::MediaStreamDispatcherHost*
MediaStreamDispatcher::GetMediaStreamDispatcherHost() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!dispatcher_host_) {
    ChildThreadImpl::current()->channel()->GetRemoteAssociatedInterface(
        &dispatcher_host_ptr_);
    dispatcher_host_ = dispatcher_host_ptr_.get();
  }
  return dispatcher_host_;
};

int MediaStreamDispatcher::audio_session_id(const std::string& label,
                                            int index) {
  DCHECK(thread_checker_.CalledOnValidThread());

  LabelStreamMap::iterator it = label_stream_map_.find(label);
  if (it == label_stream_map_.end() ||
      it->second.audio_array.size() <= static_cast<size_t>(index)) {
    return StreamDeviceInfo::kNoId;
  }
  return it->second.audio_array[index].session_id;
}

bool MediaStreamDispatcher::IsStream(const std::string& label) {
  DCHECK(thread_checker_.CalledOnValidThread());

  return label_stream_map_.find(label) != label_stream_map_.end();
}

int MediaStreamDispatcher::video_session_id(const std::string& label,
                                            int index) {
  DCHECK(thread_checker_.CalledOnValidThread());

  LabelStreamMap::iterator it = label_stream_map_.find(label);
  if (it == label_stream_map_.end() ||
      it->second.video_array.size() <= static_cast<size_t>(index)) {
    return StreamDeviceInfo::kNoId;
  }
  return it->second.video_array[index].session_id;
}

}  // namespace content
