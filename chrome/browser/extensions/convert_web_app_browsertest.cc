// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/api/icons/icons_handler.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/extensions/permissions/permission_set.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"

namespace extensions {

class ExtensionFromWebAppTest
    : public InProcessBrowserTest, public content::NotificationObserver {
 protected:
  ExtensionFromWebAppTest() : installed_extension_(NULL) {
  }

  std::string expected_extension_id_;
  const Extension* installed_extension_;

 private:
  // InProcessBrowserTest
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kEnableCrxlessWebApps);
  }

  // content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    if (type == chrome::NOTIFICATION_EXTENSION_INSTALLED) {
      const Extension* extension =
          content::Details<const Extension>(details).ptr();
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

  content::NotificationRegistrar registrar;
  registrar.Add(this, chrome::NOTIFICATION_EXTENSION_INSTALLED,
                content::NotificationService::AllSources());

  expected_extension_id_ = "fnpgoaochgbdfjndakichfafiocjjpmm";
  ui_test_utils::NavigateToURL(
      browser(),
      test_server()->GetURL(
          "files/extensions/convert_web_app/application.html"));

  if (!installed_extension_)
    content::RunMessageLoop();

  EXPECT_TRUE(installed_extension_);
  EXPECT_TRUE(installed_extension_->is_hosted_app());
  EXPECT_EQ("Test application", installed_extension_->name());
  EXPECT_EQ("the description is", installed_extension_->description());
  EXPECT_EQ(extension_misc::LAUNCH_PANEL,
            installed_extension_->launch_container());

  ASSERT_EQ(2u, installed_extension_->GetActivePermissions()->apis().size());
  EXPECT_TRUE(installed_extension_->HasAPIPermission(
      APIPermission::kGeolocation));
  EXPECT_TRUE(installed_extension_->HasAPIPermission(
      APIPermission::kNotification));

  const ExtensionIconSet& icons = IconsInfo::GetIcons(installed_extension_);
  ASSERT_EQ(3u, icons.map().size());
  EXPECT_EQ("icons/16.png", icons.Get(16, ExtensionIconSet::MATCH_EXACTLY));
  EXPECT_EQ("icons/48.png", icons.Get(48, ExtensionIconSet::MATCH_EXACTLY));
  EXPECT_EQ("icons/128.png", icons.Get(128, ExtensionIconSet::MATCH_EXACTLY));
}

}  // namespace extensions
