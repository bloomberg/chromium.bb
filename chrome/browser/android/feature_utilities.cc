// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/feature_utilities.h"

#include "jni/FeatureUtilities_jni.h"

#include "sql/connection.h"

namespace {
bool document_mode_enabled = false;
bool custom_tab_visible = false;
} // namespace

namespace chrome {
namespace android {

RunningModeHistogram GetDocumentModeValue() {
  return document_mode_enabled ? RUNNING_MODE_DOCUMENT_MODE :
      RUNNING_MODE_TABBED_MODE;
}

CustomTabsVisibilityHistogram GetCustomTabsVisibleValue() {
  return custom_tab_visible ? VISIBLE_CUSTOM_TAB :
      VISIBLE_CHROME_TAB;
}

} // namespace android
} // namespace chrome

static void SetDocumentModeEnabled(JNIEnv* env,
                                   const JavaParamRef<jclass>& clazz,
                                   jboolean enabled) {
  document_mode_enabled = enabled;
}

static void SetCustomTabVisible(JNIEnv* env,
                                const JavaParamRef<jclass>& clazz,
                                jboolean visible) {
  custom_tab_visible = visible;
}

static void SetSqlMmapDisabledByDefault(JNIEnv* env,
                                     const JavaParamRef<jclass>& clazz) {
  sql::Connection::set_mmap_disabled_by_default();
}

bool RegisterFeatureUtilities(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
