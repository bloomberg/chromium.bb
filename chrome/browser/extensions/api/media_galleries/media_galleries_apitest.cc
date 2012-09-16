// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/stringprintf.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/platform_app_browsertest_util.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"

#if defined(OS_LINUX)
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
    // On Linux, the media directories may not exist by default, so override
    // the settings to point to a temp directory with the media directories.
    ASSERT_TRUE(xdg_dir_.CreateUniqueTempDir());
    const FilePath xdg_path = xdg_dir_.path();
    const FilePath music_dir(xdg_path.Append("Megaman"));
    const FilePath pictures_dir(xdg_path.Append("Pitfall"));
    const FilePath videos_dir(xdg_path.Append("VVVV"));
    ASSERT_TRUE(file_util::CreateDirectory(music_dir));
    ASSERT_TRUE(file_util::CreateDirectory(pictures_dir));
    ASSERT_TRUE(file_util::CreateDirectory(videos_dir));

    const FilePath config_file(xdg_path.Append("user-dirs.dirs"));
    std::string xdg_user_dir_data = base::StringPrintf(
        "XDG_MUSIC_DIR=\"%s\"\n"
        "XDG_PICTURES_DIR=\"%s\"\n"
        "XDG_VIDEOS_DIR=\"%s\"\n",
        music_dir.value().c_str(),
        pictures_dir.value().c_str(),
        videos_dir.value().c_str());
    ASSERT_TRUE(file_util::WriteFile(config_file,
                                     xdg_user_dir_data.c_str(),
                                     xdg_user_dir_data.size()));

    scoped_ptr<base::Environment> env(base::Environment::Create());
    env->SetVar("XDG_CONFIG_HOME", xdg_path.value());
#else
    const int kDirectoryKeys[] = {
      chrome::DIR_USER_MUSIC,
      chrome::DIR_USER_PICTURES,
      chrome::DIR_USER_VIDEOS,
    };

    for (size_t i = 0; i < arraysize(kDirectoryKeys); ++i) {
      FilePath path;
      ASSERT_TRUE(PathService::Get(kDirectoryKeys[i], &path));
      ASSERT_TRUE(file_util::DirectoryExists(path));
    }
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
