// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_input_device_manager.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/media_stream_request.h"
#include "media/audio/audio_input_ipc.h"
#include "media/audio/audio_system.h"
#include "media/base/audio_parameters.h"
#include "media/base/channel_layout.h"
#include "media/base/media_switches.h"

#if defined(OS_CHROMEOS)
#include "chromeos/audio/cras_audio_handler.h"
#endif

namespace content {

const int AudioInputDeviceManager::kFakeOpenSessionId = 1;

namespace {
// Starting id for the first capture session.
const int kFirstSessionId = AudioInputDeviceManager::kFakeOpenSessionId + 1;
}

AudioInputDeviceManager::AudioInputDeviceManager(
    media::AudioSystem* audio_system)
    : next_capture_session_id_(kFirstSessionId),
#if defined(OS_CHROMEOS)
      keyboard_mic_streams_count_(0),
#endif
      audio_system_(audio_system) {
}

AudioInputDeviceManager::~AudioInputDeviceManager() {
}

const StreamDeviceInfo* AudioInputDeviceManager::GetOpenedDeviceInfoById(
    int session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  StreamDeviceList::iterator device = GetDevice(session_id);
  if (device == devices_.end())
    return nullptr;

  return &(*device);
}

void AudioInputDeviceManager::RegisterListener(
    MediaStreamProviderListener* listener) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(listener);
  listeners_.AddObserver(listener);
}

void AudioInputDeviceManager::UnregisterListener(
    MediaStreamProviderListener* listener) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(listener);
  listeners_.RemoveObserver(listener);
}

int AudioInputDeviceManager::Open(const MediaStreamDevice& device) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Generate a new id for this device.
  int session_id = next_capture_session_id_++;

  // base::Unretained(this) is safe, because AudioInputDeviceManager is
  // destroyed not earlier than on the IO message loop destruction.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseFakeDeviceForMediaStream)) {
    audio_system_->GetAssociatedOutputDeviceID(
        device.id,
        base::BindOnce(&AudioInputDeviceManager::OpenedOnIOThread,
                       base::Unretained(this), session_id, device,
                       base::TimeTicks::Now(),
                       media::AudioParameters::UnavailableDeviceParams(),
                       media::AudioParameters::UnavailableDeviceParams()));
  } else {
    // TODO(tommi): As is, we hit this code path when device.type is
    // MEDIA_TAB_AUDIO_CAPTURE and the device id is not a device that
    // the AudioManager can know about. This currently does not fail because
    // the implementation of GetInputStreamParameters returns valid parameters
    // by default for invalid devices. That behavior is problematic because it
    // causes other parts of the code to attempt to open truly invalid or
    // missing devices and falling back on alternate devices (and likely fail
    // twice in a row). Tab audio capture should not pass through here and
    // GetInputStreamParameters should return invalid parameters for invalid
    // devices.

    audio_system_->GetInputDeviceInfo(
        device.id, base::BindOnce(&AudioInputDeviceManager::OpenedOnIOThread,
                                  base::Unretained(this), session_id, device,
                                  base::TimeTicks::Now()));
  }

  return session_id;
}

void AudioInputDeviceManager::Close(int session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  StreamDeviceList::iterator device = GetDevice(session_id);
  if (device == devices_.end())
    return;
  const MediaStreamType stream_type = device->device.type;
  if (session_id != kFakeOpenSessionId)
    devices_.erase(device);

  // Post a callback through the listener on IO thread since
  // MediaStreamManager is expecting the callback asynchronously.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&AudioInputDeviceManager::ClosedOnIOThread, this,
                     stream_type, session_id));
}

#if defined(OS_CHROMEOS)
void AudioInputDeviceManager::RegisterKeyboardMicStream(
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  ++keyboard_mic_streams_count_;
  if (keyboard_mic_streams_count_ == 1) {
    BrowserThread::PostTaskAndReply(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(
            &AudioInputDeviceManager::SetKeyboardMicStreamActiveOnUIThread,
            this, true),
        std::move(callback));
  } else {
    std::move(callback).Run();
  }
}

void AudioInputDeviceManager::UnregisterKeyboardMicStream() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  --keyboard_mic_streams_count_;
  DCHECK_GE(keyboard_mic_streams_count_, 0);
  if (keyboard_mic_streams_count_ == 0) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(
            &AudioInputDeviceManager::SetKeyboardMicStreamActiveOnUIThread,
            this, false));
  }
}
#endif

void AudioInputDeviceManager::OpenedOnIOThread(
    int session_id,
    const MediaStreamDevice& device,
    base::TimeTicks start_time,
    const media::AudioParameters& input_params,
    const media::AudioParameters& matched_output_params,
    const std::string& matched_output_device_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(GetDevice(session_id) == devices_.end());

  UMA_HISTOGRAM_TIMES("Media.AudioInputDeviceManager.OpenOnDeviceThreadTime",
                      base::TimeTicks::Now() - start_time);

  StreamDeviceInfo info(device.type, device.name, device.id);
  info.session_id = session_id;
  info.device.input.sample_rate = input_params.sample_rate();
  info.device.input.channel_layout = input_params.channel_layout();
  info.device.input.frames_per_buffer = input_params.frames_per_buffer();
  info.device.input.effects = input_params.effects();
  info.device.input.mic_positions = input_params.mic_positions();
  info.device.matched_output_device_id = matched_output_device_id;
  info.device.matched_output.sample_rate = matched_output_params.sample_rate();
  info.device.matched_output.channel_layout =
      matched_output_params.channel_layout();
  info.device.matched_output.frames_per_buffer =
      matched_output_params.frames_per_buffer();
  info.device.matched_output.effects = matched_output_params.effects();

  devices_.push_back(info);

  for (auto& listener : listeners_)
    listener.Opened(info.device.type, session_id);
}

void AudioInputDeviceManager::ClosedOnIOThread(MediaStreamType stream_type,
                                               int session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  for (auto& listener : listeners_)
    listener.Closed(stream_type, session_id);
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

#if defined(OS_CHROMEOS)
void AudioInputDeviceManager::SetKeyboardMicStreamActiveOnUIThread(
    bool active) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  chromeos::CrasAudioHandler::Get()->SetKeyboardMicActive(active);
}
#endif

}  // namespace content
