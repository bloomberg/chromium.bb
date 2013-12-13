// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/android/audio_manager_android.h"

#include "base/android/build_info.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "jni/AudioManagerAndroid_jni.h"
#include "media/audio/android/audio_record_input.h"
#include "media/audio/android/opensles_input.h"
#include "media/audio/android/opensles_output.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_parameters.h"
#include "media/audio/fake_audio_input_stream.h"
#include "media/base/channel_layout.h"

using base::android::AppendJavaStringArrayToStringVector;
using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

namespace media {

static void AddDefaultDevice(AudioDeviceNames* device_names) {
  DCHECK(device_names->empty());
  device_names->push_front(
      AudioDeviceName(AudioManagerBase::kDefaultDeviceName,
                      AudioManagerBase::kDefaultDeviceId));
}

// Maximum number of output streams that can be open simultaneously.
static const int kMaxOutputStreams = 10;

static const int kAudioModeNormal = 0x00000000;
static const int kAudioModeInCommunication = 0x00000003;

static const int kDefaultInputBufferSize = 1024;
static const int kDefaultOutputBufferSize = 2048;

AudioManager* CreateAudioManager(AudioLogFactory* audio_log_factory) {
  return new AudioManagerAndroid(audio_log_factory);
}

AudioManagerAndroid::AudioManagerAndroid(AudioLogFactory* audio_log_factory)
    : AudioManagerBase(audio_log_factory) {
  SetMaxOutputStreamsAllowed(kMaxOutputStreams);

  j_audio_manager_.Reset(
      Java_AudioManagerAndroid_createAudioManagerAndroid(
          base::android::AttachCurrentThread(),
          base::android::GetApplicationContext(),
          reinterpret_cast<intptr_t>(this)));
  Init();
}

AudioManagerAndroid::~AudioManagerAndroid() {
  Close();
  Shutdown();
}

bool AudioManagerAndroid::HasAudioOutputDevices() {
  return true;
}

bool AudioManagerAndroid::HasAudioInputDevices() {
  return true;
}

void AudioManagerAndroid::GetAudioInputDeviceNames(
    AudioDeviceNames* device_names) {
  // Always add default device parameters as first element.
  AddDefaultDevice(device_names);

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobjectArray> j_device_array =
      Java_AudioManagerAndroid_getAudioInputDeviceNames(
          env, j_audio_manager_.obj());
  jsize len = env->GetArrayLength(j_device_array.obj());
  AudioDeviceName device;
  for (jsize i = 0; i < len; ++i) {
    ScopedJavaLocalRef<jobject> j_device(
        env, env->GetObjectArrayElement(j_device_array.obj(), i));
    ScopedJavaLocalRef<jstring> j_device_name =
        Java_AudioDeviceName_name(env, j_device.obj());
    ConvertJavaStringToUTF8(env, j_device_name.obj(), &device.device_name);
    ScopedJavaLocalRef<jstring> j_device_id =
        Java_AudioDeviceName_id(env, j_device.obj());
    ConvertJavaStringToUTF8(env, j_device_id.obj(), &device.unique_id);
    device_names->push_back(device);
  }
}

void AudioManagerAndroid::GetAudioOutputDeviceNames(
    AudioDeviceNames* device_names) {
  // TODO(henrika): enumerate using GetAudioInputDeviceNames().
  AddDefaultDevice(device_names);
}

AudioParameters AudioManagerAndroid::GetInputStreamParameters(
    const std::string& device_id) {
  JNIEnv* env = AttachCurrentThread();
  // Use mono as preferred number of input channels on Android to save
  // resources. Using mono also avoids a driver issue seen on Samsung
  // Galaxy S3 and S4 devices. See http://crbug.com/256851 for details.
  ChannelLayout channel_layout = CHANNEL_LAYOUT_MONO;
  int buffer_size = Java_AudioManagerAndroid_getMinInputFrameSize(
      env, GetNativeOutputSampleRate(),
      ChannelLayoutToChannelCount(channel_layout));
  int effects = AudioParameters::NO_EFFECTS;
  effects |= Java_AudioManagerAndroid_shouldUseAcousticEchoCanceler(env) ?
      AudioParameters::ECHO_CANCELLER : AudioParameters::NO_EFFECTS;
  AudioParameters params(
      AudioParameters::AUDIO_PCM_LOW_LATENCY, channel_layout, 0,
      GetNativeOutputSampleRate(), 16,
      buffer_size <= 0 ? kDefaultInputBufferSize : buffer_size, effects);
  return params;
}

AudioOutputStream* AudioManagerAndroid::MakeAudioOutputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const std::string& input_device_id) {
  AudioOutputStream* stream =
      AudioManagerBase::MakeAudioOutputStream(params, std::string(),
          std::string());
  if (stream && output_stream_count() == 1) {
    SetAudioMode(kAudioModeInCommunication);
  }

  {
    base::AutoLock lock(streams_lock_);
    streams_.insert(static_cast<OpenSLESOutputStream*>(stream));
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
    SetAudioMode(kAudioModeNormal);
  }
  base::AutoLock lock(streams_lock_);
  streams_.erase(static_cast<OpenSLESOutputStream*>(stream));
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
    const AudioParameters& params,
    const std::string& device_id,
    const std::string& input_device_id) {
  DLOG_IF(ERROR, !device_id.empty()) << "Not implemented!";
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());
  return new OpenSLESOutputStream(this, params);
}

