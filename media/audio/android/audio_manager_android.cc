// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/android/audio_manager_android.h"

#include <memory>

#include "base/android/build_info.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "jni/AudioManagerAndroid_jni.h"
#include "media/audio/android/audio_record_input.h"
#include "media/audio/android/audio_track_output_stream.h"
#include "media/audio/android/opensles_input.h"
#include "media/audio/android/opensles_output.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_manager.h"
#include "media/audio/fake_audio_input_stream.h"
#include "media/base/audio_parameters.h"
#include "media/base/channel_layout.h"

using base::android::AppendJavaStringArrayToStringVector;
using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace media {
namespace {

void AddDefaultDevice(AudioDeviceNames* device_names) {
  DCHECK(device_names->empty());
  device_names->push_front(AudioDeviceName::CreateDefault());
}

// Maximum number of output streams that can be open simultaneously.
const int kMaxOutputStreams = 10;

const int kDefaultInputBufferSize = 1024;
const int kDefaultOutputBufferSize = 2048;

}  // namespace

std::unique_ptr<AudioManager> CreateAudioManager(
    std::unique_ptr<AudioThread> audio_thread,
    AudioLogFactory* audio_log_factory) {
  return std::make_unique<AudioManagerAndroid>(std::move(audio_thread),
                                               audio_log_factory);
}

AudioManagerAndroid::AudioManagerAndroid(
    std::unique_ptr<AudioThread> audio_thread,
    AudioLogFactory* audio_log_factory)
    : AudioManagerBase(std::move(audio_thread), audio_log_factory),
      communication_mode_is_on_(false),
      output_volume_override_set_(false),
      output_volume_override_(0) {
  SetMaxOutputStreamsAllowed(kMaxOutputStreams);
}

AudioManagerAndroid::~AudioManagerAndroid() = default;

void AudioManagerAndroid::InitializeIfNeeded() {
  GetTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(base::IgnoreResult(&AudioManagerAndroid::GetJavaAudioManager),
                 base::Unretained(this)));
}

void AudioManagerAndroid::ShutdownOnAudioThread() {
  AudioManagerBase::ShutdownOnAudioThread();

  // Destory java android manager here because it can only be accessed on the
  // audio thread.
  if (!j_audio_manager_.is_null()) {
    DVLOG(2) << "Destroying Java part of the audio manager";
    Java_AudioManagerAndroid_close(base::android::AttachCurrentThread(),
                                   j_audio_manager_);
    j_audio_manager_.Reset();
  }
}

bool AudioManagerAndroid::HasAudioOutputDevices() {
  return true;
}

bool AudioManagerAndroid::HasAudioInputDevices() {
  return true;
}

void AudioManagerAndroid::GetAudioInputDeviceNames(
    AudioDeviceNames* device_names) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());

  // Always add default device parameters as first element.
  DCHECK(device_names->empty());
  AddDefaultDevice(device_names);

  // Get list of available audio devices.
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobjectArray> j_device_array =
      Java_AudioManagerAndroid_getAudioInputDeviceNames(env,
                                                        GetJavaAudioManager());
  if (j_device_array.is_null()) {
    // Most probable reason for a NULL result here is that the process lacks
    // MODIFY_AUDIO_SETTINGS or RECORD_AUDIO permissions.
    return;
  }
  jsize len = env->GetArrayLength(j_device_array.obj());
  AudioDeviceName device;
  for (jsize i = 0; i < len; ++i) {
    ScopedJavaLocalRef<jobject> j_device(
        env, env->GetObjectArrayElement(j_device_array.obj(), i));
    ScopedJavaLocalRef<jstring> j_device_name =
        Java_AudioDeviceName_name(env, j_device);
    ConvertJavaStringToUTF8(env, j_device_name.obj(), &device.device_name);
    ScopedJavaLocalRef<jstring> j_device_id =
        Java_AudioDeviceName_id(env, j_device);
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
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());

  // Use mono as preferred number of input channels on Android to save
  // resources. Using mono also avoids a driver issue seen on Samsung
  // Galaxy S3 and S4 devices. See http://crbug.com/256851 for details.
  JNIEnv* env = AttachCurrentThread();
  ChannelLayout channel_layout = CHANNEL_LAYOUT_MONO;
  int buffer_size = Java_AudioManagerAndroid_getMinInputFrameSize(
      env, GetNativeOutputSampleRate(),
      ChannelLayoutToChannelCount(channel_layout));
  buffer_size = buffer_size <= 0 ? kDefaultInputBufferSize : buffer_size;
  int effects = AudioParameters::NO_EFFECTS;
  effects |= Java_AudioManagerAndroid_shouldUseAcousticEchoCanceler(env) ?
      AudioParameters::ECHO_CANCELLER : AudioParameters::NO_EFFECTS;

  int user_buffer_size = GetUserBufferSize();
  if (user_buffer_size)
    buffer_size = user_buffer_size;

  AudioParameters params(AudioParameters::AUDIO_PCM_LOW_LATENCY, channel_layout,
                         GetNativeOutputSampleRate(), 16, buffer_size);
  params.set_effects(effects);
  return params;
}

