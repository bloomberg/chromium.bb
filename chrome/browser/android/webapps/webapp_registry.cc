// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapps/webapp_registry.h"

#include <jni.h>

#include "base/android/context_utils.h"
#include "base/android/jni_android.h"
#include "base/callback.h"
#include "chrome/browser/android/browsing_data/url_filter_bridge.h"
#include "chrome/browser/io_thread.h"
#include "content/public/browser/browser_thread.h"
#include "jni/WebappRegistry_jni.h"

using base::android::JavaParamRef;

void WebappRegistry::UnregisterWebappsForUrls(
    const base::Callback<bool(const GURL&)>& url_filter,
    const base::Closure& callback) {
  JNIEnv* env = base::android::AttachCurrentThread();
  // TODO(msramek): Consider implementing a wrapper class that will call and
  // destroy this closure from Java, eliminating the need for
  // OnWebappsUnregistered() and OnClearedWebappHistory() callbacks.
  uintptr_t callback_pointer = reinterpret_cast<uintptr_t>(
      new base::Closure(callback));

  // We will destroy |filter_bridge| from its Java counterpart before calling
  // back OnWebappsUnregistered().
  UrlFilterBridge* filter_bridge = new UrlFilterBridge(url_filter);

  Java_WebappRegistry_unregisterWebappsForUrls(env, filter_bridge->j_bridge(),
                                               callback_pointer);
}

void WebappRegistry::ClearWebappHistoryForUrls(
    const base::Callback<bool(const GURL&)>& url_filter,
    const base::Closure& callback) {
  JNIEnv* env = base::android::AttachCurrentThread();
  uintptr_t callback_pointer = reinterpret_cast<uintptr_t>(
      new base::Closure(callback));

  // We will destroy |filter_bridge| from its Java counterpart before calling
  // back OnClearedWebappHistory().
  UrlFilterBridge* filter_bridge = new UrlFilterBridge(url_filter);

  Java_WebappRegistry_clearWebappHistoryForUrls(env, filter_bridge->j_bridge(),
                                                callback_pointer);
}

// Callback used by Java when all web apps have been unregistered.
void OnWebappsUnregistered(JNIEnv* env,
                           const JavaParamRef<jclass>& clazz,
                           jlong jcallback) {
  base::Closure* callback = reinterpret_cast<base::Closure*>(jcallback);
  callback->Run();
  delete callback;
}

// Callback used by Java when all web app last used times have been cleared.
void OnClearedWebappHistory(JNIEnv* env,
                            const JavaParamRef<jclass>& clazz,
                            jlong jcallback) {
  base::Closure* callback = reinterpret_cast<base::Closure*>(jcallback);
  callback->Run();
  delete callback;
}

// static
bool WebappRegistry::RegisterWebappRegistry(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
