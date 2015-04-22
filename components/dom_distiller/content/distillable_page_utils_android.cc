// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/content/distillable_page_utils_android.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "components/dom_distiller/content/distillable_page_utils.h"
#include "content/public/browser/web_contents.h"
#include "jni/DistillablePageUtils_jni.h"

using base::android::ScopedJavaGlobalRef;

namespace dom_distiller {
namespace android {
namespace {
void OnIsPageDistillableResult(
    scoped_ptr<ScopedJavaGlobalRef<jobject>> callback_holder,
    bool isDistillable) {
  Java_DistillablePageUtils_callOnIsPageDistillableResult(
      base::android::AttachCurrentThread(), callback_holder->obj(),
      isDistillable);
}
}  // namespace

static void IsPageDistillable(JNIEnv* env,
                              jclass jcaller,
                              jobject webContents,
                              jboolean is_mobile_optimized,
                              jobject callback) {
  content::WebContents* web_contents(
      content::WebContents::FromJavaWebContents(webContents));
  scoped_ptr<ScopedJavaGlobalRef<jobject>> callback_holder(
      new ScopedJavaGlobalRef<jobject>());
  callback_holder->Reset(env, callback);

  if (!web_contents) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(OnIsPageDistillableResult,
                              base::Passed(&callback_holder), false));
    return;
  }
  IsDistillablePage(
      web_contents, is_mobile_optimized,
      base::Bind(OnIsPageDistillableResult, base::Passed(&callback_holder)));
}

bool RegisterDistillablePageUtils(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}
}
