// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/tts_android.h"

#include <string>

#include "base/android/jni_string.h"
#include "base/memory/singleton.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/buildflags.h"
#include "content/public/browser/tts_controller.h"
#include "jni/TtsPlatformImpl_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;

TtsPlatformImplAndroid::TtsPlatformImplAndroid() : utterance_id_(0) {
  JNIEnv* env = AttachCurrentThread();
  java_ref_.Reset(
      Java_TtsPlatformImpl_create(env, reinterpret_cast<intptr_t>(this)));
}

TtsPlatformImplAndroid::~TtsPlatformImplAndroid() {
  JNIEnv* env = AttachCurrentThread();
  Java_TtsPlatformImpl_destroy(env, java_ref_);
}

bool TtsPlatformImplAndroid::PlatformImplAvailable() {
  return true;
}

bool TtsPlatformImplAndroid::Speak(
    int utterance_id,
    const std::string& utterance,
    const std::string& lang,
    const content::VoiceData& voice,
    const content::UtteranceContinuousParameters& params) {
  JNIEnv* env = AttachCurrentThread();
  jboolean success = Java_TtsPlatformImpl_speak(
      env, java_ref_, utterance_id,
      base::android::ConvertUTF8ToJavaString(env, utterance),
      base::android::ConvertUTF8ToJavaString(env, lang), params.rate,
      params.pitch, params.volume);
  if (!success)
    return false;

  utterance_ = utterance;
  utterance_id_ = utterance_id;
  return true;
}

bool TtsPlatformImplAndroid::StopSpeaking() {
  JNIEnv* env = AttachCurrentThread();
  Java_TtsPlatformImpl_stop(env, java_ref_);
  utterance_id_ = 0;
  utterance_.clear();
  return true;
}

void TtsPlatformImplAndroid::Pause() {
  StopSpeaking();
}

void TtsPlatformImplAndroid::Resume() {}

bool TtsPlatformImplAndroid::IsSpeaking() {
  return (utterance_id_ != 0);
}

void TtsPlatformImplAndroid::GetVoices(
    std::vector<content::VoiceData>* out_voices) {
  JNIEnv* env = AttachCurrentThread();
  if (!Java_TtsPlatformImpl_isInitialized(env, java_ref_))
    return;

  int count = Java_TtsPlatformImpl_getVoiceCount(env, java_ref_);
  for (int i = 0; i < count; ++i) {
    out_voices->push_back(content::VoiceData());
    content::VoiceData& data = out_voices->back();
    data.native = true;
    data.name = base::android::ConvertJavaStringToUTF8(
        Java_TtsPlatformImpl_getVoiceName(env, java_ref_, i));
    data.lang = base::android::ConvertJavaStringToUTF8(
        Java_TtsPlatformImpl_getVoiceLanguage(env, java_ref_, i));
    data.events.insert(content::TTS_EVENT_START);
    data.events.insert(content::TTS_EVENT_END);
    data.events.insert(content::TTS_EVENT_ERROR);
  }
}

bool TtsPlatformImplAndroid::LoadBuiltInTtsExtension(
    content::BrowserContext* browser_context) {
  return false;
}

void TtsPlatformImplAndroid::WillSpeakUtteranceWithVoice(
    const content::Utterance* utterance,
    const content::VoiceData& voice_data) {}

std::string TtsPlatformImplAndroid::GetError() {
  return error_;
}

void TtsPlatformImplAndroid::ClearError() {
  error_ = std::string();
}

void TtsPlatformImplAndroid::SetError(const std::string& error) {
  error_ = error;
}

void TtsPlatformImplAndroid::VoicesChanged(JNIEnv* env,
                                           const JavaParamRef<jobject>& obj) {
  content::TtsController::GetInstance()->VoicesChanged();
}

void TtsPlatformImplAndroid::OnEndEvent(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj,
                                        jint utterance_id) {
  SendFinalTtsEvent(utterance_id, content::TTS_EVENT_END,
                    static_cast<int>(utterance_.size()));
}

void TtsPlatformImplAndroid::OnErrorEvent(JNIEnv* env,
                                          const JavaParamRef<jobject>& obj,
                                          jint utterance_id) {
  SendFinalTtsEvent(utterance_id, content::TTS_EVENT_ERROR, 0);
}

void TtsPlatformImplAndroid::OnStartEvent(JNIEnv* env,
                                          const JavaParamRef<jobject>& obj,
                                          jint utterance_id) {
  if (utterance_id != utterance_id_)
    return;

  content::TtsController::GetInstance()->OnTtsEvent(
      utterance_id_, content::TTS_EVENT_START, 0, std::string());
}

void TtsPlatformImplAndroid::SendFinalTtsEvent(int utterance_id,
                                               content::TtsEventType event_type,
                                               int char_index) {
  if (utterance_id != utterance_id_)
    return;

  content::TtsController::GetInstance()->OnTtsEvent(utterance_id_, event_type,
                                                    char_index, std::string());
  utterance_id_ = 0;
  utterance_.clear();
}

// static
TtsPlatformImplAndroid* TtsPlatformImplAndroid::GetInstance() {
  return base::Singleton<
      TtsPlatformImplAndroid,
      base::LeakySingletonTraits<TtsPlatformImplAndroid>>::get();
}
