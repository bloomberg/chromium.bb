// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/path_service.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "third_party/mojo/src/mojo/edk/embedder/embedder.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/display.h"
#include "ui/gfx/geometry/size.h"
#include "ui/mojo/init/ui_init.h"

#if defined(OS_ANDROID)
#include "base/android/apk_assets.h"
#include "base/android/jni_android.h"
#include "base/test/test_file_util.h"
#endif

namespace {

std::vector<gfx::Display> GetTestDisplays() {
  std::vector<gfx::Display> displays(1);
  displays[0].set_id(2000);
  displays[0].SetScaleAndBounds(1., gfx::Rect(0, 0, 800, 600));
  return displays;
}

class NoAtExitBaseTestSuite : public base::TestSuite {
 public:
  NoAtExitBaseTestSuite(int argc, char** argv)
      : base::TestSuite(argc, argv, false), ui_init_(GetTestDisplays()) {
#if defined(OS_ANDROID)
    base::MemoryMappedFile::Region resource_file_region;
    int fd = base::android::OpenApkAsset("assets/html_viewer.pak",
                                         &resource_file_region);
    CHECK_NE(fd, -1);
    ui::ResourceBundle::InitSharedInstanceWithPakPath(base::FilePath());
    ui::ResourceBundle::GetSharedInstance().AddDataPackFromFileRegion(
        base::File(fd), resource_file_region, ui::SCALE_FACTOR_100P);
#else
    base::FilePath pak_path;
    CHECK(PathService::Get(base::DIR_MODULE, &pak_path));
    pak_path = pak_path.AppendASCII("html_viewer.pak");
    ui::ResourceBundle::InitSharedInstanceWithPakPath(pak_path);
#endif
  }

 private:
  ui::mojo::UIInit ui_init_;

  DISALLOW_COPY_AND_ASSIGN(NoAtExitBaseTestSuite);
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
  mojo::embedder::Init();

  return base::LaunchUnitTests(argc,
                               argv,
                               base::Bind(&RunTestSuite, argc, argv));
}
