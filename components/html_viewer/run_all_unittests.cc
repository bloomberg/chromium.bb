// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/path_service.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "third_party/mojo/src/mojo/edk/embedder/test_embedder.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "base/test/test_file_util.h"
#endif

namespace {

class NoAtExitBaseTestSuite : public base::TestSuite {
 public:
  NoAtExitBaseTestSuite(int argc, char** argv)
      : base::TestSuite(argc, argv, false) {
#if !defined(OS_ANDROID)
    // This code is only needed on desktop because the ResourceBundle
    // implementation there DCHECKs otherwise.
    base::FilePath pak_path;
    CHECK(PathService::Get(base::DIR_MODULE, &pak_path));
    pak_path = pak_path.AppendASCII("html_viewer.pak");
    ui::ResourceBundle::InitSharedInstanceWithPakPath(pak_path);
#endif
  }
};

int RunTestSuite(int argc, char** argv) {
  return NoAtExitBaseTestSuite(argc, argv).Run();
}

}  // namespace

int main(int argc, char** argv) {
#if defined(OS_ANDROID)
  JNIEnv* env = base::android::AttachCurrentThread();
  base::RegisterContentUriTestUtils(env);
#else
  base::AtExitManager at_exit;
#endif
  mojo::embedder::test::InitWithSimplePlatformSupport();

  return base::LaunchUnitTests(argc,
                               argv,
                               base::Bind(&RunTestSuite, argc, argv));
}
