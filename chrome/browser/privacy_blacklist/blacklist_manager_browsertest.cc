// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_blacklist/blacklist_manager.h"

#include "app/l10n_util.h"
#include "base/command_line.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/privacy_blacklist/blacklist.h"
#include "chrome/browser/privacy_blacklist/blacklist_manager.h"
#include "chrome/browser/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "net/base/mock_host_resolver.h"
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

    host_resolver()->AddSimulatedFailure("www.example.com");
  }

 protected:
  BlacklistManager* GetBlacklistManager() {
    return browser()->profile()->GetBlacklistManager();
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
};

IN_PROC_BROWSER_TEST_F(BlacklistManagerBrowserTest, Basic) {
  static const char kTestUrl[] = "http://www.example.com/annoying_ads/ad.jpg";

  // Test loading an extension with blacklist.
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("common").AppendASCII("privacy_blacklist")));

  // Navigate to a blacklisted URL. The request should be blocked.
  ui_test_utils::NavigateToURL(browser(), GURL(kTestUrl));
  string16 expected_title(l10n_util::GetStringUTF16(IDS_BLACKLIST_TITLE));
  string16 tab_title;
  ASSERT_TRUE(ui_test_utils::GetCurrentTabTitle(browser(), &tab_title));
  EXPECT_EQ(expected_title, tab_title);
  EXPECT_TRUE(BlacklistHasMatch(kTestUrl));
}
