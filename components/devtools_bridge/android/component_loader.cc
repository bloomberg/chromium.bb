// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/devtools_bridge/android/component_loader.h"

#include "base/android/base_jni_registrar.h"
#include "components/devtools_bridge/android/apiary_client_factory.h"
#include "components/devtools_bridge/android/session_dependency_factory_android.h"

namespace devtools_bridge {
namespace android {

// static
bool ComponentLoader::OnLoad(JNIEnv* env) {
  return
      SessionDependencyFactory::InitializeSSL() &&
      base::android::RegisterJni(env) &&
      SessionDependencyFactoryAndroid::RegisterNatives(env) &&
      ApiaryClientFactory::RegisterNatives(env);

}

}  // namespace android
}  // namespace devtools_bridge
