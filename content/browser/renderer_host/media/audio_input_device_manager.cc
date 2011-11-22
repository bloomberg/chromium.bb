// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_input_device_manager.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/renderer_host/media/audio_input_device_manager_event_handler.h"
#include "content/public/browser/browser_thread.h"
#include "media/audio/audio_manager_base.h"

using content::BrowserThread;

namespace media_stream {

const int AudioInputDeviceManager::kFakeOpenSessionId = 1;
const int AudioInputDeviceManager::kInvalidSessionId = 0;
const char AudioInputDeviceManager::kInvalidDeviceId[] = "";

// Starting id for the first capture session.
const int kFirstSessionId = AudioInputDeviceManager::kFakeOpenSessionId + 1;

// Helper function.
static bool IsValidAudioInputDevice(const media::AudioDeviceName& device) {
  AudioManager* audio_manager = AudioManager::GetAudioManager();
  if (!audio_manager)
    return false;

  // Get the up-to-date list of devices and verify the device is in the list.
  media::AudioDeviceNames device_names;
  audio_manager->GetAudioInputDeviceNames(&device_names);
  if (!device_names.empty()) {
    for (media::AudioDeviceNames::iterator iter = device_names.begin();
         iter != device_names.end();
         ++iter) {
      if (iter->device_name == device.device_name &&
          iter->unique_id == device.unique_id)
        return true;
    }
  }
  // The device wasn't found.
  return false;
}

AudioInputDeviceManager::AudioInputDeviceManager()
    : audio_input_device_thread_("AudioInputDeviceManagerThread"),
      listener_(NULL),
      next_capture_session_id_(kFirstSessionId) {
  audio_input_device_thread_.Start();
}

AudioInputDeviceManager::~AudioInputDeviceManager() {
  audio_input_device_thread_.Stop();
}

void AudioInputDeviceManager::Register(MediaStreamProviderListener* listener) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!listener_);
  listener_ = listener;
}

void AudioInputDeviceManager::Unregister() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(listener_);
  listener_ = NULL;
}

void AudioInputDeviceManager::EnumerateDevices() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(listener_);

  audio_input_device_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&AudioInputDeviceManager::EnumerateOnDeviceThread,
                 base::Unretained(this)));
}

int AudioInputDeviceManager::Open(const StreamDeviceInfo& device) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Generate a new id for this device.
  int audio_input_session_id = next_capture_session_id_++;

  audio_input_device_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&AudioInputDeviceManager::OpenOnDeviceThread,
                 base::Unretained(this), audio_input_session_id, device));

  return audio_input_session_id;
}

void AudioInputDeviceManager::Close(int session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(listener_);

  audio_input_device_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&AudioInputDeviceManager::CloseOnDeviceThread,
                 base::Unretained(this), session_id));
}

void AudioInputDeviceManager::Start(
    int session_id, AudioInputDeviceManagerEventHandler* event_handler) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(event_handler);

  // Solution for not using MediaStreamManager. This is needed when Start() is
  // called without using Open(), we post default device for test purpose.
  // And we do not store the info for the kFakeOpenSessionId but return
  // the callback immediately.
  if (session_id == kFakeOpenSessionId) {
    event_handler->OnDeviceStarted(session_id,
                                   AudioManagerBase::kDefaultDeviceId);
    return;
  }

  // If session has been started, post a callback with an error.
  if (event_handlers_.find(session_id) != event_handlers_.end()) {
    // Session has been started, post a callback with error.
    event_handler->OnDeviceStarted(session_id, kInvalidDeviceId);
    return;
  }

  // Add the event handler to the session.
  event_handlers_.insert(std::make_pair(session_id, event_handler));

  audio_input_device_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&AudioInputDeviceManager::StartOnDeviceThread,
                 base::Unretained(this), session_id));
}

void AudioInputDeviceManager::Stop(int session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  audio_input_device_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&AudioInputDeviceManager::StopOnDeviceThread,
                 base::Unretained(this), session_id));
}

void AudioInputDeviceManager::EnumerateOnDeviceThread() {
  DCHECK(IsOnCaptureDeviceThread());

  // Get the device list from system.
  media::AudioDeviceNames device_names;
  AudioManager::GetAudioManager()->GetAudioInputDeviceNames(&device_names);

  StreamDeviceInfoArray devices;
  for (media::AudioDeviceNames::iterator it = device_names.begin();
       it != device_names.end();
       ++it) {
    devices.push_back(StreamDeviceInfo(kAudioCapture, it->device_name,
                                       it->unique_id, false));
  }

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&AudioInputDeviceManager::DevicesEnumeratedOnIOThread,
                 base::Unretained(this), devices));
}

