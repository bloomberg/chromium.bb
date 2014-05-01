// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_LOGO_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_LOGO_BRIDGE_H_

#include <jni.h>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"

class LogoService;

// The C++ counterpart to LogoBridge.java. Enables Java code to access the
// default search provider's logo.
class LogoBridge {
 public:
  explicit LogoBridge(jobject j_profile);
  void Destroy(JNIEnv* env, jobject obj);
  void GetCurrentLogo(JNIEnv* env, jobject obj, jobject j_logo_observer);

 private:
  ~LogoBridge();

  LogoService* logo_service_;
  base::WeakPtrFactory<LogoBridge> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(LogoBridge);
};

bool RegisterLogoBridge(JNIEnv* env);

#endif  // CHROME_BROWSER_ANDROID_LOGO_BRIDGE_H_
