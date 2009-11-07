// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_blacklist/blacklist_manager.h"

#include "chrome/browser/browser.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profile.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_source.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Returns true if |blacklist| contains a match for |url|.
bool BlacklistHasMatch(const Blacklist* blacklist, const char* url) {
  Blacklist::Match* match = blacklist->findMatch(GURL(url));

  if (!match)
    return false;

  delete match;
  return true;
}

}  // namespace

class BlacklistManagerBrowserTest : public ExtensionBrowserTest {
 public:
  void InitializeBlacklistManager() {
    Profile* profile = browser()->profile();
    blacklist_manager_ = new BlacklistManager(profile,
                                              profile->GetExtensionsService());
    WaitForBlacklistUpdate();
  }

  virtual void CleanUpOnMainThread() {
    blacklist_manager_ = NULL;
    ExtensionBrowserTest::CleanUpOnMainThread();
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
    MessageLoop::current()->Quit();
  }

 protected:
  void WaitForBlacklistUpdate() {
    NotificationRegistrar registrar;
    registrar.Add(this,
                  NotificationType::BLACKLIST_MANAGER_BLACKLIST_READ_FINISHED,
                  Source<Profile>(browser()->profile()));
    ui_test_utils::RunMessageLoop();
  }

  scoped_refptr<BlacklistManager> blacklist_manager_;
};

IN_PROC_BROWSER_TEST_F(BlacklistManagerBrowserTest, Basic) {
  static const char kTestUrl[] = "http://host/annoying_ads/ad.jpg";

  InitializeBlacklistManager();
  ASSERT_TRUE(blacklist_manager_->GetCompiledBlacklist());
  EXPECT_FALSE(BlacklistHasMatch(blacklist_manager_->GetCompiledBlacklist(),
                                 kTestUrl));

  // Test loading an extension with blacklist.
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("common").AppendASCII("privacy_blacklist")));
  if (!BlacklistHasMatch(blacklist_manager_->GetCompiledBlacklist(),
                         kTestUrl)) {
    WaitForBlacklistUpdate();
  }
  EXPECT_TRUE(BlacklistHasMatch(blacklist_manager_->GetCompiledBlacklist(),
                                kTestUrl));

  // Make sure that after unloading the extension we update the blacklist.
  ExtensionsService* extensions_service =
      browser()->profile()->GetExtensionsService();
  ASSERT_EQ(1U, extensions_service->extensions()->size());
  UnloadExtension(extensions_service->extensions()->front()->id());
  if (BlacklistHasMatch(blacklist_manager_->GetCompiledBlacklist(),
                        kTestUrl)) {
    WaitForBlacklistUpdate();
  }
  EXPECT_FALSE(BlacklistHasMatch(blacklist_manager_->GetCompiledBlacklist(),
                                 kTestUrl));
}