void AudioInputDeviceManager::OpenOnDeviceThread(
    int session_id, const StreamDeviceInfo& device) {
  DCHECK(IsOnCaptureDeviceThread());
  DCHECK(devices_.find(session_id) == devices_.end());

  media::AudioDeviceName audio_input_device_name;
  audio_input_device_name.device_name = device.name;
  audio_input_device_name.unique_id = device.device_id;

  // Check if the device is valid
  if (!IsValidAudioInputDevice(audio_input_device_name)) {
    SignalError(session_id, kDeviceNotAvailable);
    return;
  }

  // Add the session_id and device to the list.
  devices_[session_id] = audio_input_device_name;
  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(
                              &AudioInputDeviceManager::OpenedOnIOThread,
                              base::Unretained(this),
                              session_id));
}

void AudioInputDeviceManager::CloseOnDeviceThread(int session_id) {
  DCHECK(IsOnCaptureDeviceThread());
  DCHECK(devices_.find(session_id) != devices_.end());

  devices_.erase(session_id);
  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(
                              &AudioInputDeviceManager::ClosedOnIOThread,
                              base::Unretained(this),
                              session_id));
}

void AudioInputDeviceManager::StartOnDeviceThread(const int session_id) {
  DCHECK(IsOnCaptureDeviceThread());

  // Get the up-to-date device enumeration list from the OS and find out
  // the unique id of the device.
  std::string device_id = kInvalidDeviceId;
  AudioInputDeviceMap::const_iterator it = devices_.find(session_id);
  if (it != devices_.end()) {
    media::AudioDeviceNames device_names;
    AudioManager::GetAudioManager()->GetAudioInputDeviceNames(&device_names);
    if (!device_names.empty()) {
      int index = 0;
      for (media::AudioDeviceNames::iterator iter = device_names.begin();
           iter != device_names.end();
           ++iter, ++index) {
        if (iter->device_name == it->second.device_name &&
            iter->unique_id == it->second.unique_id) {
          // Found the device.
          device_id = iter->unique_id;
          break;
        }
      }
    }
  }

  // Posts the index to AudioInputRenderHost through the event handler.
  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(
                              &AudioInputDeviceManager::StartedOnIOThread,
                              base::Unretained(this),
                              session_id,
                              device_id));
}

void AudioInputDeviceManager::StopOnDeviceThread(int session_id) {
  DCHECK(IsOnCaptureDeviceThread());
  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(
                              &AudioInputDeviceManager::StoppedOnIOThread,
                              base::Unretained(this),
                              session_id));
}

void AudioInputDeviceManager::DevicesEnumeratedOnIOThread(
    const StreamDeviceInfoArray& devices) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (listener_)
    listener_->DevicesEnumerated(kAudioCapture, devices);
}

void AudioInputDeviceManager::OpenedOnIOThread(int session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (listener_)
    listener_->Opened(kAudioCapture, session_id);
}

void AudioInputDeviceManager::ClosedOnIOThread(int session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  EventHandlerMap::iterator it = event_handlers_.find(session_id);
  if (it != event_handlers_.end()) {
    // The device hasn't been stopped, send stop signal.
    it->second->OnDeviceStopped(session_id);
    event_handlers_.erase(session_id);
  }
  listener_->Closed(kAudioCapture, session_id);
}


void AudioInputDeviceManager::ErrorOnIOThread(int session_id,
                                              MediaStreamProviderError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (listener_)
    listener_->Error(kAudioCapture, session_id, error);
}

void AudioInputDeviceManager::StartedOnIOThread(
    int session_id, const std::string& device_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  EventHandlerMap::iterator it = event_handlers_.find(session_id);
  if (it == event_handlers_.end())
    return;

  // Post a callback through the AudioInputRendererHost to notify the renderer
  // device has been started.
  it->second->OnDeviceStarted(session_id, device_id);
}

void AudioInputDeviceManager::StoppedOnIOThread(int session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // Erase the event handler referenced by the session_id.
  event_handlers_.erase(session_id);
}

void AudioInputDeviceManager::SignalError(int session_id,
                                          MediaStreamProviderError error) {
  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(
                              &AudioInputDeviceManager::ErrorOnIOThread,
                              base::Unretained(this),
                              session_id,
                              error));
}

bool AudioInputDeviceManager::IsOnCaptureDeviceThread() const {
  return MessageLoop::current() == audio_input_device_thread_.message_loop();
}

void AudioInputDeviceManager::UnregisterEventHandler(int session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  event_handlers_.erase(session_id);
}

bool AudioInputDeviceManager::HasEventHandler(int session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return event_handlers_.find(session_id) != event_handlers_.end();
}

MessageLoop* AudioInputDeviceManager::message_loop() {
  return audio_input_device_thread_.message_loop();
}

}  // namespace media_stream
