// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/stringprintf.h"
#if defined (OS_WIN)
#include "base/win/windows_version.h"
#endif  // defined (OS_WIN)

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_webstore_private_api.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "content/common/notification_service.h"
#include "net/base/mock_host_resolver.h"

// This is a helper class to let us automatically accept extension install
// dialogs.
class GalleryInstallApiTestObserver :
    public base::RefCounted<GalleryInstallApiTestObserver>,
    public NotificationObserver {
 public:
  GalleryInstallApiTestObserver() {
    registrar_.Add(this,
                  chrome::NOTIFICATION_EXTENSION_WILL_SHOW_CONFIRM_DIALOG,
                  NotificationService::AllSources());
  }

  void InstallUIProceed(ExtensionInstallUI::Delegate* delegate) {
    delegate->InstallUIProceed();
  }

  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE {
    ExtensionInstallUI* prompt = Source<ExtensionInstallUI>(source).ptr();
    CHECK(prompt->delegate_);
    MessageLoop::current()->PostTask(
        FROM_HERE, NewRunnableMethod(
            this,
            &GalleryInstallApiTestObserver::InstallUIProceed,
            prompt->delegate_));
  }

 private:
  NotificationRegistrar registrar_;
};

class ExtensionGalleryInstallApiTest : public ExtensionApiTest {
 public:
  void SetUpCommandLine(CommandLine* command_line) {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kAppsGalleryURL,
        "http://www.example.com");
  }

  bool RunInstallTest(const std::string& page) {
    scoped_refptr<GalleryInstallApiTestObserver> observer =
        new GalleryInstallApiTestObserver();  // Responds to install dialog.
    std::string base_url = base::StringPrintf(
        "http://www.example.com:%u/files/extensions/",
        test_server()->host_port_pair().port());

    std::string testing_install_base_url = base_url;
    testing_install_base_url += "good.crx";

    CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kAppsGalleryUpdateURL, testing_install_base_url);

    std::string page_url = base_url;
    page_url += "api_test/extension_gallery_install/" + page;

    return RunPageTest(page_url.c_str());
  }
};

namespace {

bool RunningOnXP() {
#if defined (OS_WIN)
  return GetVersion() < base::win::VERSION_VISTA;
#else
  return false;
#endif  // defined (OS_WIN)
}

}  // namespace

// TODO(asargent) - for some reason this test occasionally fails on XP,
// but not other versions of windows. http://crbug.com/55642
#if defined (OS_WIN)
#define MAYBE_InstallAndUninstall DISABLED_InstallAndUninstall
#else
#define MAYBE_InstallAndUninstall InstallAndUninstall
#endif
IN_PROC_BROWSER_TEST_F(ExtensionGalleryInstallApiTest,
                       MAYBE_InstallAndUninstall) {
  if (RunningOnXP()) {
    LOG(INFO) << "Adding host resolver rule";
  }
  host_resolver()->AddRule("www.example.com", "127.0.0.1");
  if (RunningOnXP()) {
    LOG(INFO) << "Starting test server";
  }
  ASSERT_TRUE(test_server()->Start());

  if (RunningOnXP()) {
    LOG(INFO) << "Starting tests without user gesture checking";
  }
  BeginInstallFunction::SetIgnoreUserGestureForTests(true);
  ASSERT_TRUE(RunInstallTest("test.html"));
  ASSERT_TRUE(RunInstallTest("complete_without_begin.html"));
  ASSERT_TRUE(RunInstallTest("invalid_begin.html"));

  if (RunningOnXP()) {
    LOG(INFO) << "Starting tests with user gesture checking";
  }
  BeginInstallFunction::SetIgnoreUserGestureForTests(false);
  ASSERT_TRUE(RunInstallTest("no_user_gesture.html"));
}
