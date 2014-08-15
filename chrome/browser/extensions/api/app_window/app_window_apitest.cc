// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/app_window.h"
#include "apps/app_window_registry.h"
#include "apps/ui/native_app_window.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/features/feature_channel.h"
#include "chrome/test/base/testing_profile.h"
#include "ui/base/base_window.h"
#include "ui/gfx/rect.h"

#if defined(OS_WIN)
#include "ui/base/win/shell.h"
#endif

using apps::AppWindow;

namespace {

class TestAppWindowRegistryObserver : public apps::AppWindowRegistry::Observer {
 public:
  explicit TestAppWindowRegistryObserver(Profile* profile)
      : profile_(profile), icon_updates_(0) {
    apps::AppWindowRegistry::Get(profile_)->AddObserver(this);
  }
  virtual ~TestAppWindowRegistryObserver() {
    apps::AppWindowRegistry::Get(profile_)->RemoveObserver(this);
  }

  // Overridden from AppWindowRegistry::Observer:
  virtual void OnAppWindowIconChanged(AppWindow* app_window) OVERRIDE {
    ++icon_updates_;
  }

  int icon_updates() { return icon_updates_; }

 private:
  Profile* profile_;
  int icon_updates_;

  DISALLOW_COPY_AND_ASSIGN(TestAppWindowRegistryObserver);
};

}  // namespace

namespace extensions {

// Tests chrome.app.window.setIcon.
IN_PROC_BROWSER_TEST_F(ExperimentalPlatformAppBrowserTest, WindowsApiSetIcon) {
  scoped_ptr<TestAppWindowRegistryObserver> test_observer(
      new TestAppWindowRegistryObserver(browser()->profile()));
  ExtensionTestMessageListener listener("ready", true);

  // Launch the app and wait for it to be ready.
  LoadAndLaunchPlatformApp("windows_api_set_icon", &listener);
  EXPECT_EQ(0, test_observer->icon_updates());
  listener.Reply("");

  // Now wait until the WebContent has decoded the icon and chrome has
  // processed it. This needs to be in a loop since the renderer runs in a
  // different process.
  while (test_observer->icon_updates() < 1) {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }
  AppWindow* app_window = GetFirstAppWindow();
  ASSERT_TRUE(app_window);
  EXPECT_NE(std::string::npos,
            app_window->app_icon_url().spec().find("icon.png"));
  EXPECT_EQ(1, test_observer->icon_updates());
}

// TODO(asargent) - Figure out what to do about the fact that minimize events
// don't work under ubuntu unity.
// (crbug.com/162794 and https://bugs.launchpad.net/unity/+bug/998073).
// TODO(linux_aura) http://crbug.com/163931
// Flaky on Mac, http://crbug.com/232330
#if defined(TOOLKIT_VIEWS) && !(defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(USE_AURA))

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, WindowsApiProperties) {
  EXPECT_TRUE(
      RunExtensionTest("platform_apps/windows_api_properties")) << message_;
}

#endif  // defined(TOOLKIT_VIEWS)

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest,
                       WindowsApiAlwaysOnTopWithPermissions) {
  EXPECT_TRUE(RunPlatformAppTest(
      "platform_apps/windows_api_always_on_top/has_permissions")) << message_;
}

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest,
                       WindowsApiAlwaysOnTopWithOldPermissions) {
  EXPECT_TRUE(RunPlatformAppTest(
      "platform_apps/windows_api_always_on_top/has_old_permissions"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest,
                       WindowsApiAlwaysOnTopNoPermissions) {
  EXPECT_TRUE(RunPlatformAppTest(
      "platform_apps/windows_api_always_on_top/no_permissions")) << message_;
}

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, WindowsApiGet) {
  EXPECT_TRUE(RunPlatformAppTest("platform_apps/windows_api_get"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, WindowsApiSetShape) {
  EXPECT_TRUE(RunPlatformAppTest("platform_apps/windows_api_shape"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest,
                       WindowsApiAlphaEnabledHasPermissions) {
  const char* no_alpha_dir =
      "platform_apps/windows_api_alpha_enabled/has_permissions_no_alpha";
  const char* test_dir = no_alpha_dir;

#if defined(USE_AURA) && (defined(OS_CHROMEOS) || !defined(OS_LINUX))
  test_dir =
      "platform_apps/windows_api_alpha_enabled/has_permissions_has_alpha";
#if defined(OS_WIN)
  if (!ui::win::IsAeroGlassEnabled()) {
    test_dir = no_alpha_dir;
  }
#endif  // OS_WIN
#endif  // USE_AURA && (OS_CHROMEOS || !OS_LINUX)

  EXPECT_TRUE(RunPlatformAppTest(test_dir)) << message_;
}

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest,
                       WindowsApiAlphaEnabledNoPermissions) {
  EXPECT_TRUE(RunPlatformAppTest(
      "platform_apps/windows_api_alpha_enabled/no_permissions"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, WindowsApiAlphaEnabledInStable) {
  extensions::ScopedCurrentChannel channel(chrome::VersionInfo::CHANNEL_STABLE);
  EXPECT_TRUE(RunPlatformAppTestWithFlags(
      "platform_apps/windows_api_alpha_enabled/in_stable",
      // Ignore manifest warnings because the extension will not load at all
      // in stable.
      kFlagIgnoreManifestWarnings))
      << message_;
}

IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest,
                       WindowsApiAlphaEnabledWrongFrameType) {
  EXPECT_TRUE(RunPlatformAppTest(
      "platform_apps/windows_api_alpha_enabled/wrong_frame_type"))
      << message_;
}

}  // namespace extensions
