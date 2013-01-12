// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/lib/main/aw_main_delegate.h"
#include "android_webview/native/android_webview_jni_registrar.h"
#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "components/web_contents_delegate_android/component_jni_registrar.h"
#include "content/components/navigation_interception/component_jni_registrar.h"
#include "content/public/app/android_library_loader_hooks.h"
#include "content/public/app/content_main.h"

static base::android::RegistrationMethod
kWebViewDependencyRegisteredMethods[] = {
  { "NavigationInterception", content::RegisterNavigationInterceptionJni },
  { "WebContentsDelegateAndroid",
      components::RegisterWebContentsDelegateAndroidJni },
};

// This is called by the VM when the shared library is first loaded.
// Most of the initialization is done in LibraryLoadedOnMainThread(), not here.
JNI_EXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  base::android::InitVM(vm);
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!content::RegisterLibraryLoaderEntryHook(env))
    return -1;

  // Register JNI for components we depend on.
  if (!RegisterNativeMethods(
      env,
      kWebViewDependencyRegisteredMethods,
      arraysize(kWebViewDependencyRegisteredMethods)))
    return -1;

  if (!android_webview::RegisterJni(env))
    return -1;

  content::SetContentMainDelegate(new android_webview::AwMainDelegate());

  return JNI_VERSION_1_4;
}
