// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_resource_throttle.h"

#include <memory>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/page_type.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

using content::NavigationController;
using content::WebContents;

namespace {

static const char* kExampleHost = "www.example.com";
static const char* kExampleHost2 = "www.example2.com";

static const char* kIframeHost2 = "www.iframe2.com";

}  // namespace

class SupervisedUserResourceThrottleTest : public InProcessBrowserTest {
 protected:
  SupervisedUserResourceThrottleTest() {}
  ~SupervisedUserResourceThrottleTest() override {}

  void BlockHost(const std::string& host) {
    Profile* profile = browser()->profile();
    SupervisedUserSettingsService* settings_service =
        SupervisedUserSettingsServiceFactory::GetForProfile(profile);
    auto dict = base::MakeUnique<base::DictionaryValue>();
    dict->SetBooleanWithoutPathExpansion(host, false);
    settings_service->SetLocalSetting(
        supervised_users::kContentPackManualBehaviorHosts, std::move(dict));
  }

 private:
  void SetUpOnMainThread() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;
};

void SupervisedUserResourceThrottleTest::SetUpOnMainThread() {
  // Resolve everything to localhost.
  host_resolver()->AddIPLiteralRule("*", "127.0.0.1", "localhost");

  ASSERT_TRUE(embedded_test_server()->Start());
}

void SupervisedUserResourceThrottleTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  command_line->AppendSwitchASCII(switches::kSupervisedUserId, "asdf");
}

// Tests that showing the blocking interstitial for a WebContents without a
// SupervisedUserNavigationObserver doesn't crash.
IN_PROC_BROWSER_TEST_F(SupervisedUserResourceThrottleTest,
                       NoNavigationObserverBlock) {
  Profile* profile = browser()->profile();
  SupervisedUserSettingsService* supervised_user_settings_service =
      SupervisedUserSettingsServiceFactory::GetForProfile(profile);
  supervised_user_settings_service->SetLocalSetting(
      supervised_users::kContentPackDefaultFilteringBehavior,
      std::unique_ptr<base::Value>(
          new base::Value(SupervisedUserURLFilter::BLOCK)));

  std::unique_ptr<WebContents> web_contents(
      WebContents::Create(WebContents::CreateParams(profile)));
  NavigationController& controller = web_contents->GetController();
  content::TestNavigationObserver observer(web_contents.get());
  controller.LoadURL(GURL("http://www.example.com"), content::Referrer(),
                     ui::PAGE_TRANSITION_TYPED, std::string());
  observer.Wait();
  content::NavigationEntry* entry = controller.GetActiveEntry();
  ASSERT_TRUE(entry);
  EXPECT_EQ(content::PAGE_TYPE_INTERSTITIAL, entry->GetPageType());
}

IN_PROC_BROWSER_TEST_F(SupervisedUserResourceThrottleTest,
                       BlockMainFrameWithInterstitial) {
  BlockHost(kExampleHost2);

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  GURL allowed_url = embedded_test_server()->GetURL(
      kExampleHost, "/supervised_user/simple.html");
  ui_test_utils::NavigateToURL(browser(), allowed_url);
  EXPECT_FALSE(tab->ShowingInterstitialPage());

  GURL blocked_url = embedded_test_server()->GetURL(
      kExampleHost2, "/supervised_user/simple.html");
  ui_test_utils::NavigateToURL(browser(), blocked_url);
  EXPECT_TRUE(tab->ShowingInterstitialPage());
}

IN_PROC_BROWSER_TEST_F(SupervisedUserResourceThrottleTest, DontBlockSubFrame) {
  BlockHost(kExampleHost2);
  BlockHost(kIframeHost2);

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  GURL allowed_url_with_iframes = embedded_test_server()->GetURL(
      kExampleHost, "/supervised_user/with_iframes.html");
  ui_test_utils::NavigateToURL(browser(), allowed_url_with_iframes);
  EXPECT_FALSE(tab->ShowingInterstitialPage());

  // Both iframes (from allowed host iframe1.com as well as from blocked host
  // iframe2.com) should be loaded normally, since we don't filter iframes
  // (yet) - see crbug.com/651115.
  bool loaded1 = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(tab, "loaded1()", &loaded1));
  EXPECT_TRUE(loaded1);
  bool loaded2 = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(tab, "loaded2()", &loaded2));
  EXPECT_TRUE(loaded2);
}
