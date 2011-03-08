// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "content/common/notification_details.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "content/common/notification_type.h"

class ExtensionFromWebAppTest
    : public InProcessBrowserTest, public NotificationObserver {
 protected:
  ExtensionFromWebAppTest() : installed_extension_(NULL) {
  }

  std::string expected_extension_id_;
  const Extension* installed_extension_;

 private:
  // InProcessBrowserTest
  virtual void SetUpCommandLine(CommandLine* command_line) {
    command_line->AppendSwitch(switches::kEnableCrxlessWebApps);
  }

  // NotificationObserver
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    if (type == NotificationType::EXTENSION_INSTALLED) {
      const Extension* extension = Details<const Extension>(details).ptr();
      if (extension->id() == expected_extension_id_) {
        installed_extension_ = extension;
        MessageLoopForUI::current()->Quit();
      }
    }
  }
};

IN_PROC_BROWSER_TEST_F(ExtensionFromWebAppTest, Basic) {
  ASSERT_TRUE(test_server()->Start());
  browser()->profile()->GetExtensionService()->set_show_extensions_prompts(
      false);

  NotificationRegistrar registrar;
  registrar.Add(this, NotificationType::EXTENSION_INSTALLED,
                NotificationService::AllSources());

  expected_extension_id_ = "fnpgoaochgbdfjndakichfafiocjjpmm";
  ui_test_utils::NavigateToURL(
      browser(),
      test_server()->GetURL(
          "files/extensions/convert_web_app/application.html"));

  if (!installed_extension_)
    ui_test_utils::RunMessageLoop();

  EXPECT_TRUE(installed_extension_);
  EXPECT_TRUE(installed_extension_->is_hosted_app());
  EXPECT_EQ("Test application", installed_extension_->name());
  EXPECT_EQ("the description is", installed_extension_->description());
  EXPECT_EQ(extension_misc::LAUNCH_PANEL,
            installed_extension_->launch_container());

  ASSERT_EQ(2u, installed_extension_->api_permissions().size());
  EXPECT_TRUE(installed_extension_->api_permissions().find("geolocation") !=
              installed_extension_->api_permissions().end());
  EXPECT_TRUE(installed_extension_->api_permissions().find("notifications") !=
              installed_extension_->api_permissions().end());

  ASSERT_EQ(3u, installed_extension_->icons().map().size());
  EXPECT_EQ("icons/16.png", installed_extension_->icons().Get(
      16, ExtensionIconSet::MATCH_EXACTLY));
  EXPECT_EQ("icons/48.png", installed_extension_->icons().Get(
      48, ExtensionIconSet::MATCH_EXACTLY));
  EXPECT_EQ("icons/128.png", installed_extension_->icons().Get(
      128, ExtensionIconSet::MATCH_EXACTLY));
}
