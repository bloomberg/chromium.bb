// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/logo_bridge.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "chrome/browser/android/logo_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "components/search_provider_logos/logo_tracker.h"
#include "jni/LogoBridge_jni.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/android/java_bitmap.h"

using base::android::ScopedJavaLocalRef;
using base::android::ConvertUTF8ToJavaString;

namespace {

// Converts a C++ Logo to a Java Logo.
ScopedJavaLocalRef<jobject> ConvertLogoToJavaObject(
    JNIEnv* env,
    const search_provider_logos::Logo* logo) {
  if (!logo)
    return ScopedJavaLocalRef<jobject>();

  ScopedJavaLocalRef<jobject> j_bitmap = gfx::ConvertToJavaBitmap(&logo->image);

  ScopedJavaLocalRef<jstring> j_on_click_url;
  if (!logo->metadata.on_click_url.empty())
    j_on_click_url = ConvertUTF8ToJavaString(env, logo->metadata.on_click_url);

  ScopedJavaLocalRef<jstring> j_alt_text;
  if (!logo->metadata.alt_text.empty())
    j_alt_text = ConvertUTF8ToJavaString(env, logo->metadata.alt_text);

  return Java_LogoBridge_createLogo(
      env, j_bitmap.obj(), j_on_click_url.obj(), j_alt_text.obj());
}

class LogoObserverAndroid : public search_provider_logos::LogoObserver {
 public:
  LogoObserverAndroid(base::WeakPtr<LogoBridge> logo_bridge,
                      JNIEnv* env,
                      jobject j_logo_observer)
      : logo_bridge_(logo_bridge) {
    j_logo_observer_.Reset(env, j_logo_observer);
  }

  virtual ~LogoObserverAndroid() {}

  // seach_provider_logos::LogoObserver:
  virtual void OnLogoAvailable(const search_provider_logos::Logo* logo,
                               bool from_cache) OVERRIDE {
    if (!logo_bridge_)
      return;

    JNIEnv* env = base::android::AttachCurrentThread();
    ScopedJavaLocalRef<jobject> j_logo = ConvertLogoToJavaObject(env, logo);
    Java_LogoObserver_onLogoAvailable(
        env, j_logo_observer_.obj(), j_logo.obj(), from_cache);
  }

  virtual void OnObserverRemoved() OVERRIDE {
    delete this;
  }

 private:
  // The associated LogoBridge. We won't call back to Java if the LogoBridge has
  // been destroyed.
  base::WeakPtr<LogoBridge> logo_bridge_;

  base::android::ScopedJavaGlobalRef<jobject> j_logo_observer_;
};

}  // namespace

static jlong Init(JNIEnv* env, jobject obj, jobject j_profile) {
  LogoBridge* logo_bridge = new LogoBridge(j_profile);
  return reinterpret_cast<intptr_t>(logo_bridge);
}

LogoBridge::LogoBridge(jobject j_profile) : weak_ptr_factory_(this) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  if (profile)
    logo_service_ = LogoServiceFactory::GetForProfile(profile);
}

LogoBridge::~LogoBridge() {}

void LogoBridge::Destroy(JNIEnv* env, jobject obj) {
  delete this;
}

void LogoBridge::GetCurrentLogo(JNIEnv* env,
                                jobject obj,
                                jobject j_logo_observer) {
  if (!logo_service_)
    return;

  // |observer| is deleted in LogoObserverAndroid::OnObserverRemoved().
  LogoObserverAndroid* observer = new LogoObserverAndroid(
      weak_ptr_factory_.GetWeakPtr(), env, j_logo_observer);
  logo_service_->GetLogo(observer);
}

// static
bool RegisterLogoBridge(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
