// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/android/audio_manager_android.h"

#include "base/logging.h"
#include "jni/AudioManagerAndroid_jni.h"
#include "media/audio/android/opensles_input.h"
#include "media/audio/android/opensles_output.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_parameters.h"
#include "media/audio/audio_util.h"
#include "media/audio/fake_audio_input_stream.h"
#include "media/base/channel_layout.h"

namespace media {

// Maximum number of output streams that can be open simultaneously.
static const int kMaxOutputStreams = 10;

static const int kAudioModeNormal = 0x00000000;
static const int kAudioModeInCommunication = 0x00000003;

static const int kDefaultInputBufferSize = 1024;
static const int kDefaultOutputBufferSize = 2048;

AudioManager* CreateAudioManager() {
  return new AudioManagerAndroid();
}

AudioManagerAndroid::AudioManagerAndroid() {
  SetMaxOutputStreamsAllowed(kMaxOutputStreams);

  j_audio_manager_.Reset(
      Java_AudioManagerAndroid_createAudioManagerAndroid(
          base::android::AttachCurrentThread(),
          base::android::GetApplicationContext()));
}

AudioManagerAndroid::~AudioManagerAndroid() {
  Shutdown();
}

bool AudioManagerAndroid::HasAudioOutputDevices() {
  return true;
}

bool AudioManagerAndroid::HasAudioInputDevices() {
  return true;
}

void AudioManagerAndroid::GetAudioInputDeviceNames(
    media::AudioDeviceNames* device_names) {
  DCHECK(device_names->empty());
  device_names->push_front(
      media::AudioDeviceName(kDefaultDeviceName, kDefaultDeviceId));
}

AudioParameters AudioManagerAndroid::GetInputStreamParameters(
    const std::string& device_id) {
  // Use mono as preferred number of input channels on Android to save
  // resources. Using mono also avoids a driver issue seen on Samsung
  // Galaxy S3 and S4 devices. See http://crbug.com/256851 for details.
  ChannelLayout channel_layout = CHANNEL_LAYOUT_MONO;
  int buffer_size = Java_AudioManagerAndroid_getMinInputFrameSize(
      base::android::AttachCurrentThread(), GetNativeOutputSampleRate(),
      ChannelLayoutToChannelCount(channel_layout));

  return AudioParameters(
      AudioParameters::AUDIO_PCM_LOW_LATENCY, channel_layout,
      GetNativeOutputSampleRate(), 16,
      buffer_size <= 0 ? kDefaultInputBufferSize : buffer_size);
}

AudioOutputStream* AudioManagerAndroid::MakeAudioOutputStream(
    const AudioParameters& params, const std::string& input_device_id) {
  AudioOutputStream* stream =
    AudioManagerBase::MakeAudioOutputStream(params, std::string());
  if (stream && output_stream_count() == 1) {
    SetAudioMode(kAudioModeInCommunication);
    RegisterHeadsetReceiver();
  }
  return stream;
}

AudioInputStream* AudioManagerAndroid::MakeAudioInputStream(
    const AudioParameters& params, const std::string& device_id) {
  AudioInputStream* stream =
    AudioManagerBase::MakeAudioInputStream(params, device_id);
  return stream;
}

void AudioManagerAndroid::ReleaseOutputStream(AudioOutputStream* stream) {
  AudioManagerBase::ReleaseOutputStream(stream);
  if (!output_stream_count()) {
    UnregisterHeadsetReceiver();
    SetAudioMode(kAudioModeNormal);
  }
}

void AudioManagerAndroid::ReleaseInputStream(AudioInputStream* stream) {
  AudioManagerBase::ReleaseInputStream(stream);
}

AudioOutputStream* AudioManagerAndroid::MakeLinearOutputStream(
      const AudioParameters& params) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LINEAR, params.format());
  return new OpenSLESOutputStream(this, params);
}

AudioOutputStream* AudioManagerAndroid::MakeLowLatencyOutputStream(
      const AudioParameters& params, const std::string& input_device_id) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());
  return new OpenSLESOutputStream(this, params);
}

AudioInputStream* AudioManagerAndroid::MakeLinearInputStream(
    const AudioParameters& params, const std::string& device_id) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LINEAR, params.format());
  return new OpenSLESInputStream(this, params);
}

AudioInputStream* AudioManagerAndroid::MakeLowLatencyInputStream(
    const AudioParameters& params, const std::string& device_id) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());
  return new OpenSLESInputStream(this, params);
}

int AudioManagerAndroid::GetOptimalOutputFrameSize(int sample_rate,
                                                   int channels) {
  if (IsAudioLowLatencySupported()) {
    return GetAudioLowLatencyOutputFrameSize();
  } else {
    return std::max(kDefaultOutputBufferSize,
                    Java_AudioManagerAndroid_getMinOutputFrameSize(
                        base::android::AttachCurrentThread(),
                        sample_rate, channels));
  }
}

AudioParameters AudioManagerAndroid::GetPreferredOutputStreamParameters(
    const AudioParameters& input_params) {
  ChannelLayout channel_layout = CHANNEL_LAYOUT_STEREO;
  int sample_rate = GetNativeOutputSampleRate();
  int buffer_size = GetOptimalOutputFrameSize(sample_rate, 2);
  int bits_per_sample = 16;
  int input_channels = 0;
  if (input_params.IsValid()) {
    // Use the client's input parameters if they are valid.
    sample_rate = input_params.sample_rate();
    bits_per_sample = input_params.bits_per_sample();
    channel_layout = input_params.channel_layout();
    input_channels = input_params.input_channels();
    buffer_size = GetOptimalOutputFrameSize(
        sample_rate, ChannelLayoutToChannelCount(channel_layout));
  }

  int user_buffer_size = GetUserBufferSize();
  if (user_buffer_size)
    buffer_size = user_buffer_size;

  return AudioParameters(
      AudioParameters::AUDIO_PCM_LOW_LATENCY, channel_layout, input_channels,
      sample_rate, bits_per_sample, buffer_size);
}

// static
bool AudioManagerAndroid::RegisterAudioManager(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void AudioManagerAndroid::SetAudioMode(int mode) {
  Java_AudioManagerAndroid_setMode(
      base::android::AttachCurrentThread(),
      j_audio_manager_.obj(), mode);
}

void AudioManagerAndroid::RegisterHeadsetReceiver() {
  Java_AudioManagerAndroid_registerHeadsetReceiver(
      base::android::AttachCurrentThread(),
      j_audio_manager_.obj());
}

void AudioManagerAndroid::UnregisterHeadsetReceiver() {
  Java_AudioManagerAndroid_unregisterHeadsetReceiver(
      base::android::AttachCurrentThread(),
      j_audio_manager_.obj());
}

int AudioManagerAndroid::GetNativeOutputSampleRate() {
  return Java_AudioManagerAndroid_getNativeOutputSampleRate(
      base::android::AttachCurrentThread(),
      j_audio_manager_.obj());
}

bool AudioManagerAndroid::IsAudioLowLatencySupported() {
  return Java_AudioManagerAndroid_isAudioLowLatencySupported(
      base::android::AttachCurrentThread(),
      j_audio_manager_.obj());
}

int AudioManagerAndroid::GetAudioLowLatencyOutputFrameSize() {
  return Java_AudioManagerAndroid_getAudioLowLatencyOutputFrameSize(
      base::android::AttachCurrentThread(),
      j_audio_manager_.obj());
}

}  // namespace media
