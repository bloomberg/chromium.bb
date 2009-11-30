// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_blacklist/blacklist_manager.h"

#include "base/command_line.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/privacy_blacklist/blacklist.h"
#include "chrome/browser/privacy_blacklist/blacklist_manager.h"
#include "chrome/browser/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_source.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

void GetBlacklistHasMatchOnIOThread(const BlacklistManager* manager,
                                    const GURL& url,
                                    bool *has_match) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  const Blacklist* blacklist = manager->GetCompiledBlacklist();
  scoped_ptr<Blacklist::Match> match(blacklist->findMatch(url));
  *has_match = (match.get() != NULL);
  ChromeThread::PostTask(ChromeThread::UI, FROM_HERE,
                         new MessageLoop::QuitTask());
}

}  // namespace

class BlacklistManagerBrowserTest : public ExtensionBrowserTest {
 public:
  virtual void SetUp() {
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnablePrivacyBlacklists);
    ExtensionBrowserTest::SetUp();
  }

  virtual void SetUpInProcessBrowserTestFixture() {
    ExtensionBrowserTest::SetUpInProcessBrowserTestFixture();

    received_blacklist_notification_ = false;
    host_resolver()->AddSimulatedFailure("www.example.com");
  }

  // NotificationObserver
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    if (type != NotificationType::BLACKLIST_MANAGER_ERROR &&
        type != NotificationType::BLACKLIST_MANAGER_BLACKLIST_READ_FINISHED) {
      ExtensionBrowserTest::Observe(type, source, details);
      return;
    }
    received_blacklist_notification_ = true;
    MessageLoop::current()->Quit();
  }

 protected:
  BlacklistManager* GetBlacklistManager() {
    return browser()->profile()->GetBlacklistManager();
  }

  bool GetAndResetReceivedBlacklistNotification() {
    bool result = received_blacklist_notification_;
    received_blacklist_notification_ = false;
    return result;
  }

  bool BlacklistHasMatch(const std::string& url) {
    bool has_match = false;
    ChromeThread::PostTask(ChromeThread::IO, FROM_HERE,
                           NewRunnableFunction(GetBlacklistHasMatchOnIOThread,
                                               GetBlacklistManager(),
                                               GURL(url),
                                               &has_match));
    ui_test_utils::RunMessageLoop();
    return has_match;
  }

 private:
  bool received_blacklist_notification_;
};

IN_PROC_BROWSER_TEST_F(BlacklistManagerBrowserTest, Basic) {
  static const char kTestUrl[] = "http://www.example.com/annoying_ads/ad.jpg";

  NotificationRegistrar registrar;
  registrar.Add(this,
                NotificationType::BLACKLIST_MANAGER_BLACKLIST_READ_FINISHED,
                Source<Profile>(browser()->profile()));

  // Test loading an extension with blacklist.
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("common").AppendASCII("privacy_blacklist")));

  // Wait until the blacklist is loaded and ready.
  if (!GetAndResetReceivedBlacklistNotification())
    ui_test_utils::RunMessageLoop();

  // The blacklist should block our test URL.
  EXPECT_TRUE(BlacklistHasMatch(kTestUrl));

  // TODO(phajdan.jr): Verify that we really blocked the request etc.
  ui_test_utils::NavigateToURL(browser(), GURL(kTestUrl));
}
