// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVTOOLS_BRIDGE_ANDROID_APIARY_CLIENT_FACTORY_H_
#define COMPONENTS_DEVTOOLS_BRIDGE_ANDROID_APIARY_CLIENT_FACTORY_H_

#include <jni.h>

namespace devtools_bridge {
namespace android {

/**
 * Native counterpart of Java |ApiaryClientFactory| in DevTools Bridge.
 * Provides access to API keys and OAuth credentials needed for GCD
 * robotic account.
 */
class ApiaryClientFactory {
 public:
  static bool RegisterNatives(JNIEnv* env);
};

}  // namespace android
}  // namespace devtools_bridge

#endif  // COMPONENTS_DEVTOOLS_BRIDGE_ANDROID_APIARY_CLIENT_FACTORY_H_
