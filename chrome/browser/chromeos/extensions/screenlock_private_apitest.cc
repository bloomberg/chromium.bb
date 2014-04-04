// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/screen_locker.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/browser/notification_service.h"
#include "extensions/browser/api/test/test_api.h"

namespace extensions {

namespace {

const char kAttemptClickAuthMessage[] = "attemptClickAuth";

}  // namespace

class ScreenlockPrivateApiTest : public ExtensionApiTest,
                                 public content::NotificationObserver {
 public:
  ScreenlockPrivateApiTest() {}

  virtual ~ScreenlockPrivateApiTest() {}

 protected:
  chromeos::ScreenLocker* GetScreenLocker() {
    chromeos::ScreenLocker* locker =
        chromeos::ScreenLocker::default_screen_locker();
    EXPECT_TRUE(locker);
    return locker;
  }

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
      chromeos::ScreenLocker* locker = GetScreenLocker();
      const chromeos::UserList& users = locker->users();
      EXPECT_GE(1u, users.size());
      GetScreenLocker()->Authenticate(
          chromeos::UserContext(users[0]->email(), "", ""));
    }
  }

  content::NotificationRegistrar registrar_;
};

IN_PROC_BROWSER_TEST_F(ScreenlockPrivateApiTest, LockUnlock) {
  ASSERT_TRUE(RunExtensionTest("screenlock_private/lock_unlock")) << message_;
}

IN_PROC_BROWSER_TEST_F(ScreenlockPrivateApiTest, AuthType) {
  ASSERT_TRUE(RunExtensionTest("screenlock_private/auth_type")) << message_;
}

}  // namespace extensions
