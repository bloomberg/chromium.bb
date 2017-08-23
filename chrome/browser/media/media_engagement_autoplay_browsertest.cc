// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/chrome_content_settings_utils.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/media/media_engagement_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/renderer_configuration.mojom.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/media_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace {

const char kMediaEngagementTestDataPath[] = "chrome/test/data/media/engagement";

const base::string16 kAllowedTitle = base::ASCIIToUTF16("Allowed");

const base::string16 kDeniedTitle = base::ASCIIToUTF16("Denied");

}  // namespace

// Class used to test that origins with a high Media Engagement score
// can bypass autoplay policies.
class MediaEngagementAutoplayBrowserTest : public InProcessBrowserTest {
 public:
  MediaEngagementAutoplayBrowserTest() {
    http_server_.ServeFilesFromSourceDirectory(kMediaEngagementTestDataPath);
    http_server_origin2_.ServeFilesFromSourceDirectory(
        kMediaEngagementTestDataPath);
  }

  ~MediaEngagementAutoplayBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        switches::kAutoplayPolicy,
        switches::autoplay::kUserGestureRequiredPolicy);
  }

  void SetUp() override {
    ASSERT_TRUE(http_server_.Start());
    ASSERT_TRUE(http_server_origin2_.Start());

    scoped_feature_list_.InitWithFeatures(
        {media::kRecordMediaEngagementScores,
         media::kMediaEngagementBypassAutoplayPolicies},
        {});

    InProcessBrowserTest::SetUp();
  };

  void LoadTestPage(const std::string& page) {
    ui_test_utils::NavigateToURL(browser(), http_server_.GetURL("/" + page));
  }

  void LoadTestPageSecondaryOrigin(const std::string& page) {
    ui_test_utils::NavigateToURL(browser(),
                                 http_server_origin2_.GetURL("/" + page));
  }

  void LoadSubFrame(const std::string& page) {
    EXPECT_TRUE(content::ExecuteScript(
        GetWebContents(), "window.open(\"" +
                              http_server_origin2_.GetURL("/" + page).spec() +
                              "\", \"subframe\")"));
  }

  void SetScores(GURL url, int visits, int media_playbacks) {
    MediaEngagementScore score = GetService()->CreateEngagementScore(url);
    score.SetVisits(visits);
    score.SetMediaPlaybacks(media_playbacks);
    score.Commit();
  }

  GURL PrimaryOrigin() { return http_server_.GetURL("/"); }

  GURL SecondaryOrigin() { return http_server_origin2_.GetURL("/"); }

  void ExpectAutoplayAllowed() { EXPECT_EQ(kAllowedTitle, WaitAndGetTitle()); }

  void ExpectAutoplayDenied() { EXPECT_EQ(kDeniedTitle, WaitAndGetTitle()); }

 private:
  base::string16 WaitAndGetTitle() {
    content::TitleWatcher title_watcher(GetWebContents(), kAllowedTitle);
    title_watcher.AlsoWaitForTitle(kDeniedTitle);
    return title_watcher.WaitAndGetTitle();
  }

  MediaEngagementService* GetService() {
    return MediaEngagementService::Get(browser()->profile());
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  net::EmbeddedTestServer http_server_;
  net::EmbeddedTestServer http_server_origin2_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(MediaEngagementAutoplayBrowserTest,
                       BypassAutoplayHighEngagement) {
  SetScores(PrimaryOrigin(), 7, 7);
  LoadTestPage("engagement_autoplay_test.html");
  ExpectAutoplayAllowed();
}

IN_PROC_BROWSER_TEST_F(MediaEngagementAutoplayBrowserTest,
                       DoNotBypassAutoplayLowEngagement) {
  SetScores(PrimaryOrigin(), 1, 1);
  LoadTestPage("engagement_autoplay_test.html");
  ExpectAutoplayDenied();
}

IN_PROC_BROWSER_TEST_F(MediaEngagementAutoplayBrowserTest,
                       DoNotBypassAutoplayNoEngagement) {
  LoadTestPage("engagement_autoplay_test.html");
  ExpectAutoplayDenied();
}

IN_PROC_BROWSER_TEST_F(MediaEngagementAutoplayBrowserTest,
                       BypassAutoplayFrameHighEngagement) {
  SetScores(PrimaryOrigin(), 7, 7);
  LoadTestPage("engagement_autoplay_iframe_test.html");
  LoadSubFrame("engagement_autoplay_iframe_test_frame.html");
  ExpectAutoplayAllowed();
}

IN_PROC_BROWSER_TEST_F(MediaEngagementAutoplayBrowserTest,
                       DoNotBypassAutoplayFrameLowEngagement) {
  SetScores(SecondaryOrigin(), 7, 7);
  LoadTestPage("engagement_autoplay_iframe_test.html");
  LoadSubFrame("engagement_autoplay_iframe_test_frame.html");
  ExpectAutoplayDenied();
}

IN_PROC_BROWSER_TEST_F(MediaEngagementAutoplayBrowserTest,
                       DoNotBypassAutoplayFrameNoEngagement) {
  LoadTestPage("engagement_autoplay_iframe_test.html");
  LoadSubFrame("engagement_autoplay_iframe_test_frame.html");
  ExpectAutoplayDenied();
}

IN_PROC_BROWSER_TEST_F(MediaEngagementAutoplayBrowserTest,
                       ClearEngagementOnNavigation) {
  SetScores(PrimaryOrigin(), 7, 7);
  LoadTestPage("engagement_autoplay_test.html");
  ExpectAutoplayAllowed();

  LoadTestPageSecondaryOrigin("engagement_autoplay_test.html");
  ExpectAutoplayDenied();
  SetScores(SecondaryOrigin(), 7, 7);

  LoadTestPage("engagement_autoplay_test.html");
  ExpectAutoplayAllowed();

  LoadTestPageSecondaryOrigin("engagement_autoplay_test.html");
  ExpectAutoplayAllowed();
}

// Test have high score threshold.
IN_PROC_BROWSER_TEST_F(MediaEngagementAutoplayBrowserTest,
                       HasHighScoreThreshold) {
  SetScores(PrimaryOrigin(), 10, 8);
  SetScores(PrimaryOrigin(), 10, 5);
  LoadTestPage("engagement_autoplay_test.html");
  ExpectAutoplayAllowed();
}