const char* AudioManagerAndroid::GetName() {
  return "Android";
}

AudioOutputStream* AudioManagerAndroid::MakeAudioOutputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  AudioOutputStream* stream = AudioManagerBase::MakeAudioOutputStream(
      params, std::string(), AudioManager::LogCallback());
  if (stream)
    streams_.insert(static_cast<MuteableAudioOutputStream*>(stream));
  return stream;
}

AudioInputStream* AudioManagerAndroid::MakeAudioInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  bool has_no_input_streams = HasNoAudioInputStreams();
  AudioInputStream* stream = AudioManagerBase::MakeAudioInputStream(
      params, device_id, AudioManager::LogCallback());

  // The audio manager for Android creates streams intended for real-time
  // VoIP sessions and therefore sets the audio mode to MODE_IN_COMMUNICATION.
  // If a Bluetooth headset is used, the audio stream will use the SCO
  // channel and therefore have a limited bandwidth (8kHz).
  if (stream && has_no_input_streams) {
    communication_mode_is_on_ = true;
    SetCommunicationAudioModeOn(true);
  }
  return stream;
}

void AudioManagerAndroid::ReleaseOutputStream(AudioOutputStream* stream) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  streams_.erase(static_cast<MuteableAudioOutputStream*>(stream));
  AudioManagerBase::ReleaseOutputStream(stream);
}

void AudioManagerAndroid::ReleaseInputStream(AudioInputStream* stream) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  AudioManagerBase::ReleaseInputStream(stream);

  // Restore the audio mode which was used before the first communication-
  // mode stream was created.
  if (HasNoAudioInputStreams()) {
    communication_mode_is_on_ = false;
    SetCommunicationAudioModeOn(false);
  }
}

AudioOutputStream* AudioManagerAndroid::MakeLinearOutputStream(
    const AudioParameters& params,
    const LogCallback& log_callback) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LINEAR, params.format());
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  return new OpenSLESOutputStream(this, params, SL_ANDROID_STREAM_MEDIA);
}

AudioOutputStream* AudioManagerAndroid::MakeLowLatencyOutputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DLOG_IF(ERROR, !device_id.empty()) << "Not implemented!";
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());

  // Set stream type which matches the current system-wide audio mode used by
  // the Android audio manager.
  const SLint32 stream_type = communication_mode_is_on_ ?
      SL_ANDROID_STREAM_VOICE : SL_ANDROID_STREAM_MEDIA;
  return new OpenSLESOutputStream(this, params, stream_type);
}

AudioOutputStream* AudioManagerAndroid::MakeBitstreamOutputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK(params.IsBitstreamFormat());
  return new AudioTrackOutputStream(this, params);
}

AudioInputStream* AudioManagerAndroid::MakeLinearInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  // TODO(henrika): add support for device selection if/when any client
  // needs it.
  DLOG_IF(ERROR, !device_id.empty()) << "Not implemented!";
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LINEAR, params.format());
  return new OpenSLESInputStream(this, params);
}

AudioInputStream* AudioManagerAndroid::MakeLowLatencyInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());
  DLOG_IF(ERROR, device_id.empty()) << "Invalid device ID!";

  // Use the device ID to select the correct input device.
  // Note that the input device is always associated with a certain output
  // device, i.e., this selection does also switch the output device.
  // All input and output streams will be affected by the device selection.
  if (!SetAudioDevice(device_id)) {
    LOG(ERROR) << "Unable to select audio device!";
    return NULL;
  }

  if (params.effects() != AudioParameters::NO_EFFECTS) {
    // Platform effects can only be enabled through the AudioRecord path.
    // An effect should only have been requested here if recommended by
    // AudioManagerAndroid#shouldUse...().
    DVLOG(1) << "Creating AudioRecordInputStream";
    return new AudioRecordInputStream(this, params);
  }
  DVLOG(1) << "Creating OpenSLESInputStream";
  return new OpenSLESInputStream(this, params);
}

// static
bool AudioManagerAndroid::SupportsPerformanceModeForOutput() {
  return base::android::BuildInfo::GetInstance()->sdk_int() >=
         base::android::SDK_VERSION_NOUGAT_MR1;
}

void AudioManagerAndroid::SetMute(JNIEnv* env,
                                  const JavaParamRef<jobject>& obj,
                                  jboolean muted) {
  GetTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(
          &AudioManagerAndroid::DoSetMuteOnAudioThread,
          base::Unretained(this),
          muted));
}

void AudioManagerAndroid::SetOutputVolumeOverride(double volume) {
  GetTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(
          &AudioManagerAndroid::DoSetVolumeOnAudioThread,
          base::Unretained(this),
          volume));
}

bool AudioManagerAndroid::HasOutputVolumeOverride(double* out_volume) const {
  if (output_volume_override_set_) {
    *out_volume = output_volume_override_;
  }
  return output_volume_override_set_;
}

