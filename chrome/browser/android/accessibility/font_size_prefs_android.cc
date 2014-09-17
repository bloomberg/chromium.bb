// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/accessibility/font_size_prefs_android.h"

#include "base/observer_list.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "jni/FontSizePrefs_jni.h"

FontSizePrefsAndroid::FontSizePrefsAndroid(JNIEnv* env, jobject obj)
    : pref_service_(ProfileManager::GetActiveUserProfile()->GetPrefs()) {
  java_ref_.Reset(env, obj);
  pref_change_registrar_.reset(new PrefChangeRegistrar);
  pref_change_registrar_->Init(pref_service_);
  pref_change_registrar_->Add(
      prefs::kWebKitFontScaleFactor,
      base::Bind(&FontSizePrefsAndroid::OnFontScaleFactorPrefsChanged,
                 base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kWebKitForceEnableZoom,
      base::Bind(&FontSizePrefsAndroid::OnForceEnableZoomPrefsChanged,
                 base::Unretained(this)));
}

FontSizePrefsAndroid::~FontSizePrefsAndroid() {
}

void FontSizePrefsAndroid::SetFontScaleFactor(JNIEnv* env,
                                              jobject obj,
                                              jfloat font_size) {
  pref_service_->SetDouble(prefs::kWebKitFontScaleFactor,
                           static_cast<double>(font_size));
}

float FontSizePrefsAndroid::GetFontScaleFactor(JNIEnv* env, jobject obj) {
  return pref_service_->GetDouble(prefs::kWebKitFontScaleFactor);
}

void FontSizePrefsAndroid::SetForceEnableZoom(JNIEnv* env,
                                              jobject obj,
                                              jboolean enabled) {
  pref_service_->SetBoolean(prefs::kWebKitForceEnableZoom, enabled);
}

bool FontSizePrefsAndroid::GetForceEnableZoom(JNIEnv* env, jobject obj) {
  return pref_service_->GetBoolean(prefs::kWebKitForceEnableZoom);
}

void FontSizePrefsAndroid::AddObserver(JNIEnv* env,
                                       jobject obj,
                                       jlong observer_ptr) {
  FontSizePrefsObserverAndroid* font_size_prefs_observer_android =
      reinterpret_cast<FontSizePrefsObserverAndroid*>(observer_ptr);
  observers_.AddObserver(font_size_prefs_observer_android);
}

void FontSizePrefsAndroid::RemoveObserver(JNIEnv* env,
                                          jobject obj,
                                          jlong observer_ptr) {
  FontSizePrefsObserverAndroid* font_size_prefs_observer_android =
      reinterpret_cast<FontSizePrefsObserverAndroid*>(observer_ptr);
  observers_.RemoveObserver(font_size_prefs_observer_android);
}

bool FontSizePrefsAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

jlong Init(JNIEnv* env, jobject obj) {
  FontSizePrefsAndroid* font_size_prefs_android =
      new FontSizePrefsAndroid(env, obj);
  return reinterpret_cast<intptr_t>(font_size_prefs_android);
}

void FontSizePrefsAndroid::OnFontScaleFactorPrefsChanged() {
  JNIEnv* env = base::android::AttachCurrentThread();
  FOR_EACH_OBSERVER(Observer,
                    observers_,
                    OnChangeFontSize(GetFontScaleFactor(env, java_ref_.obj())));
}

void FontSizePrefsAndroid::OnForceEnableZoomPrefsChanged() {
  JNIEnv* env = base::android::AttachCurrentThread();
  FOR_EACH_OBSERVER(
      Observer,
      observers_,
      OnChangeForceEnableZoom(GetForceEnableZoom(env, java_ref_.obj())));
}

FontSizePrefsObserverAndroid::FontSizePrefsObserverAndroid(JNIEnv* env,
                                                           jobject obj) {
  java_ref_.Reset(env, obj);
}

FontSizePrefsObserverAndroid::~FontSizePrefsObserverAndroid() {
}

bool FontSizePrefsObserverAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void FontSizePrefsObserverAndroid::DestroyObserverAndroid(JNIEnv* env,
                                                          jobject obj) {
  delete this;
}

jlong InitObserverAndroid(JNIEnv* env, jobject obj) {
  FontSizePrefsObserverAndroid* observer_wrapper =
      new FontSizePrefsObserverAndroid(env, obj);
  return reinterpret_cast<intptr_t>(observer_wrapper);
}

void FontSizePrefsObserverAndroid::OnChangeFontSize(float font_size) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_FontSizePrefsObserverWrapper_onChangeFontSize(
      env, java_ref_.obj(), font_size);
}

void FontSizePrefsObserverAndroid::OnChangeForceEnableZoom(bool enabled) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_FontSizePrefsObserverWrapper_onChangeForceEnableZoom(
      env, java_ref_.obj(), enabled);
}
