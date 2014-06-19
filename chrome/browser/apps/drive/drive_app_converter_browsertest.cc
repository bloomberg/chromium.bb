// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/drive/drive_app_converter.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/version.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::AppLaunchInfo;
using extensions::Extension;
using extensions::ExtensionRegistry;

namespace {

const char kAppName[] = "Test drive app";
const char kAppUrl[] = "http://foobar.com/drive_app";

}  // namespace

class DriveAppConverterTest : public ExtensionBrowserTest {
 public:
  DriveAppConverterTest() {}
  virtual ~DriveAppConverterTest() {}

  // ExtensionBrowserTest:
  virtual void SetUpOnMainThread() OVERRIDE {
    ExtensionBrowserTest::SetUpOnMainThread();

    base::FilePath test_data_dir;
    PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
    embedded_test_server()->ServeFilesFromDirectory(test_data_dir);
    ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  }

  void InstallAndWaitFinish(const drive::DriveAppInfo& drive_app) {
    runner_ = new content::MessageLoopRunner;

    converter_.reset(new DriveAppConverter(
        profile(),
        drive_app,
        base::Bind(&DriveAppConverterTest::ConverterFinished,
                   base::Unretained(this))));
    converter_->Start();

    runner_->Run();
  }

  GURL GetTestUrl(const std::string& path) {
    return embedded_test_server()->base_url().Resolve(path);
  }

  drive::DriveAppInfo GetTestDriveApp() {
    // Define four icons. icon1.png is 16x16 and good to use. icon2.png is
    // 16x16 but claims to be 32x32 and should be dropped. icon3.png is 66x66
    // and not a valid extension icon size and should be dropped too. The forth
    // one is icon2.png with 16x16 but should be ignored because 16x16 already
    // has icon1.png as its resource.
    drive::DriveAppInfo::IconList app_icons;
    app_icons.push_back(std::make_pair(16, GetTestUrl("extensions/icon1.png")));
    app_icons.push_back(std::make_pair(32, GetTestUrl("extensions/icon2.png")));
    app_icons.push_back(std::make_pair(66, GetTestUrl("extensions/icon3.png")));
    app_icons.push_back(std::make_pair(16, GetTestUrl("extensions/icon2.png")));

    drive::DriveAppInfo::IconList document_icons;

    return drive::DriveAppInfo("fake_drive_app_id",
                               "fake_product_id",
                               app_icons,
                               document_icons,
                               kAppName,
                               GURL(kAppUrl),
                               true);
  }

  const DriveAppConverter* converter() const { return converter_.get(); }

 private:
  void ConverterFinished(const DriveAppConverter* converter, bool success) {
    if (runner_)
      runner_->Quit();
  }

  scoped_ptr<DriveAppConverter> converter_;
  scoped_refptr<content::MessageLoopRunner> runner_;

  DISALLOW_COPY_AND_ASSIGN(DriveAppConverterTest);
};

IN_PROC_BROWSER_TEST_F(DriveAppConverterTest, GoodApp) {
  InstallAndWaitFinish(GetTestDriveApp());

  const Extension* app = converter()->extension();
  ASSERT_TRUE(app != NULL);
  EXPECT_EQ(kAppName, app->name());
  EXPECT_TRUE(app->is_hosted_app());
  EXPECT_TRUE(app->from_bookmark());
  EXPECT_EQ(GURL(kAppUrl), AppLaunchInfo::GetLaunchWebURL(app));
  EXPECT_EQ(extensions::LAUNCH_CONTAINER_TAB,
            AppLaunchInfo::GetLaunchContainer(app));
  EXPECT_EQ(0u, app->permissions_data()->active_permissions()->apis().size());
  EXPECT_EQ(1u, extensions::IconsInfo::GetIcons(app).map().size());

  const Extension* installed = extensions::ExtensionSystem::Get(profile())
                                   ->extension_service()
                                   ->GetInstalledExtension(app->id());
  EXPECT_EQ(app, installed);
  EXPECT_FALSE(extensions::util::ShouldSyncApp(app, profile()));
}

IN_PROC_BROWSER_TEST_F(DriveAppConverterTest, BadApp) {
  drive::DriveAppInfo no_name = GetTestDriveApp();
  no_name.app_name.clear();
  InstallAndWaitFinish(no_name);
  EXPECT_TRUE(converter()->extension() == NULL);

  drive::DriveAppInfo no_url = GetTestDriveApp();
  no_url.create_url = GURL();
  InstallAndWaitFinish(no_url);
  EXPECT_TRUE(converter()->extension() == NULL);
}

IN_PROC_BROWSER_TEST_F(DriveAppConverterTest, InstallTwice) {
  InstallAndWaitFinish(GetTestDriveApp());
  const Extension* first_install = converter()->extension();
  ASSERT_TRUE(first_install != NULL);
  EXPECT_TRUE(converter()->is_new_install());
  const std::string first_install_id = first_install->id();
  const base::Version first_install_version(first_install->VersionString());
  ASSERT_TRUE(first_install_version.IsValid());

  InstallAndWaitFinish(GetTestDriveApp());
  const Extension* second_install = converter()->extension();
  ASSERT_TRUE(second_install != NULL);
  EXPECT_FALSE(converter()->is_new_install());

  // Two different app instances.
  ASSERT_NE(first_install, second_install);
  EXPECT_EQ(first_install_id, second_install->id());
  EXPECT_GE(second_install->version()->CompareTo(first_install_version), 0);
}
