// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_input_device_manager.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/renderer_host/media/audio_input_device_manager_event_handler.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/media_stream_request.h"
#include "media/audio/audio_device_name.h"
#include "media/audio/audio_input_ipc.h"
#include "media/audio/audio_manager_base.h"

using content::BrowserThread;

namespace media_stream {

const int AudioInputDeviceManager::kFakeOpenSessionId = 1;

namespace {
// Starting id for the first capture session.
const int kFirstSessionId = AudioInputDeviceManager::kFakeOpenSessionId + 1;
}

AudioInputDeviceManager::AudioInputDeviceManager(
    media::AudioManager* audio_manager)
    : listener_(NULL),
      next_capture_session_id_(kFirstSessionId),
      audio_manager_(audio_manager) {
}

AudioInputDeviceManager::~AudioInputDeviceManager() {
}

void AudioInputDeviceManager::Register(
    MediaStreamProviderListener* listener,
    base::MessageLoopProxy* device_thread_loop) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!listener_);
  DCHECK(!device_loop_);
  listener_ = listener;
  device_loop_ = device_thread_loop;
}

void AudioInputDeviceManager::Unregister() {
  DCHECK(listener_);
  listener_ = NULL;
}

void AudioInputDeviceManager::EnumerateDevices() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(listener_);

  device_loop_->PostTask(
      FROM_HERE,
      base::Bind(&AudioInputDeviceManager::EnumerateOnDeviceThread, this));
}

int AudioInputDeviceManager::Open(const StreamDeviceInfo& device) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // Generate a new id for this device.
  int session_id = next_capture_session_id_++;
  device_loop_->PostTask(
      FROM_HERE,
      base::Bind(&AudioInputDeviceManager::OpenOnDeviceThread,
                 this, session_id, device));

  return session_id;
}

void AudioInputDeviceManager::Close(int session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(listener_);
  // Check if the device has been stopped, if not, send stop signal.
  EventHandlerMap::iterator it = event_handlers_.find(session_id);
  if (it != event_handlers_.end()) {
    it->second->OnDeviceStopped(session_id);
    event_handlers_.erase(it);
  }

  device_loop_->PostTask(
      FROM_HERE,
      base::Bind(&AudioInputDeviceManager::CloseOnDeviceThread,
                 this, session_id));
}

void AudioInputDeviceManager::Start(
    int session_id, AudioInputDeviceManagerEventHandler* handler) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(handler);

  // Solution for not using MediaStreamManager. This is needed when Start() is
  // called without using Open(), we post default device for test purpose.
  // And we do not store the info for the kFakeOpenSessionId but return
  // the callback immediately.
  if (session_id == kFakeOpenSessionId) {
    handler->OnDeviceStarted(session_id,
                             media::AudioManagerBase::kDefaultDeviceId);
    return;
  }

  // Look up the device_id of the session so we can notify the renderer that
  // the device has started. If the session has not been started,
  // use the empty device_id to indicate that Start() failed.
  std::string device_id;
  if (event_handlers_.insert(std::make_pair(session_id, handler)).second) {
    StreamDeviceMap::const_iterator it = devices_.find(session_id);
    if (it != devices_.end())
      device_id = it->second.device_id;
  }

  // Post a callback through the AudioInputRendererHost to notify the renderer
  // that the device has started.
  handler->OnDeviceStarted(session_id, device_id);
}

void AudioInputDeviceManager::Stop(int session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Erase the event handler referenced by the session_id.
  event_handlers_.erase(session_id);
}

void AudioInputDeviceManager::EnumerateOnDeviceThread() {
  DCHECK(IsOnDeviceThread());
  // AudioManager is guaranteed to outlive MediaStreamManager in
  // BrowserMainloop.
  media::AudioDeviceNames device_names;
  audio_manager_->GetAudioInputDeviceNames(&device_names);

  StreamDeviceInfoArray* devices = new StreamDeviceInfoArray;
  for (media::AudioDeviceNames::iterator it = device_names.begin();
       it != device_names.end();
       ++it) {
    // NOTE: Only support enumeration of the MEDIA_DEVICE_AUDIO_CAPTURE type.
    devices->push_back(StreamDeviceInfo(
        content::MEDIA_DEVICE_AUDIO_CAPTURE, it->device_name,
        it->unique_id, false));
  }

  // Return the device list through the listener by posting a task on
  // IO thread since MediaStreamManager handles the callback asynchronously.
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&AudioInputDeviceManager::DevicesEnumeratedOnIOThread,
                 this,
                 devices));
}

void AudioInputDeviceManager::OpenOnDeviceThread(
    int session_id, const StreamDeviceInfo& device) {
  DCHECK(IsOnDeviceThread());

  // Add the session_id and device to the map.
  if (!devices_.insert(std::make_pair(session_id, device)).second) {
    NOTREACHED();
    devices_[session_id] = device;
  }

  // Return the |session_id| through the listener by posting a task on
  // IO thread since MediaStreamManager handles the callback asynchronously.
  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(&AudioInputDeviceManager::OpenedOnIOThread,
                                     this,
                                     device.stream_type, session_id));
}

void AudioInputDeviceManager::CloseOnDeviceThread(int session_id) {
  DCHECK(IsOnDeviceThread());

  StreamDeviceMap::iterator it = devices_.find(session_id);
  if (it == devices_.end())
    return;
  const content::MediaStreamDeviceType stream_type = it->second.stream_type;
  devices_.erase(it);

  // Post a callback through the listener on IO thread since
  // MediaStreamManager handles the callback asynchronously.
  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(&AudioInputDeviceManager::ClosedOnIOThread,
                                     this,
                                     stream_type, session_id));
}

void AudioInputDeviceManager::DevicesEnumeratedOnIOThread(
    StreamDeviceInfoArray* devices) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // Ensure that |devices| gets deleted on exit.
  scoped_ptr<StreamDeviceInfoArray> devices_array(devices);
  if (listener_) {
    // NOTE: Only support enumeration of the MEDIA_DEVICE_AUDIO_CAPTURE type.
    listener_->DevicesEnumerated(content::MEDIA_DEVICE_AUDIO_CAPTURE,
                                 *devices_array);
  }
}

void AudioInputDeviceManager::OpenedOnIOThread(
    content::MediaStreamDeviceType stream_type, int session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (listener_)
    listener_->Opened(stream_type, session_id);
}

void AudioInputDeviceManager::ClosedOnIOThread(
    content::MediaStreamDeviceType stream_type, int session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (listener_)
    listener_->Closed(stream_type, session_id);
}

bool AudioInputDeviceManager::IsOnDeviceThread() const {
  return device_loop_->BelongsToCurrentThread();
}

}  // namespace media_stream
