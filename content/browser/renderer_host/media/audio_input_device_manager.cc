// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_input_device_manager.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/media_stream_request.h"
#include "media/audio/audio_device_name.h"
#include "media/audio/audio_input_ipc.h"
#include "media/audio/audio_manager_base.h"
#include "media/audio/audio_parameters.h"
#include "media/base/channel_layout.h"

namespace content {

const int AudioInputDeviceManager::kFakeOpenSessionId = 1;

namespace {
// Starting id for the first capture session.
const int kFirstSessionId = AudioInputDeviceManager::kFakeOpenSessionId + 1;
}

AudioInputDeviceManager::AudioInputDeviceManager(
    media::AudioManager* audio_manager)
    : listener_(NULL),
      next_capture_session_id_(kFirstSessionId),
      use_fake_device_(false),
      audio_manager_(audio_manager) {
}

AudioInputDeviceManager::~AudioInputDeviceManager() {
}

const StreamDeviceInfo* AudioInputDeviceManager::GetOpenedDeviceInfoById(
    int session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  StreamDeviceList::iterator device = GetDevice(session_id);
  if (device == devices_.end())
    return NULL;

  return &(*device);
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

void AudioInputDeviceManager::EnumerateDevices(MediaStreamType stream_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(listener_);

  device_loop_->PostTask(
      FROM_HERE,
      base::Bind(&AudioInputDeviceManager::EnumerateOnDeviceThread,
                 this, stream_type));
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
  StreamDeviceList::iterator device = GetDevice(session_id);
  if (device == devices_.end())
    return;
  const MediaStreamType stream_type = device->device.type;
  devices_.erase(device);

  // Post a callback through the listener on IO thread since
  // MediaStreamManager is expecting the callback asynchronously.
  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(&AudioInputDeviceManager::ClosedOnIOThread,
                                     this, stream_type, session_id));
}

void AudioInputDeviceManager::UseFakeDevice() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  use_fake_device_ = true;
}

bool AudioInputDeviceManager::ShouldUseFakeDevice() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return use_fake_device_;
}

void AudioInputDeviceManager::EnumerateOnDeviceThread(
    MediaStreamType stream_type) {
  DCHECK(IsOnDeviceThread());

  media::AudioDeviceNames device_names;

  switch (stream_type) {
    case MEDIA_DEVICE_AUDIO_CAPTURE:
      // AudioManager is guaranteed to outlive MediaStreamManager in
      // BrowserMainloop.
      audio_manager_->GetAudioInputDeviceNames(&device_names);
      break;

    default:
      NOTREACHED();
      break;
  }

  scoped_ptr<StreamDeviceInfoArray> devices(new StreamDeviceInfoArray());
  for (media::AudioDeviceNames::iterator it = device_names.begin();
       it != device_names.end(); ++it) {
    // Add device information to device vector.
    devices->push_back(StreamDeviceInfo(
        stream_type, it->device_name, it->unique_id, false));
  }

  // Return the device list through the listener by posting a task on
  // IO thread since MediaStreamManager handles the callback asynchronously.
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&AudioInputDeviceManager::DevicesEnumeratedOnIOThread,
                 this, stream_type, base::Passed(&devices)));
}

void AudioInputDeviceManager::OpenOnDeviceThread(
    int session_id, const StreamDeviceInfo& info) {
  DCHECK(IsOnDeviceThread());

  // Get the preferred sample rate and channel configuration for the
  // audio device.
  media::AudioParameters params =
      audio_manager_->GetInputStreamParameters(info.device.id);

  StreamDeviceInfo out(info.device.type, info.device.name, info.device.id,
                       params.sample_rate(), params.channel_layout(), false);
  out.session_id = session_id;

  // Return the |session_id| through the listener by posting a task on
  // IO thread since MediaStreamManager handles the callback asynchronously.
  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(&AudioInputDeviceManager::OpenedOnIOThread,
                                     this, session_id, out));
}

void AudioInputDeviceManager::DevicesEnumeratedOnIOThread(
    MediaStreamType stream_type,
    scoped_ptr<StreamDeviceInfoArray> devices) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // Ensure that |devices| gets deleted on exit.
  if (listener_)
    listener_->DevicesEnumerated(stream_type, *devices);
}

void AudioInputDeviceManager::OpenedOnIOThread(int session_id,
                                               const StreamDeviceInfo& info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK_EQ(session_id, info.session_id);
  DCHECK(GetDevice(session_id) == devices_.end());
  devices_.push_back(info);

  if (listener_)
    listener_->Opened(info.device.type, session_id);
}

void AudioInputDeviceManager::ClosedOnIOThread(MediaStreamType stream_type,
                                               int session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (listener_)
    listener_->Closed(stream_type, session_id);
}

bool AudioInputDeviceManager::IsOnDeviceThread() const {
  return device_loop_->BelongsToCurrentThread();
}

AudioInputDeviceManager::StreamDeviceList::iterator
AudioInputDeviceManager::GetDevice(int session_id) {
  for (StreamDeviceList::iterator i(devices_.begin()); i != devices_.end();
       ++i) {
    if (i->session_id == session_id)
      return i;
  }

  return devices_.end();
}

}  // namespace content
