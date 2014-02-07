// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_ANDROID_AUDIO_MANAGER_ANDROID_H_
#define MEDIA_AUDIO_ANDROID_AUDIO_MANAGER_ANDROID_H_

#include <set>

#include "base/android/jni_android.h"
#include "base/gtest_prod_util.h"
#include "base/synchronization/lock.h"
#include "media/audio/audio_manager_base.h"

namespace media {

class OpenSLESOutputStream;

// Android implemention of AudioManager.
class MEDIA_EXPORT AudioManagerAndroid : public AudioManagerBase {
 public:
  AudioManagerAndroid(AudioLogFactory* audio_log_factory);

  // Implementation of AudioManager.
  virtual bool HasAudioOutputDevices() OVERRIDE;
  virtual bool HasAudioInputDevices() OVERRIDE;
  virtual void GetAudioInputDeviceNames(
      AudioDeviceNames* device_names) OVERRIDE;
  virtual void GetAudioOutputDeviceNames(
      AudioDeviceNames* device_names) OVERRIDE;
  virtual AudioParameters GetInputStreamParameters(
      const std::string& device_id) OVERRIDE;

  virtual AudioOutputStream* MakeAudioOutputStream(
      const AudioParameters& params,
      const std::string& device_id) OVERRIDE;
  virtual AudioInputStream* MakeAudioInputStream(
      const AudioParameters& params,
      const std::string& device_id) OVERRIDE;
  virtual void ReleaseOutputStream(AudioOutputStream* stream) OVERRIDE;
  virtual void ReleaseInputStream(AudioInputStream* stream) OVERRIDE;

  // Implementation of AudioManagerBase.
  virtual AudioOutputStream* MakeLinearOutputStream(
      const AudioParameters& params) OVERRIDE;
  virtual AudioOutputStream* MakeLowLatencyOutputStream(
      const AudioParameters& params,
      const std::string& device_id) OVERRIDE;
  virtual AudioInputStream* MakeLinearInputStream(
      const AudioParameters& params,
      const std::string& device_id) OVERRIDE;
  virtual AudioInputStream* MakeLowLatencyInputStream(
      const AudioParameters& params,
      const std::string& device_id) OVERRIDE;

  static bool RegisterAudioManager(JNIEnv* env);

  void SetMute(JNIEnv* env, jobject obj, jboolean muted);

 protected:
  virtual ~AudioManagerAndroid();

  virtual AudioParameters GetPreferredOutputStreamParameters(
      const std::string& output_device_id,
      const AudioParameters& input_params) OVERRIDE;

 private:
  bool HadNoAudioStreams();
  void Init();
  void Close();
  void SetCommunicationAudioModeOn(bool on);
  bool SetAudioDevice(const std::string& device_id);
  int GetNativeOutputSampleRate();
  bool IsAudioLowLatencySupported();
  int GetAudioLowLatencyOutputFrameSize();
  int GetOptimalOutputFrameSize(int sample_rate, int channels);

  void DoSetMuteOnAudioThread(bool muted);

  // Allow the AudioAndroidTest to access private methods.
  FRIEND_TEST_ALL_PREFIXES(AudioAndroidOutputTest, IsAudioLowLatencySupported);

  // Java AudioManager instance.
  base::android::ScopedJavaGlobalRef<jobject> j_audio_manager_;

  typedef std::set<OpenSLESOutputStream*> OutputStreams;
  OutputStreams streams_;
  // TODO(wjia): remove this lock once unit test modules are fixed to call
  // AudioManager::MakeAudioOutputStream on the audio thread. For now, this
  // lock is used to guard access to |streams_|.
  base::Lock streams_lock_;

  DISALLOW_COPY_AND_ASSIGN(AudioManagerAndroid);
};

}  // namespace media

#endif  // MEDIA_AUDIO_ANDROID_AUDIO_MANAGER_ANDROID_H_
