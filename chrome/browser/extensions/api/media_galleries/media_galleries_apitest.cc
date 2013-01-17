// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/platform_app_browsertest_util.h"
#include "chrome/browser/media_gallery/media_galleries_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/render_view_host.h"

using extensions::PlatformAppBrowserTest;

namespace {

class ExperimentalMediaGalleriesApiTest : public ExtensionApiTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableExperimentalExtensionApis);
  }
};

class PlatformAppMediaGalleriesBrowserTest : public PlatformAppBrowserTest {
 protected:
  // Since ExtensionTestMessageListener does not work with RunPlatformAppTest(),
  // This helper method can be used to run additional media gallery tests.
  void RunSecondTestPhase(int expected_galleries) {
    const extensions::Extension* extension = GetSingleLoadedExtension();
    extensions::ExtensionHost* host =
        extensions::ExtensionSystem::Get(browser()->profile())->
            process_manager()->GetBackgroundHostForExtension(extension->id());
    ASSERT_TRUE(host);

    static const char kTestGalleries[] = "testGalleries(%d)";
    ResultCatcher catcher;
    host->render_view_host()->ExecuteJavascriptInWebFrame(
        string16(),
        ASCIIToUTF16(base::StringPrintf(kTestGalleries, expected_galleries)));
    EXPECT_TRUE(catcher.GetNextResult()) << message_;
  }
};

}  // namespace

IN_PROC_BROWSER_TEST_F(PlatformAppMediaGalleriesBrowserTest,
                       MediaGalleriesNoAccess) {
  chrome::EnsureMediaDirectoriesExists media_directories;
  ASSERT_TRUE(RunPlatformAppTest("api_test/media_galleries/no_access"))
      << message_;
  RunSecondTestPhase(media_directories.num_galleries());
}

IN_PROC_BROWSER_TEST_F(PlatformAppMediaGalleriesBrowserTest, NoGalleriesRead) {
  chrome::EnsureMediaDirectoriesExists media_directories;
  ASSERT_TRUE(RunPlatformAppTest("api_test/media_galleries/no_galleries"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(PlatformAppMediaGalleriesBrowserTest, NoGalleriesWrite) {
  chrome::EnsureMediaDirectoriesExists media_directories;
  ASSERT_TRUE(RunPlatformAppTest("api_test/media_galleries/no_galleries_write"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(PlatformAppMediaGalleriesBrowserTest,
                       MediaGalleriesRead) {
  chrome::EnsureMediaDirectoriesExists media_directories;
  ASSERT_TRUE(RunPlatformAppTest("api_test/media_galleries/read_access"))
      << message_;
  RunSecondTestPhase(media_directories.num_galleries());
}

IN_PROC_BROWSER_TEST_F(PlatformAppMediaGalleriesBrowserTest,
                       MediaGalleriesWrite) {
  chrome::EnsureMediaDirectoriesExists media_directories;
  ASSERT_TRUE(RunPlatformAppTest("api_test/media_galleries/write_access"))
      << message_;
  RunSecondTestPhase(media_directories.num_galleries());
}

IN_PROC_BROWSER_TEST_F(PlatformAppMediaGalleriesBrowserTest,
                       GetFilesystemMetadata) {
  chrome::EnsureMediaDirectoriesExists media_directories;
  ASSERT_TRUE(RunPlatformAppTest("api_test/media_galleries/metadata"))
      << message_;
}


IN_PROC_BROWSER_TEST_F(ExperimentalMediaGalleriesApiTest,
                       ExperimentalMediaGalleries) {
  ASSERT_TRUE(RunExtensionTest("media_galleries/experimental")) << message_;
}