AudioInputStream* AudioManagerAndroid::MakeLinearInputStream(
    const AudioParameters& params, const std::string& device_id) {
  // TODO(henrika): add support for device selection if/when any client
  // needs it.
  DLOG_IF(ERROR, !device_id.empty()) << "Not implemented!";
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LINEAR, params.format());
  return new OpenSLESInputStream(this, params);
}

AudioInputStream* AudioManagerAndroid::MakeLowLatencyInputStream(
    const AudioParameters& params, const std::string& device_id) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());
  DLOG_IF(ERROR, device_id.empty()) << "Invalid device ID!";
  // Utilize the device ID to select the correct input device.
  // Note that the input device is always associated with a certain output
  // device, i.e., this selection does also switch the output device.
  // All input and output streams will be affected by the device selection.
  SetAudioDevice(device_id);

  if (params.effects() != AudioParameters::NO_EFFECTS) {
    // Platform effects can only be enabled through the AudioRecord path.
    // An effect should only have been requested here if recommended by
    // AudioManagerAndroid.shouldUse<Effect>.
    //
    // Creating this class requires Jelly Bean, which is already guaranteed by
    // shouldUse<Effect>. Only DCHECK on that condition to allow tests to use
    // the effect settings as a way to select the input path.
    DCHECK_GE(base::android::BuildInfo::GetInstance()->sdk_int(), 16);
    DVLOG(1) << "Creating AudioRecordInputStream";
    return new AudioRecordInputStream(this, params);
  }
  DVLOG(1) << "Creating OpenSLESInputStream";
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
    const std::string& output_device_id,
    const AudioParameters& input_params) {
  // TODO(tommi): Support |output_device_id|.
  DLOG_IF(ERROR, !output_device_id.empty()) << "Not implemented!";
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
      sample_rate, bits_per_sample, buffer_size, AudioParameters::NO_EFFECTS);
}

// static
bool AudioManagerAndroid::RegisterAudioManager(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void AudioManagerAndroid::Init() {
  Java_AudioManagerAndroid_init(
      base::android::AttachCurrentThread(),
      j_audio_manager_.obj());
}

void AudioManagerAndroid::Close() {
  Java_AudioManagerAndroid_close(
      base::android::AttachCurrentThread(),
      j_audio_manager_.obj());
}

void AudioManagerAndroid::SetMute(JNIEnv* env, jobject obj, jboolean muted) {
  GetMessageLoop()->PostTask(
      FROM_HERE,
      base::Bind(
          &AudioManagerAndroid::DoSetMuteOnAudioThread,
          base::Unretained(this),
          muted));
}

void AudioManagerAndroid::DoSetMuteOnAudioThread(bool muted) {
  base::AutoLock lock(streams_lock_);
  for (OutputStreams::iterator it = streams_.begin();
       it != streams_.end(); ++it) {
    (*it)->SetMute(muted);
  }
}

void AudioManagerAndroid::SetAudioMode(int mode) {
  Java_AudioManagerAndroid_setMode(
      base::android::AttachCurrentThread(),
      j_audio_manager_.obj(), mode);
}

void AudioManagerAndroid::SetAudioDevice(const std::string& device_id) {
  JNIEnv* env = AttachCurrentThread();

  // Send the unique device ID to the Java audio manager and make the
  // device switch. Provide an empty string to the Java audio manager
  // if the default device is selected.
  ScopedJavaLocalRef<jstring> j_device_id = ConvertUTF8ToJavaString(
      env,
      device_id == AudioManagerBase::kDefaultDeviceId ?
          std::string() : device_id);
  Java_AudioManagerAndroid_setDevice(
      env, j_audio_manager_.obj(), j_device_id.obj());
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
