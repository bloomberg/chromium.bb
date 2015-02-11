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
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/search_engines/template_url_service.h"
#include "components/variations/entropy_provider.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace chrome {

using ChromeContentBrowserClientTest = testing::Test;

TEST_F(ChromeContentBrowserClientTest, ShouldAssignSiteForURL) {
  ChromeContentBrowserClient client;
  EXPECT_FALSE(client.ShouldAssignSiteForURL(GURL("chrome-native://test")));
  EXPECT_TRUE(client.ShouldAssignSiteForURL(GURL("http://www.google.com")));
  EXPECT_TRUE(client.ShouldAssignSiteForURL(GURL("https://www.google.com")));
}

// BrowserWithTestWindowTest doesn't work on iOS and Android.
#if !defined(OS_ANDROID) && !defined(OS_IOS)

using ChromeContentBrowserClientWindowTest = BrowserWithTestWindowTest;

static void DidOpenURLForWindowTest(content::WebContents** target_contents,
                                    content::WebContents* opened_contents) {
  DCHECK(target_contents);

  *target_contents = opened_contents;
}

// This test opens two URLs using ContentBrowserClient::OpenURL. It expects the
// URLs to be opened in new tabs and activated, changing the active tabs after
// each call and increasing the tab count by 2.
TEST_F(ChromeContentBrowserClientWindowTest, OpenURL) {
  ChromeContentBrowserClient client;

  int previous_count = browser()->tab_strip_model()->count();

  GURL urls[] = { GURL("https://www.google.com"),
                  GURL("https://www.chromium.org") };

  for (const GURL& url : urls) {
    content::OpenURLParams params(url,
                                  content::Referrer(),
                                  NEW_FOREGROUND_TAB,
                                  ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                  false);
    // TODO(peter): We should have more in-depth browser tests for the window
    // opening functionality, which also covers Android. This test can currently
    // only be ran on platforms where OpenURL is implemented synchronously.
    // See https://crbug.com/457667.
    content::WebContents* web_contents = nullptr;
    client.OpenURL(browser()->profile(),
                   params,
                   base::Bind(&DidOpenURLForWindowTest, &web_contents));

    EXPECT_TRUE(web_contents);

    content::WebContents* active_contents = browser()->tab_strip_model()->
        GetActiveWebContents();
    EXPECT_EQ(web_contents, active_contents);
    EXPECT_EQ(url, active_contents->GetVisibleURL());
  }

  EXPECT_EQ(previous_count + 2, browser()->tab_strip_model()->count());
}

#endif // !defined(OS_ANDROID) && !defined(OS_IOS)

#if defined(ENABLE_WEBRTC)

// NOTE: Any updates to the expectations in these tests should also be done in
// the browser test WebRtcDisableEncryptionFlagBrowserTest.
class DisableWebRtcEncryptionFlagTest : public testing::Test {
 public:
  DisableWebRtcEncryptionFlagTest()
      : from_command_line_(base::CommandLine::NO_PROGRAM),
        to_command_line_(base::CommandLine::NO_PROGRAM) {}

 protected:
  void SetUp() override {
    from_command_line_.AppendSwitch(switches::kDisableWebRtcEncryption);
  }

  void MaybeCopyDisableWebRtcEncryptionSwitch(VersionInfo::Channel channel) {
    ChromeContentBrowserClient::MaybeCopyDisableWebRtcEncryptionSwitch(
        &to_command_line_,
        from_command_line_,
        channel);
  }

  base::CommandLine from_command_line_;
  base::CommandLine to_command_line_;

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

#endif  // ENABLE_WEBRTC

}  // namespace chrome

#if !defined(OS_IOS) && !defined(OS_ANDROID)
namespace content {

class InstantNTPURLRewriteTest : public BrowserWithTestWindowTest {
 protected:
  void SetUp() override {
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

namespace chrome {

// For testing permissions related functionality.
class PermissionBrowserClientTest : public testing::Test {
 public:
  PermissionBrowserClientTest() : url_("https://www.google.com") {}

  void CheckPermissionStatus(content::PermissionType type,
                             content::PermissionStatus expected) {
    EXPECT_EQ(expected, client_.GetPermissionStatus(type, &profile_,
                                                    url_.GetOrigin(),
                                                    url_.GetOrigin()));
  }

  void SetPermission(ContentSettingsType type, ContentSetting value) {
    profile_.GetHostContentSettingsMap()->SetContentSetting(
        ContentSettingsPattern::FromURLNoWildcard(url_),
        ContentSettingsPattern::FromURLNoWildcard(url_),
        type, std::string(), value);
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  ChromeContentBrowserClient client_;
  TestingProfile profile_;
  GURL url_;
};

TEST_F(PermissionBrowserClientTest, GetPermissionStatusDefault) {
  using namespace content;
  CheckPermissionStatus(PERMISSION_MIDI_SYSEX, PERMISSION_STATUS_ASK);
  CheckPermissionStatus(PERMISSION_PUSH_MESSAGING, PERMISSION_STATUS_ASK);
  CheckPermissionStatus(PERMISSION_NOTIFICATIONS, PERMISSION_STATUS_ASK);
  CheckPermissionStatus(PERMISSION_GEOLOCATION, PERMISSION_STATUS_ASK);
#if defined(OS_ANDROID)
  CheckPermissionStatus(PERMISSION_PROTECTED_MEDIA_IDENTIFIER,
                        PERMISSION_STATUS_ASK);
#endif
}

TEST_F(PermissionBrowserClientTest, GetPermissionStatusAfterSet) {
  using namespace content;
  SetPermission(CONTENT_SETTINGS_TYPE_GEOLOCATION, CONTENT_SETTING_ALLOW);
  CheckPermissionStatus(PERMISSION_GEOLOCATION, PERMISSION_STATUS_GRANTED);

  SetPermission(CONTENT_SETTINGS_TYPE_NOTIFICATIONS, CONTENT_SETTING_ALLOW);
  CheckPermissionStatus(PERMISSION_NOTIFICATIONS, PERMISSION_STATUS_GRANTED);

  SetPermission(CONTENT_SETTINGS_TYPE_MIDI_SYSEX, CONTENT_SETTING_ALLOW);
  CheckPermissionStatus(PERMISSION_MIDI_SYSEX, PERMISSION_STATUS_GRANTED);

  SetPermission(CONTENT_SETTINGS_TYPE_PUSH_MESSAGING, CONTENT_SETTING_ALLOW);
  CheckPermissionStatus(PERMISSION_PUSH_MESSAGING, PERMISSION_STATUS_GRANTED);

#if defined(OS_ANDROID)
  SetPermission(CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER,
                CONTENT_SETTING_ALLOW);
  CheckPermissionStatus(PERMISSION_PROTECTED_MEDIA_IDENTIFIER,
                        PERMISSION_STATUS_GRANTED);
#endif
}

}  // namespace chrome
