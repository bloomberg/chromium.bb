// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/uma_bridge.h"

#include <jni.h>

#include "content/public/browser/user_metrics.h"
#include "jni/UmaBridge_jni.h"

using base::UserMetricsAction;
using content::RecordAction;
using content::RecordComputedAction;

static void RecordMenuShow(JNIEnv*, jclass) {
  RecordAction(UserMetricsAction("MobileMenuShow"));
}

static void RecordUsingMenu(JNIEnv*,
                            jclass,
                            jboolean is_by_hw_button,
                            jboolean is_dragging) {
  if (is_by_hw_button) {
    if (is_dragging) {
      RecordAction(UserMetricsAction("MobileUsingMenuByHwButtonDragging"));
    } else {
      RecordAction(UserMetricsAction("MobileUsingMenuByHwButtonTap"));
    }
  } else {
    if (is_dragging) {
      RecordAction(UserMetricsAction("MobileUsingMenuBySwButtonDragging"));
    } else {
      RecordAction(UserMetricsAction("MobileUsingMenuBySwButtonTap"));
    }
  }
}

namespace chrome {
namespace android {

// Register native methods
bool RegisterUmaBridge(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android
}  // namespace chrome
