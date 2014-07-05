// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_content_browser_client.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/search_engines/template_url_service.h"
#include "components/variations/entropy_provider.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace chrome {

typedef testing::Test ChromeContentBrowserClientTest;

TEST_F(ChromeContentBrowserClientTest, ShouldAssignSiteForURL) {
  ChromeContentBrowserClient client;
  EXPECT_FALSE(client.ShouldAssignSiteForURL(GURL("chrome-native://test")));
  EXPECT_TRUE(client.ShouldAssignSiteForURL(GURL("http://www.google.com")));
  EXPECT_TRUE(client.ShouldAssignSiteForURL(GURL("https://www.google.com")));
}

// NOTE: Any updates to the expectations in these tests should also be done in
// the browser test WebRtcDisableEncryptionFlagBrowserTest.
class DisableWebRtcEncryptionFlagTest : public testing::Test {
 public:
  DisableWebRtcEncryptionFlagTest()
      : from_command_line_(CommandLine::NO_PROGRAM),
        to_command_line_(CommandLine::NO_PROGRAM) {}

 protected:
  virtual void SetUp() {
    from_command_line_.AppendSwitch(switches::kDisableWebRtcEncryption);
  }

  void MaybeCopyDisableWebRtcEncryptionSwitch(VersionInfo::Channel channel) {
    ChromeContentBrowserClient::MaybeCopyDisableWebRtcEncryptionSwitch(
        &to_command_line_,
        from_command_line_,
        channel);
  }

  CommandLine from_command_line_;
  CommandLine to_command_line_;

  DISALLOW_COPY_AND_ASSIGN(DisableWebRtcEncryptionFlagTest);
};

TEST_F(DisableWebRtcEncryptionFlagTest, UnknownChannel) {
  MaybeCopyDisableWebRtcEncryptionSwitch(VersionInfo::CHANNEL_UNKNOWN);
  EXPECT_TRUE(to_command_line_.HasSwitch(switches::kDisableWebRtcEncryption));
}

TEST_F(DisableWebRtcEncryptionFlagTest, CanaryChannel) {
  MaybeCopyDisableWebRtcEncryptionSwitch(VersionInfo::CHANNEL_CANARY);
  EXPECT_TRUE(to_command_line_.HasSwitch(switches::kDisableWebRtcEncryption));
}

TEST_F(DisableWebRtcEncryptionFlagTest, DevChannel) {
  MaybeCopyDisableWebRtcEncryptionSwitch(VersionInfo::CHANNEL_DEV);
  EXPECT_TRUE(to_command_line_.HasSwitch(switches::kDisableWebRtcEncryption));
}

TEST_F(DisableWebRtcEncryptionFlagTest, BetaChannel) {
  MaybeCopyDisableWebRtcEncryptionSwitch(VersionInfo::CHANNEL_BETA);
#if defined(OS_ANDROID)
  EXPECT_TRUE(to_command_line_.HasSwitch(switches::kDisableWebRtcEncryption));
#else
  EXPECT_FALSE(to_command_line_.HasSwitch(switches::kDisableWebRtcEncryption));
#endif
}

TEST_F(DisableWebRtcEncryptionFlagTest, StableChannel) {
  MaybeCopyDisableWebRtcEncryptionSwitch(VersionInfo::CHANNEL_STABLE);
  EXPECT_FALSE(to_command_line_.HasSwitch(switches::kDisableWebRtcEncryption));
}

}  // namespace chrome

#if !defined(OS_IOS) && !defined(OS_ANDROID)
namespace content {

class InstantNTPURLRewriteTest : public BrowserWithTestWindowTest {
 protected:
  virtual void SetUp() OVERRIDE {
    BrowserWithTestWindowTest::SetUp();
    field_trial_list_.reset(new base::FieldTrialList(
        new metrics::SHA1EntropyProvider("42")));
  }

  void InstallTemplateURLWithNewTabPage(GURL new_tab_page_url) {
    TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(), &TemplateURLServiceFactory::BuildInstanceFor);
    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(browser()->profile());
    ui_test_utils::WaitForTemplateURLServiceToLoad(template_url_service);

    TemplateURLData data;
    data.SetURL("http://foo.com/url?bar={searchTerms}");
    data.new_tab_url = new_tab_page_url.spec();
    TemplateURL* template_url = new TemplateURL(data);
    // Takes ownership.
    template_url_service->Add(template_url);
    template_url_service->SetUserSelectedDefaultSearchProvider(template_url);
  }

  scoped_ptr<base::FieldTrialList> field_trial_list_;
};

TEST_F(InstantNTPURLRewriteTest, UberURLHandler_InstantExtendedNewTabPage) {
  const GURL url_original("chrome://newtab");
  const GURL url_rewritten("https://www.example.com/newtab");
  InstallTemplateURLWithNewTabPage(url_rewritten);
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial("InstantExtended",
      "Group1 use_cacheable_ntp:1"));

  AddTab(browser(), GURL("chrome://blank"));
  NavigateAndCommitActiveTab(url_original);

  NavigationEntry* entry = browser()->tab_strip_model()->
      GetActiveWebContents()->GetController().GetLastCommittedEntry();
  ASSERT_TRUE(entry != NULL);
  EXPECT_EQ(url_rewritten, entry->GetURL());
  EXPECT_EQ(url_original, entry->GetVirtualURL());
}

}  // namespace content
#endif  // !defined(OS_IOS) && !defined(OS_ANDROID)
