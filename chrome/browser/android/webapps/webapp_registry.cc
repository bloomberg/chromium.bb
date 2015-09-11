// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapps/webapp_registry.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/callback.h"
#include "chrome/browser/io_thread.h"
#include "content/public/browser/browser_thread.h"
#include "jni/WebappRegistry_jni.h"

// static
void WebappRegistry::UnregisterWebapps(
    const base::Closure& callback) {
  JNIEnv* env = base::android::AttachCurrentThread();
  uintptr_t callback_pointer = reinterpret_cast<uintptr_t>(
      new base::Closure(callback));

  Java_WebappRegistry_unregisterAllWebapps(
      env,
      base::android::GetApplicationContext(),
      callback_pointer);
}

// Callback used by Java when all web apps have been unregistered.
void OnWebappsUnregistered(JNIEnv* env,
                           const JavaParamRef<jclass>& klass,
                           jlong jcallback) {
  base::Closure* callback = reinterpret_cast<base::Closure*>(jcallback);
  callback->Run();
  delete callback;
}

// static
bool WebappRegistry::RegisterWebappRegistry(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
