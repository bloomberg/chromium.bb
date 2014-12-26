// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVTOOLS_BRIDGE_ANDROID_COMPONENT_LOADER_H_
#define COMPONENTS_DEVTOOLS_BRIDGE_ANDROID_COMPONENT_LOADER_H_

#include <jni.h>

namespace devtools_bridge {
namespace android {

class ComponentLoader {
 public:
  static bool OnLoad(JNIEnv* env);
};

}  // namespace android
}  // namespace devtools_bridge

#endif  // COMPONENTS_DEVTOOLS_BRIDGE_ANDROID_APIARY_COMPONENT_LOADER_H_
