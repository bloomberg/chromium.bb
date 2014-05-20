// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/screenlock_private/screenlock_private_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/common/signin_switches.h"
#include "content/public/browser/notification_service.h"
#include "extensions/browser/api/test/test_api.h"

namespace extensions {

namespace {

const char kTestUser[] = "testuser@gmail.com";
const char kAttemptClickAuthMessage[] = "attemptClickAuth";

}  // namespace

class ScreenlockPrivateApiTest : public ExtensionApiTest,
                                 public content::NotificationObserver {
 public:
  ScreenlockPrivateApiTest() {}

  virtual ~ScreenlockPrivateApiTest() {}

  // ExtensionApiTest
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);

#if !defined(OS_CHROMEOS)
    // New profile management needs to be on for non-ChromeOS lock.
    command_line->AppendSwitch(switches::kNewProfileManagement);
#endif
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    SigninManagerFactory::GetForProfile(profile())
        ->SetAuthenticatedUsername(kTestUser);
    ExtensionApiTest::SetUpOnMainThread();
  }

 protected:
  // ExtensionApiTest override:
  virtual void RunTestOnMainThreadLoop() OVERRIDE {
    registrar_.Add(this,
                   chrome::NOTIFICATION_EXTENSION_TEST_MESSAGE,
                   content::NotificationService::AllSources());
    ExtensionApiTest::RunTestOnMainThreadLoop();
    registrar_.RemoveAll();
  }

  // content::NotificationObserver override:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    const std::string& content = *content::Details<std::string>(details).ptr();
    if (content == kAttemptClickAuthMessage) {
      extensions::ScreenlockPrivateEventRouter* router =
          extensions::ScreenlockPrivateEventRouter::GetFactoryInstance()->Get(
              profile());
      router->OnAuthAttempted(
          ScreenlockBridge::Get()->lock_handler()->GetAuthType(kTestUser), "");
    }
  }

 private:
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ScreenlockPrivateApiTest);
};

IN_PROC_BROWSER_TEST_F(ScreenlockPrivateApiTest, LockUnlock) {
  ASSERT_TRUE(RunExtensionTest("screenlock_private/lock_unlock")) << message_;
}

IN_PROC_BROWSER_TEST_F(ScreenlockPrivateApiTest, AuthType) {
  ASSERT_TRUE(RunExtensionTest("screenlock_private/auth_type")) << message_;
}

}  // namespace extensions
