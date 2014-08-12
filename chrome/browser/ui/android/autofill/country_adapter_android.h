// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_COUNTRY_ADAPTER_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_COUNTRY_ADAPTER_ANDROID_H_

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/ui/autofill/country_combobox_model.h"

namespace autofill {

// A bridge class for showing a list of countries in the Android settings UI.
class CountryAdapterAndroid {
 public:
  CountryAdapterAndroid(JNIEnv* env, jobject obj);

  // Gets all the country codes and names. Every even index is a country name,
  // and the following odd index is the associated country code.
  base::android::ScopedJavaLocalRef<jobjectArray>
      GetItems(JNIEnv* env, jobject unused_obj);

  // Registers the JNI bindings for this class.
  static bool Register(JNIEnv* env);

 private:
  ~CountryAdapterAndroid();

  // Pointer to the java counterpart.
  JavaObjectWeakGlobalRef weak_java_obj_;

  CountryComboboxModel country_combobox_model_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_COUNTRY_ADAPTER_ANDROID_H_
