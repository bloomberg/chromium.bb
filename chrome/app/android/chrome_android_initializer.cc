// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/android/chrome_android_initializer.h"

#include "base/android/jni_android.h"
#include "base/logging.h"
#include "chrome/app/android/chrome_main_delegate_android.h"
#include "content/public/app/android_library_loader_hooks.h"
#include "content/public/app/content_main.h"
#include "net/proxy/proxy_resolver_v8.h"

jint RunChrome(JavaVM* vm, ChromeMainDelegateAndroid* main_delegate) {
  base::android::InitVM(vm);
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!content::RegisterLibraryLoaderEntryHook(env))
    return -1;

  DCHECK(main_delegate);
  content::SetContentMainDelegate(main_delegate);

  // http://crbug.com/173648 . V8's default isolate is used by ProxyResolverV8
  // to resolve PAC urls. The default isolate is created by static initializer
  // on the shared library thread, and can only be looked up in that thread's
  // TLS.
  net::ProxyResolverV8::RememberDefaultIsolate();

  return JNI_VERSION_1_4;
}
