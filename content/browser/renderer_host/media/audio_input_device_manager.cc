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
#include "media/base/scoped_histogram_timer.h"

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
  // TODO(xians): Remove this fake_device after the unittests do not need it.
  StreamDeviceInfo fake_device(MEDIA_DEVICE_AUDIO_CAPTURE,
                               media::AudioManagerBase::kDefaultDeviceName,
                               media::AudioManagerBase::kDefaultDeviceId,
                               44100, media::CHANNEL_LAYOUT_STEREO,
                               0);
  fake_device.session_id = kFakeOpenSessionId;
  devices_.push_back(fake_device);
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
  DCHECK(!device_loop_.get());
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
  if (session_id != kFakeOpenSessionId)
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
  SCOPED_UMA_HISTOGRAM_TIMER(
      "Media.AudioInputDeviceManager.EnumerateOnDeviceThreadTime");
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
        stream_type, it->device_name, it->unique_id));
  }

  // If the |use_fake_device_| flag is on, inject the fake device if there is
  // no available device on the OS.
  if (use_fake_device_ && devices->empty()) {
    devices->push_back(StreamDeviceInfo(
        stream_type, media::AudioManagerBase::kDefaultDeviceName,
        media::AudioManagerBase::kDefaultDeviceId));
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
  SCOPED_UMA_HISTOGRAM_TIMER(
      "Media.AudioInputDeviceManager.OpenOnDeviceThreadTime");
  DCHECK(IsOnDeviceThread());

  StreamDeviceInfo out(info.device.type, info.device.name, info.device.id,
                       0, 0, 0);
  out.session_id = session_id;

  MediaStreamDevice::AudioDeviceParameters& input_params = out.device.input;

  if (use_fake_device_) {
    // Don't need to query the hardware information if using fake device.
    input_params.sample_rate = 44100;
    input_params.channel_layout = media::CHANNEL_LAYOUT_STEREO;
  } else {
    // Get the preferred sample rate and channel configuration for the
    // audio device.
    media::AudioParameters params =
        audio_manager_->GetInputStreamParameters(info.device.id);
    input_params.sample_rate = params.sample_rate();
    input_params.channel_layout = params.channel_layout();
    input_params.frames_per_buffer = params.frames_per_buffer();
    input_params.effects = params.effects();

    // Add preferred output device information if a matching output device
    // exists.
    out.device.matched_output_device_id =
        audio_manager_->GetAssociatedOutputDeviceID(info.device.id);
    if (!out.device.matched_output_device_id.empty()) {
      params = audio_manager_->GetOutputStreamParameters(
          out.device.matched_output_device_id);
      MediaStreamDevice::AudioDeviceParameters& matched_output_params =
          out.device.matched_output;
      matched_output_params.sample_rate = params.sample_rate();
      matched_output_params.channel_layout = params.channel_layout();
      matched_output_params.frames_per_buffer = params.frames_per_buffer();
    }
  }

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
