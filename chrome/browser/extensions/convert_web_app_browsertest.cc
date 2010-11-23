// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_type.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"

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
  browser()->profile()->GetExtensionsService()->set_show_extensions_prompts(
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
}