AudioParameters AudioManagerAndroid::GetPreferredOutputStreamParameters(
    const std::string& output_device_id,
    const AudioParameters& input_params) {
  // TODO(tommi): Support |output_device_id|.
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  DLOG_IF(ERROR, !output_device_id.empty()) << "Not implemented!";
  ChannelLayout channel_layout = CHANNEL_LAYOUT_STEREO;
  int sample_rate = GetNativeOutputSampleRate();
  int buffer_size = GetOptimalOutputFrameSize(sample_rate, 2);
  int bits_per_sample = 16;
  if (input_params.IsValid()) {
    // Use the client's input parameters if they are valid.
    sample_rate = input_params.sample_rate();
    bits_per_sample = input_params.bits_per_sample();

    // Pre-Lollipop devices don't support > stereo OpenSLES output and the
    // AudioManager APIs for GetOptimalOutputFrameSize() don't support channel
    // layouts greater than stereo unless low latency audio is supported.
    if (input_params.channels() <= 2 ||
        (base::android::BuildInfo::GetInstance()->sdk_int() >=
             base::android::SDK_VERSION_LOLLIPOP &&
         IsAudioLowLatencySupported())) {
      channel_layout = input_params.channel_layout();
    }

    // For high latency playback on supported platforms, pass through the
    // requested buffer size; this provides significant power savings (~25%) and
    // reduces the potential for glitches under load.
    if (SupportsPerformanceModeForOutput() &&
        input_params.latency_tag() == AudioLatency::LATENCY_PLAYBACK) {
      buffer_size = input_params.frames_per_buffer();
    } else {
      buffer_size = GetOptimalOutputFrameSize(
          sample_rate, ChannelLayoutToChannelCount(channel_layout));
    }
  }

  int user_buffer_size = GetUserBufferSize();
  if (user_buffer_size)
    buffer_size = user_buffer_size;

  return AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY, channel_layout,
                         sample_rate, bits_per_sample, buffer_size);
}

bool AudioManagerAndroid::HasNoAudioInputStreams() {
  return input_stream_count() == 0;
}

const JavaRef<jobject>& AudioManagerAndroid::GetJavaAudioManager() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  if (j_audio_manager_.is_null()) {
    // Create the Android audio manager on the audio thread.
    DVLOG(2) << "Creating Java part of the audio manager";
    j_audio_manager_.Reset(Java_AudioManagerAndroid_createAudioManagerAndroid(
        base::android::AttachCurrentThread(),
        reinterpret_cast<intptr_t>(this)));

    // Prepare the list of audio devices and register receivers for device
    // notifications.
    Java_AudioManagerAndroid_init(base::android::AttachCurrentThread(),
                                  j_audio_manager_);
  }
  return j_audio_manager_;
}

void AudioManagerAndroid::SetCommunicationAudioModeOn(bool on) {
  Java_AudioManagerAndroid_setCommunicationAudioModeOn(
      base::android::AttachCurrentThread(), GetJavaAudioManager(), on);
}

bool AudioManagerAndroid::SetAudioDevice(const std::string& device_id) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());

  // Send the unique device ID to the Java audio manager and make the
  // device switch. Provide an empty string to the Java audio manager
  // if the default device is selected.
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_device_id = ConvertUTF8ToJavaString(
      env, device_id == AudioDeviceDescription::kDefaultDeviceId ? std::string()
                                                                 : device_id);
  return Java_AudioManagerAndroid_setDevice(env, GetJavaAudioManager(),
                                            j_device_id);
}

int AudioManagerAndroid::GetNativeOutputSampleRate() {
  return Java_AudioManagerAndroid_getNativeOutputSampleRate(
      base::android::AttachCurrentThread(), GetJavaAudioManager());
}

bool AudioManagerAndroid::IsAudioLowLatencySupported() {
  return Java_AudioManagerAndroid_isAudioLowLatencySupported(
      base::android::AttachCurrentThread(), GetJavaAudioManager());
}

int AudioManagerAndroid::GetAudioLowLatencyOutputFrameSize() {
  return Java_AudioManagerAndroid_getAudioLowLatencyOutputFrameSize(
      base::android::AttachCurrentThread(), GetJavaAudioManager());
}

int AudioManagerAndroid::GetOptimalOutputFrameSize(int sample_rate,
                                                   int channels) {
  if (IsAudioLowLatencySupported())
    return GetAudioLowLatencyOutputFrameSize();

  return std::max(kDefaultOutputBufferSize,
                  Java_AudioManagerAndroid_getMinOutputFrameSize(
                      base::android::AttachCurrentThread(),
                      sample_rate, channels));
}

void AudioManagerAndroid::DoSetMuteOnAudioThread(bool muted) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  for (OutputStreams::iterator it = streams_.begin();
       it != streams_.end(); ++it) {
    (*it)->SetMute(muted);
  }
}

void AudioManagerAndroid::DoSetVolumeOnAudioThread(double volume) {
  output_volume_override_set_ = true;
  output_volume_override_ = volume;

  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  for (OutputStreams::iterator it = streams_.begin();
       it != streams_.end(); ++it) {
    (*it)->SetVolume(volume);
  }
}

}  // namespace media
