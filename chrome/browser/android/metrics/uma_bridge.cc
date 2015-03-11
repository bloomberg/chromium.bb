// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/metrics/uma_bridge.h"

#include <jni.h>

#include "base/metrics/histogram.h"
#include "content/public/browser/user_metrics.h"
#include "jni/UmaBridge_jni.h"

using base::UserMetricsAction;
using content::RecordAction;

static void RecordMenuShow(JNIEnv*, jclass) {
  RecordAction(UserMetricsAction("MobileMenuShow"));
}

static void RecordUsingMenu(JNIEnv*,
                            jclass,
                            jboolean is_by_hw_button,
                            jboolean is_dragging) {
  if (is_by_hw_button) {
    if (is_dragging) {
      NOTREACHED() << "We do not support dragging for hardware menu button.";
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

// Android Beam

static void RecordBeamCallbackSuccess(JNIEnv*, jclass) {
  RecordAction(UserMetricsAction("MobileBeamCallbackSuccess"));
}

static void RecordBeamInvalidAppState(JNIEnv*, jclass) {
  RecordAction(UserMetricsAction("MobileBeamInvalidAppState"));
}

// Data Saver

static void RecordDataReductionProxyTurnedOn(JNIEnv*, jclass) {
  RecordAction(UserMetricsAction("DataReductionProxy_TurnedOn"));
}

static void RecordDataReductionProxyTurnedOff(JNIEnv*, jclass) {
  RecordAction(UserMetricsAction("DataReductionProxy_TurnedOff"));
}

static void RecordDataReductionProxyTurnedOnFromPromo(JNIEnv*, jclass) {
  RecordAction(UserMetricsAction("DataReductionProxy_TurnedOnFromPromo"));
}

static void RecordDataReductionProxyPromoAction(
    JNIEnv*, jclass, jint action, jint boundary) {
  UMA_HISTOGRAM_ENUMERATION("DataReductionProxy.PromoAction",
                            action,
                            boundary);
}

static void RecordDataReductionProxyPromoDisplayed(JNIEnv*, jclass) {
  RecordAction(UserMetricsAction("DataReductionProxy_PromoDisplayed"));
}

static void RecordDataReductionProxySettings(
    JNIEnv*, jclass, jint notification, jint boundary) {
  UMA_HISTOGRAM_ENUMERATION("DataReductionProxy.SettingsConversion",
                            notification,
                            boundary);
}

// First Run Experience
static void RecordFreSignInShown(JNIEnv*, jclass) {
  RecordAction(UserMetricsAction("MobileFre.SignInShown"));
}

namespace chrome {
namespace android {

// Register native methods
bool RegisterUmaBridge(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android
}  // namespace chrome
