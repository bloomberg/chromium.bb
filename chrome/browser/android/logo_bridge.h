// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_LOGO_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_LOGO_BRIDGE_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"

class LogoService;

// The C++ counterpart to LogoBridge.java. Enables Java code to access the
// default search provider's logo.
class LogoBridge {
 public:
  explicit LogoBridge(jobject j_profile);
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  // Gets the current non-animated logo (downloading it if necessary) and passes
  // it to the observer.
  void GetCurrentLogo(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& j_logo_observer);

  // Downloads the animated logo from the given URL and returns it to the
  // callback. Does not support multiple concurrent requests: A second request
  // for the same URL will be ignored; a request for a different URL will cancel
  // any ongoing request. If downloading fails, the callback is not called.
  void GetAnimatedLogo(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& obj,
                       const base::android::JavaParamRef<jobject>& j_callback,
                       const base::android::JavaParamRef<jstring>& j_url);

 private:
  class AnimatedLogoFetcher;

  ~LogoBridge();

  LogoService* logo_service_;

  std::unique_ptr<AnimatedLogoFetcher> animated_logo_fetcher_;

  base::WeakPtrFactory<LogoBridge> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(LogoBridge);
};

bool RegisterLogoBridge(JNIEnv* env);

#endif  // CHROME_BROWSER_ANDROID_LOGO_BRIDGE_H_
