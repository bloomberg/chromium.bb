// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_ACCESSIBILITY_FONT_SIZE_PREFS_ANDROID_H_
#define CHROME_BROWSER_ANDROID_ACCESSIBILITY_FONT_SIZE_PREFS_ANDROID_H_

#include <jni.h>
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/observer_list.h"

class PrefChangeRegistrar;
class PrefService;

/*
 * Native implementation of FontSizePrefs. This class is used to get and set
 * FontScaleFactor and ForceEnableZoom.
 */
class FontSizePrefsAndroid {
 public:
  class Observer {
   public:
    virtual void OnChangeFontSize(float font) = 0;
    virtual void OnChangeForceEnableZoom(bool enabled) = 0;
  };

  FontSizePrefsAndroid(JNIEnv* env, jobject obj);
  ~FontSizePrefsAndroid();

  void SetFontScaleFactor(JNIEnv* env,
                          const base::android::JavaRef<jobject>& obj,
                          jfloat font);
  float GetFontScaleFactor(JNIEnv* env,
                           const base::android::JavaRef<jobject>& obj);
  void SetForceEnableZoom(JNIEnv* env,
                          const base::android::JavaRef<jobject>& obj,
                          jboolean enabled);
  bool GetForceEnableZoom(JNIEnv* env,
                          const base::android::JavaRef<jobject>& obj);

  void AddObserver(JNIEnv* env,
                   const base::android::JavaRef<jobject>& obj,
                   jlong obs);
  void RemoveObserver(JNIEnv* env,
                      const base::android::JavaRef<jobject>& obj,
                      jlong obs);

  static bool Register(JNIEnv* env);

 private:
  // Callback for FontScaleFactor changes from pref change registrar.
  void OnFontScaleFactorPrefsChanged();
  // Callback for ForceEnableZoom changes from pref change registrar.
  void OnForceEnableZoomPrefsChanged();

  scoped_ptr<PrefChangeRegistrar> pref_change_registrar_;
  PrefService* const pref_service_;
  base::ObserverList<Observer> observers_;
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;

  DISALLOW_COPY_AND_ASSIGN(FontSizePrefsAndroid);
};

/*
 * Native implementation of FontSizePrefsObserverWrapper. Adds observer support
 * for FontSizePrefs.
 */
class FontSizePrefsObserverAndroid : public FontSizePrefsAndroid::Observer {
 public:
  FontSizePrefsObserverAndroid(JNIEnv* env, jobject obj);
  virtual ~FontSizePrefsObserverAndroid();
  void DestroyObserverAndroid(JNIEnv* env,
                              const base::android::JavaParamRef<jobject>& obj);

  static bool Register(JNIEnv* env);

  // FontSizePrefs::Observer implementation.
  void OnChangeFontSize(float font) override;
  void OnChangeForceEnableZoom(bool enabled) override;

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;
};

#endif  // CHROME_BROWSER_ANDROID_ACCESSIBILITY_FONT_SIZE_PREFS_ANDROID_H_
