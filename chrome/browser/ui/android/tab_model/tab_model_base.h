// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_MODEL_BASE_H_
#define CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_MODEL_BASE_H_

#include <jni.h>
#include <vector>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "chrome/browser/ui/android/tab_model/tab_model_jni_bridge.h"

class Profile;
class TabAndroid;

namespace content {
class WebContents;
}

// Native representation of TabModelBase which provides access to information
// about a tabstrip to native code and could potentially be used in place of
// Browser for some functionality in Clank.
class TabModelBase : public TabModelJniBridge {
 public:
  TabModelBase(JNIEnv* env, jobject obj, bool is_incognito);
  virtual ~TabModelBase() { }

  // TabModel:
};

// Register the Tab's native methods through jni.
bool RegisterTabModelBase(JNIEnv* env);

#endif  // CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_MODEL_BASE_H_
