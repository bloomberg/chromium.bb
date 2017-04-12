// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/payments/android/journey_logger_android.h"

#include "base/android/jni_string.h"
#include "chrome/browser/browser_process.h"
#include "jni/JourneyLogger_jni.h"
#include "url/gurl.h"

namespace payments {
namespace {

using ::base::android::JavaParamRef;
using ::base::android::ConvertJavaStringToUTF8;

}  // namespace

// static
bool JourneyLoggerAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

JourneyLoggerAndroid::JourneyLoggerAndroid(bool is_incognito,
                                           const std::string& url)
    : journey_logger_(is_incognito,
                      GURL(url),
                      g_browser_process->ukm_service()) {}

JourneyLoggerAndroid::~JourneyLoggerAndroid() {}

void JourneyLoggerAndroid::Destroy(JNIEnv* env,
                                   const JavaParamRef<jobject>& jcaller) {
  delete this;
}

void JourneyLoggerAndroid::SetNumberOfSuggestionsShown(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jint jsection,
    jint jnumber) {
  DCHECK_GE(jsection, 0);
  DCHECK_LT(jsection, JourneyLogger::Section::SECTION_MAX);
  journey_logger_.SetNumberOfSuggestionsShown(
      static_cast<JourneyLogger::Section>(jsection), jnumber);
}

void JourneyLoggerAndroid::IncrementSelectionChanges(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jint jsection) {
  DCHECK_GE(jsection, 0);
  DCHECK_LT(jsection, JourneyLogger::Section::SECTION_MAX);
  journey_logger_.IncrementSelectionChanges(
      static_cast<JourneyLogger::Section>(jsection));
}

void JourneyLoggerAndroid::IncrementSelectionEdits(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jint jsection) {
  DCHECK_GE(jsection, 0);
  DCHECK_LT(jsection, JourneyLogger::Section::SECTION_MAX);
  journey_logger_.IncrementSelectionEdits(
      static_cast<JourneyLogger::Section>(jsection));
}

void JourneyLoggerAndroid::IncrementSelectionAdds(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jint jsection) {
  DCHECK_GE(jsection, 0);
  DCHECK_LT(jsection, JourneyLogger::Section::SECTION_MAX);
  journey_logger_.IncrementSelectionAdds(
      static_cast<JourneyLogger::Section>(jsection));
}

void JourneyLoggerAndroid::SetCanMakePaymentValue(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jboolean jvalue) {
  journey_logger_.SetCanMakePaymentValue(jvalue);
}

void JourneyLoggerAndroid::SetShowCalled(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  journey_logger_.SetShowCalled();
}

void JourneyLoggerAndroid::SetEventOccurred(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jint jevent) {
  DCHECK_GE(jevent, 0);
  DCHECK_LT(jevent, JourneyLogger::Event::EVENT_MAX);
  journey_logger_.SetEventOccurred(static_cast<JourneyLogger::Event>(jevent));
}

void JourneyLoggerAndroid::RecordJourneyStatsHistograms(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jint jcompletion_status) {
  DCHECK_GE(jcompletion_status, 0);
  DCHECK_LT(jcompletion_status,
            JourneyLogger::CompletionStatus::COMPLETION_STATUS_MAX);
  journey_logger_.RecordJourneyStatsHistograms(
      static_cast<JourneyLogger::CompletionStatus>(jcompletion_status));
}

static jlong InitJourneyLoggerAndroid(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    jboolean jis_incognito,
    const base::android::JavaParamRef<jstring>& jurl) {
  return reinterpret_cast<jlong>(new JourneyLoggerAndroid(
      jis_incognito, ConvertJavaStringToUTF8(env, jurl)));
}

}  // namespace payments
