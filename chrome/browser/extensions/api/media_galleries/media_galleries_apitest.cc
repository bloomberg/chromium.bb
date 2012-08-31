// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/platform_app_browsertest_util.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"

#if defined(OS_LINUX)
#include <fstream>

#include "base/environment.h"
#include "base/scoped_temp_dir.h"
#endif

using extensions::PlatformAppBrowserTest;

namespace {

class ExperimentalMediaGalleriesApiTest : public ExtensionApiTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableExperimentalExtensionApis);
  }
};

class EnsurePictureDirectoryExists {
 public:
  EnsurePictureDirectoryExists() {
    Init();
  }

 private:
  void Init() {
#if defined(OS_CHROMEOS) || defined(OS_ANDROID)
    return;
#elif defined(OS_LINUX)
    // On Linux, the picture directory probably doesn't exist by default,
    // so we override the settings to point to a tempdir.
    ASSERT_TRUE(xdg_dir_.CreateUniqueTempDir());

    FilePath config_file(xdg_dir_.path().Append("user-dirs.dirs"));
    std::ofstream file;
    file.open(config_file.value().c_str());
    ASSERT_TRUE(file.is_open());
    file << "XDG_PICTURES_DIR=\"" << xdg_dir_.path().value() << "\"";
    file.close();

    scoped_ptr<base::Environment> env(base::Environment::Create());
    env->SetVar("XDG_CONFIG_HOME", xdg_dir_.path().value());
#else
    FilePath pictures_path;
    ASSERT_TRUE(PathService::Get(chrome::DIR_USER_PICTURES, &pictures_path));
    ASSERT_TRUE(file_util::DirectoryExists(pictures_path));
#endif
  }

#if defined(OS_LINUX)
  ScopedTempDir xdg_dir_;
#endif
};


}  // namespace

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, NoGalleries) {
  ASSERT_TRUE(RunPlatformAppTest("api_test/media_galleries/no_galleries"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, MediaGalleriesRead) {
  EnsurePictureDirectoryExists picture_directory;
  ASSERT_TRUE(RunPlatformAppTest("api_test/media_galleries/read_access"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, MediaGalleriesNoAccess) {
  EnsurePictureDirectoryExists picture_directory;
  ASSERT_TRUE(RunPlatformAppTest("api_test/media_galleries/no_access"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ExperimentalMediaGalleriesApiTest,
                       ExperimentalMediaGalleries) {
  ASSERT_TRUE(RunExtensionTest("media_galleries/experimental")) << message_;
}
