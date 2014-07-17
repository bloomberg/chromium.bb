// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/android/component_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "base/basictypes.h"
#include "components/dom_distiller/core/distilled_page_prefs_android.h"
#include "components/dom_distiller/core/dom_distiller_service_android.h"
#include "components/dom_distiller/core/url_utils_android.h"

namespace dom_distiller {

namespace android {

static base::android::RegistrationMethod kDomDistillerRegisteredMethods[] = {
    {"DistilledPagePrefs", DistilledPagePrefsAndroid::Register},
    {"DomDistillerService", DomDistillerServiceAndroid::Register},
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

}  // namespace dom_distiller
