// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/basictypes.h"
#include "base/logging.h"
#include "content/browser/android/content_view_statics.h"
#include "content/common/android/address_parser.h"
#include "jni/content_view_statics_jni.h"

using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF16ToJavaString;
using base::android::ScopedJavaLocalRef;


// Return the first substring consisting of the address of a physical location.
static jstring FindAddress(JNIEnv* env, jclass clazz, jstring addr) {
  string16 content_16 = ConvertJavaStringToUTF16(env, addr);
  string16 result_16;
  if (content::address_parser::FindAddress(content_16, &result_16))
    return ConvertUTF16ToJavaString(env, result_16).Release();
  return NULL;
}

namespace content {

bool RegisterWebViewStatics(JNIEnv* env) {
  return RegisterNativesImpl(env) >= 0;
}

}  // namespace content
