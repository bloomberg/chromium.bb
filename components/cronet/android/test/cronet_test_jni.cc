// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "base/android/base_jni_onload.h"
#include "base/android/base_jni_registrar.h"
#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "base/android/library_loader/library_loader_hooks.h"
#include "base/macros.h"
#include "cronet_test_util.h"
#include "cronet_url_request_context_config_test.h"
#include "mock_cert_verifier.h"
#include "mock_url_request_job_factory.h"
#include "native_test_server.h"
#include "quic_test_server.h"
#include "sdch_test_util.h"
#include "test_upload_data_stream_handler.h"

namespace {

const base::android::RegistrationMethod kCronetTestsRegisteredMethods[] = {
    {"MockCertVerifier", cronet::RegisterMockCertVerifier},
    {"MockUrlRequestJobFactory", cronet::RegisterMockUrlRequestJobFactory},
    {"NativeTestServer", cronet::RegisterNativeTestServer},
    {"QuicTestServer", cronet::RegisterQuicTestServer},
    {"SdchTestUtil", cronet::RegisterSdchTestUtil},
    {"TestUploadDataStreamHandlerRegisterJni",
     cronet::TestUploadDataStreamHandlerRegisterJni},
    {"CronetUrlRequestContextConfigTest",
     cronet::RegisterCronetUrlRequestContextConfigTest},
    {"CronetTestUtil", cronet::TestUtil::Register},
};

}  // namespace

// This is called by the VM when the shared library is first loaded.
// Checks the available version of JNI. Also, caches Java reflection artifacts.
extern "C" jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  base::android::InitVM(vm);
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!base::android::OnJNIOnLoadInit()) {
    return -1;
  }

  if (!base::android::RegisterNativeMethods(
          env,
          kCronetTestsRegisteredMethods,
          arraysize(kCronetTestsRegisteredMethods))) {
    return -1;
  }
  return JNI_VERSION_1_6;
}

extern "C" void JNI_OnUnLoad(JavaVM* vm, void* reserved) {
  base::android::LibraryLoaderExitHook();
}
