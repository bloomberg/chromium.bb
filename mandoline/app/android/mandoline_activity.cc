// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "jni/MandolineActivity_jni.h"
#include "mandoline/ui/desktop_ui/public/interfaces/launch_handler.mojom.h"
#include "mojo/shell/connect_util.h"
#include "mojo/shell/standalone/android/main.h"
#include "mojo/shell/standalone/context.h"

namespace mandoline {

static void LaunchURL(JNIEnv* env,
                      const JavaParamRef<jclass>& clazz,
                      const JavaParamRef<jstring>& jurl) {
  LaunchHandlerPtr launch_handler;
  mojo::shell::ConnectToService(
      mojo::shell::GetContext()->application_manager(), GURL("mojo:phone_ui"),
      &launch_handler);
  launch_handler->LaunchURL(
      base::android::ConvertJavaStringToUTF8(env, jurl));
}

bool RegisterMandolineActivity(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace mandoline
