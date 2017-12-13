// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/chrome_content_settings_utils.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/media/media_engagement_preloaded_list.h"
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

base::FilePath GetPythonPath() {
#if defined(OS_WIN)
  // Windows bots do not have python installed and available on the PATH.
  // Please see infra/doc/users/python.md
  base::FilePath bot_path =
      base::FilePath(FILE_PATH_LITERAL("c:/infra-system/bin/python.exe"));

  if (base::PathExists(bot_path))
    return bot_path;
  return base::FilePath(FILE_PATH_LITERAL("python.exe"));
#else
  return base::FilePath(FILE_PATH_LITERAL("python"));
#endif
}

const base::FilePath kTestDataPath = base::FilePath(
    FILE_PATH_LITERAL("chrome/test/data/media/engagement/preload"));

const char kMediaEngagementTestDataPath[] = "chrome/test/data/media/engagement";

const base::string16 kAllowedTitle = base::ASCIIToUTF16("Allowed");

const base::string16 kDeniedTitle = base::ASCIIToUTF16("Denied");

const base::FilePath kEmptyDataPath = kTestDataPath.AppendASCII("empty.pb");

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
         media::kMediaEngagementBypassAutoplayPolicies,
         media::kPreloadMediaEngagementData},
        {});

    InProcessBrowserTest::SetUp();

    // Clear any preloaded MEI data.
    ApplyEmptyPreloadedList();
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

  void ApplyPreloadedOrigin(GURL url) {
    base::ScopedAllowBlockingForTesting allow_blocking;

    // Get two temporary files.
    base::FilePath input_path;
    base::FilePath output_path;
    EXPECT_TRUE(base::CreateTemporaryFile(&input_path));
    EXPECT_TRUE(base::CreateTemporaryFile(&output_path));

    // Write JSON file with the server origin in it.
    base::ListValue list;
    list.AppendString(url::Origin::Create(url).Serialize());
    std::string json_data;
    base::JSONWriter::Write(list, &json_data);
    EXPECT_TRUE(
        base::WriteFile(input_path, json_data.c_str(), json_data.size()));

    // Get the path to the "generator" binary in the module path.
    base::FilePath module_dir;
    EXPECT_TRUE(PathService::Get(base::DIR_MODULE, &module_dir));

    // Launch the generator and wait for it to finish.
    base::CommandLine cmd(GetPythonPath());
    cmd.AppendArg("tools/media_engagement_preload/make_dafsa.py");
    cmd.AppendArgPath(input_path);
    cmd.AppendArgPath(output_path);
    base::Process process = base::LaunchProcess(cmd, base::LaunchOptions());
    EXPECT_TRUE(process.WaitForExit(0));

    // Load the preloaded list.
    EXPECT_TRUE(
        MediaEngagementPreloadedList::GetInstance()->LoadFromFile(output_path));
  }

  void ApplyEmptyPreloadedList() {
    // Get the path relative to the source root.
    base::FilePath source_root;
    EXPECT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &source_root));

    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(MediaEngagementPreloadedList::GetInstance()->LoadFromFile(
        source_root.Append(kEmptyDataPath)));
  }

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

IN_PROC_BROWSER_TEST_F(MediaEngagementAutoplayBrowserTest,
                       UsePreloadedData_Allowed) {
  // Autoplay should be blocked by default if we have a bad score.
  SetScores(PrimaryOrigin(), 0, 0);
  LoadTestPage("engagement_autoplay_test.html");
  ExpectAutoplayDenied();

  // Load the preloaded data and we should now be able to autoplay.
  ApplyPreloadedOrigin(PrimaryOrigin());
  LoadTestPage("engagement_autoplay_test.html");
  ExpectAutoplayAllowed();

  // If we now have a high MEI score we should still be allowed to autoplay.
  SetScores(PrimaryOrigin(), 10, 10);
  LoadTestPage("engagement_autoplay_test.html");
  ExpectAutoplayAllowed();

  // If we clear the preloaded data we should still be allowed to autoplay.
  ApplyEmptyPreloadedList();
  LoadTestPage("engagement_autoplay_test.html");
  ExpectAutoplayAllowed();
}

IN_PROC_BROWSER_TEST_F(MediaEngagementAutoplayBrowserTest,
                       UsePreloadedData_Denied) {
  // Autoplay should be blocked by default if we have a bad score.
  SetScores(PrimaryOrigin(), 0, 0);
  LoadTestPage("engagement_autoplay_test.html");
  ExpectAutoplayDenied();

  // Load the preloaded data but we are not in that dataset so we should not be
  // allowed to autoplay.
  ApplyPreloadedOrigin(SecondaryOrigin());
  LoadTestPage("engagement_autoplay_test.html");
  ExpectAutoplayDenied();

  // If we now have a high MEI score we should now be allowed to autoplay.
  SetScores(PrimaryOrigin(), 10, 10);
  LoadTestPage("engagement_autoplay_test.html");
  ExpectAutoplayAllowed();

  // If we clear the preloaded data we should still be allowed to autoplay.
  ApplyEmptyPreloadedList();
  LoadTestPage("engagement_autoplay_test.html");
  ExpectAutoplayAllowed();
}

IN_PROC_BROWSER_TEST_F(MediaEngagementAutoplayBrowserTest,
                       PreloadedDataAndHighVisits) {
  // Autoplay should be denied due to a low score.
  SetScores(PrimaryOrigin(), 10, 0);
  LoadTestPage("engagement_autoplay_test.html");
  ExpectAutoplayDenied();

  // Load the preloaded data and even though we are in the dataset we should not
  // be allowed to play.
  ApplyPreloadedOrigin(PrimaryOrigin());
  LoadTestPage("engagement_autoplay_test.html");
  ExpectAutoplayDenied();
}
