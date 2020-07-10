// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/jni_utils.h"
#include "base/android/library_loader/library_loader_hooks.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/no_destructor.h"
#include "base/test/test_support_android.h"
#include "chrome/app/android/chrome_jni_onload.h"
#include "chrome/browser/android/chrome_jni_for_test_registration.h"
#include "chrome/utility/chrome_content_utility_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/network_service_test_helper.h"
#include "services/service_manager/sandbox/switches.h"

namespace {

content::NetworkServiceTestHelper* GetNetworkServiceTestHelper() {
  static base::NoDestructor<content::NetworkServiceTestHelper> instance;
  return instance.get();
}

bool NativeInit(base::android::LibraryProcessType) {
  // Setup a working test environment for the network service in case it's used.
  // Only create this object in the utility process, so that its members don't
  // interfere with other test objects in the browser process.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->GetSwitchValueASCII(switches::kProcessType) ==
          switches::kUtilityProcess &&
      command_line->GetSwitchValueASCII(
          service_manager::switches::kServiceSandboxType) ==
          service_manager::switches::kNetworkSandbox) {
    ChromeContentUtilityClient::SetNetworkBinderCreationCallback(
        base::BindRepeating(
            [](content::NetworkServiceTestHelper* helper,
               service_manager::BinderRegistry* registry) {
              helper->RegisterNetworkBinders(registry);
            },
            GetNetworkServiceTestHelper()));
  }

  return android::OnJNIOnLoadInit();
}

}  // namespace

// This is called by the VM when the shared library is first loaded.
JNI_EXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  // By default, all JNI methods are registered. However, since render processes
  // don't need very much Java code, we enable selective JNI registration on the
  // Java side and only register a subset of JNI methods.
  base::android::InitVM(vm);
  JNIEnv* env = base::android::AttachCurrentThread();

  if (!base::android::IsSelectiveJniRegistrationEnabled(env) &&
      !RegisterNonMainDexNatives(env)) {
    return -1;
  }

  if (!RegisterMainDexNatives(env)) {
    return -1;
  }
  base::android::SetNativeInitializationHook(NativeInit);
  return JNI_VERSION_1_4;
}
