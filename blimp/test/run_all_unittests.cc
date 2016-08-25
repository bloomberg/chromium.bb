// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "mojo/edk/embedder/embedder.h"

#if defined(OS_ANDROID)
#include "base/android/base_jni_registrar.h"
#include "base/android/jni_android.h"
#include "blimp/client/public/android/blimp_jni_registrar.h"
#include "net/android/net_jni_registrar.h"
#include "ui/gfx/android/gfx_jni_registrar.h"
#endif

namespace {

class BlimpTestSuite : public base::TestSuite {
 public:
  BlimpTestSuite(int argc, char** argv) : base::TestSuite(argc, argv) {}

 protected:
  void Initialize() override {
    base::TestSuite::Initialize();

#if defined(OS_ANDROID)
    if (!RegisterJni(base::android::AttachCurrentThread())) {
      LOG(FATAL) << "Jni Registration failed";
    }
#endif
  }

 private:
#if defined(OS_ANDROID)
  bool RegisterJni(JNIEnv* env) {
    if (!base::android::RegisterJni(env)) {
      return false;
    }

    if (!net::android::RegisterJni(env)) {
      return false;
    }

    if (!gfx::android::RegisterJni(env)) {
      return false;
    }

    if (!blimp::client::RegisterBlimpJni(env)) {
      return false;
    }

    return true;
  }
#endif

  DISALLOW_COPY_AND_ASSIGN(BlimpTestSuite);
};

}  // namespace

int main(int argc, char** argv) {
  mojo::edk::Init();
  BlimpTestSuite test_suite(argc, argv);
  return base::LaunchUnitTests(
      argc, argv,
      base::Bind(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
