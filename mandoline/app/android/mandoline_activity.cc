// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "jni/MandolineActivity_jni.h"
#include "mandoline/ui/browser/public/interfaces/launch_handler.mojom.h"
#include "mojo/runner/android/main.h"
#include "mojo/runner/context.h"

namespace mandoline {

static void LaunchURL(JNIEnv* env, jclass clazz, jstring jurl) {
  LaunchHandlerPtr launch_handler;
  mojo::runner::GetContext()->application_manager()->ConnectToService(
      GURL("mojo:browser"), &launch_handler);
  launch_handler->LaunchURL(
      base::android::ConvertJavaStringToUTF8(env, jurl));
}

bool RegisterMandolineActivity(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace mandoline
