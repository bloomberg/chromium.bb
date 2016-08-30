// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/android/core_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "base/macros.h"
#include "components/dom_distiller/core/distilled_page_prefs_android.h"
#include "components/dom_distiller/core/dom_distiller_service_android.h"
#include "components/dom_distiller/core/url_utils_android.h"

namespace dom_distiller {

namespace core {

namespace android {

static base::android::RegistrationMethod kDomDistillerRegisteredMethods[] = {
    {"DistilledPagePrefs",
     dom_distiller::android::DistilledPagePrefsAndroid::Register},
    {"DomDistillerService",
     dom_distiller::android::DomDistillerServiceAndroid::Register},
    {"DomDistillerUrlUtils",
     dom_distiller::url_utils::android::RegisterUrlUtils},
};

bool RegisterDomDistiller(JNIEnv* env) {
  return base::android::RegisterNativeMethods(
      env,
      kDomDistillerRegisteredMethods,
      arraysize(kDomDistillerRegisteredMethods));
}

}  // namespace android

}  // namespace core

}  // namespace dom_distiller
