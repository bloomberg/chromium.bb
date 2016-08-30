// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/content/browser/android/content_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "base/macros.h"
#include "components/dom_distiller/content/browser/distillable_page_utils_android.h"

namespace dom_distiller {

namespace content {

namespace android {

static base::android::RegistrationMethod kDomDistillerRegisteredMethods[] = {
    {"DistillablePageUtils",
     dom_distiller::android::RegisterDistillablePageUtils},
};

bool RegisterDomDistiller(JNIEnv* env) {
  return base::android::RegisterNativeMethods(
      env,
      kDomDistillerRegisteredMethods,
      arraysize(kDomDistillerRegisteredMethods));
}

}  // namespace android

}  // namespace content

}  // namespace dom_distiller
