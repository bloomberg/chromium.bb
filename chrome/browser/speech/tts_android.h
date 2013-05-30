// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_TTS_ANDROID_H_
#define CHROME_BROWSER_SPEECH_TTS_ANDROID_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/speech/tts_platform.h"

class TtsPlatformImplAndroid : public TtsPlatformImpl {
 public:
  // TtsPlatformImpl implementation.
  virtual bool PlatformImplAvailable() OVERRIDE;
  virtual bool Speak(
      int utterance_id,
      const std::string& utterance,
      const std::string& lang,
      const VoiceData& voice,
      const UtteranceContinuousParameters& params) OVERRIDE;
  virtual bool StopSpeaking() OVERRIDE;
  virtual void Pause() OVERRIDE;
  virtual void Resume() OVERRIDE;
  virtual bool IsSpeaking() OVERRIDE;
  virtual void GetVoices(std::vector<VoiceData>* out_voices) OVERRIDE;

  // Methods called from Java via JNI.
  void VoicesChanged(JNIEnv* env, jobject obj);
  void OnEndEvent(JNIEnv* env, jobject obj, jint utterance_id);
  void OnErrorEvent(JNIEnv* env, jobject obj, jint utterance_id);
  void OnStartEvent(JNIEnv* env, jobject obj, jint utterance_id);

  // Static functions.
  static TtsPlatformImplAndroid* GetInstance();
  static bool Register(JNIEnv* env);

 private:
  friend struct DefaultSingletonTraits<TtsPlatformImplAndroid>;

  TtsPlatformImplAndroid();
  virtual ~TtsPlatformImplAndroid();

  void SendFinalTtsEvent(
      int utterance_id, TtsEventType event_type, int char_index);

  base::android::ScopedJavaGlobalRef<jobject> java_ref_;
  int utterance_id_;
  std::string utterance_;

  DISALLOW_COPY_AND_ASSIGN(TtsPlatformImplAndroid);
};

#endif  // CHROME_BROWSER_SPEECH_TTS_ANDROID_H_
