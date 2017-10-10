// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/media/media_engagement_contents_observer.h"
#include "chrome/browser/media/media_engagement_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/media_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace {

const char* kMediaEngagementTestDataPath = "chrome/test/data/media/engagement";

const base::string16 kReadyTitle = base::ASCIIToUTF16("Ready");

// Watches WasRecentlyAudible changes on a WebContents, blocking until the
// tab is audible. The audio stream monitor runs at 15Hz so we need have
// a slight delay to ensure it has run.
class WasRecentlyAudibleWatcher {
 public:
  // |web_contents| must be non-NULL and needs to stay alive for the
  // entire lifetime of |this|.
  explicit WasRecentlyAudibleWatcher(content::WebContents* web_contents)
      : web_contents_(web_contents), timer_(new base::Timer(true, true)){};
  ~WasRecentlyAudibleWatcher() = default;

  // Waits until WasRecentlyAudible is true.
  void WaitForWasRecentlyAudible() {
    if (!web_contents_->WasRecentlyAudible()) {
      timer_->Start(
          FROM_HERE, base::TimeDelta::FromMicroseconds(100),
          base::Bind(&WasRecentlyAudibleWatcher::TestWasRecentlyAudible,
                     base::Unretained(this)));
      run_loop_.reset(new base::RunLoop());
      run_loop_->Run();
    }
  };

 private:
  void TestWasRecentlyAudible() {
    if (web_contents_->WasRecentlyAudible()) {
      run_loop_->Quit();
      timer_->Stop();
    }
  };

  content::WebContents* web_contents_;

  std::unique_ptr<base::Timer> timer_;
  std::unique_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(WasRecentlyAudibleWatcher);
};

}  // namespace

// Class used to test the Media Engagement service.
class MediaEngagementBrowserTest : public InProcessBrowserTest {
 public:
  MediaEngagementBrowserTest()
      : task_runner_(new base::TestMockTimeTaskRunner()) {
    http_server_.ServeFilesFromSourceDirectory(kMediaEngagementTestDataPath);
    http_server_origin2_.ServeFilesFromSourceDirectory(
        kMediaEngagementTestDataPath);
  }

  ~MediaEngagementBrowserTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(http_server_.Start());
    ASSERT_TRUE(http_server_origin2_.Start());

    scoped_feature_list_.InitAndEnableFeature(
        media::kRecordMediaEngagementScores);

    InProcessBrowserTest::SetUp();
  };

  void LoadTestPage(const std::string& page) {
    // We can't do this in SetUp as the browser isn't ready yet and we
    // need it before the page navigates.
    InjectTimerTaskRunner();

    ui_test_utils::NavigateToURL(browser(), http_server_.GetURL("/" + page));
  };

  void LoadTestPageAndWaitForPlay(const std::string& page) {
    LoadTestPage(page);
    WaitForPlay();
  }

  void LoadTestPageAndWaitForPlayAndAudible(const std::string& page) {
    LoadTestPageAndWaitForPlay(page);
    WaitForWasRecentlyAudible();
  };

  void Advance(base::TimeDelta time) {
    task_runner_->FastForwardBy(time);
    base::RunLoop().RunUntilIdle();
  }

  void AdvanceMeaningfulPlaybackTime() {
    Advance(MediaEngagementBrowserTest::kMaxWaitingTime);
  }

  void ExpectScores(int visits, int media_playbacks) {
    ExpectScores(http_server_.GetURL("/"), visits, media_playbacks);
  }

  void ExpectScoresSecondOrigin(int visits, int media_playbacks) {
    ExpectScores(http_server_origin2_.GetURL("/"), visits, media_playbacks);
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void ExecuteScript(const std::string& script) {
    EXPECT_TRUE(content::ExecuteScript(GetWebContents(), script));
  }

  void OpenTab() {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL("chrome://about"),
        WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  }

  void LoadSubFrame(const std::string& page) {
    ExecuteScript("window.open(\"" +
                  http_server_origin2_.GetURL("/" + page).spec() +
                  "\", \"subframe\")");
  }

  void WaitForPlay() {
    content::TitleWatcher title_watcher(GetWebContents(), kReadyTitle);
    EXPECT_EQ(kReadyTitle, title_watcher.WaitAndGetTitle());
  }

  void WaitForWasRecentlyAudible() {
    WasRecentlyAudibleWatcher watcher(GetWebContents());
    watcher.WaitForWasRecentlyAudible();
  }

 private:
  void ExpectScores(GURL url, int visits, int media_playbacks) {
    MediaEngagementScore score = GetService()->CreateEngagementScore(url);
    EXPECT_EQ(visits, score.visits());
    EXPECT_EQ(media_playbacks, score.media_playbacks());
  }

  void InjectTimerTaskRunner() {
    contents_observer()->playback_timer_->SetTaskRunner(task_runner_);
  }

  MediaEngagementService* GetService() {
    return MediaEngagementService::Get(browser()->profile());
  }

  MediaEngagementContentsObserver* contents_observer() {
    MediaEngagementService* mei_service = GetService();
    DCHECK(mei_service->contents_observers_.size() == 1);
    return *mei_service->contents_observers_.begin();
  }

  net::EmbeddedTestServer http_server_;
  net::EmbeddedTestServer http_server_origin2_;

  base::test::ScopedFeatureList scoped_feature_list_;

  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;

  const base::TimeDelta kMaxWaitingTime =
      MediaEngagementContentsObserver::kSignificantMediaPlaybackTime +
      base::TimeDelta::FromSeconds(2);
};

IN_PROC_BROWSER_TEST_F(MediaEngagementBrowserTest, RecordEngagement) {
  LoadTestPageAndWaitForPlayAndAudible("engagement_test.html");
  AdvanceMeaningfulPlaybackTime();
  ExpectScores(1, 1);
}

IN_PROC_BROWSER_TEST_F(MediaEngagementBrowserTest, RecordEngagement_AudioOnly) {
  LoadTestPageAndWaitForPlayAndAudible("engagement_test_audio.html");
  AdvanceMeaningfulPlaybackTime();
  ExpectScores(1, 1);
}

IN_PROC_BROWSER_TEST_F(MediaEngagementBrowserTest,
                       DoNotRecordEngagement_NotTime) {
  LoadTestPageAndWaitForPlayAndAudible("engagement_test.html");
  Advance(base::TimeDelta::FromSeconds(1));
  ExpectScores(1, 0);
}

IN_PROC_BROWSER_TEST_F(MediaEngagementBrowserTest,
                       DoNotRecordEngagement_NotTime_AudioOnly) {
  LoadTestPageAndWaitForPlayAndAudible("engagement_test_audio.html");
  Advance(base::TimeDelta::FromSeconds(1));
  ExpectScores(1, 0);
}

IN_PROC_BROWSER_TEST_F(MediaEngagementBrowserTest,
                       DoNotRecordEngagement_TabMuted) {
  GetWebContents()->SetAudioMuted(true);
  LoadTestPageAndWaitForPlayAndAudible("engagement_test.html");
  AdvanceMeaningfulPlaybackTime();
  ExpectScores(1, 0);
}

IN_PROC_BROWSER_TEST_F(MediaEngagementBrowserTest,
                       DoNotRecordEngagement_TabMuted_AudioOnly) {
  GetWebContents()->SetAudioMuted(true);
  LoadTestPageAndWaitForPlayAndAudible("engagement_test_audio.html");
  AdvanceMeaningfulPlaybackTime();
  ExpectScores(1, 0);
}

IN_PROC_BROWSER_TEST_F(MediaEngagementBrowserTest,
                       DoNotRecordEngagement_PlayerMuted) {
  LoadTestPageAndWaitForPlay("engagement_test_muted.html");
  AdvanceMeaningfulPlaybackTime();
  ExpectScores(1, 0);
}

IN_PROC_BROWSER_TEST_F(MediaEngagementBrowserTest,
                       DoNotRecordEngagement_PlayerMuted_AudioOnly) {
  LoadTestPageAndWaitForPlay("engagement_test_audio_muted.html");
  AdvanceMeaningfulPlaybackTime();
  ExpectScores(1, 0);
}

IN_PROC_BROWSER_TEST_F(MediaEngagementBrowserTest,
                       DoNotRecordEngagement_PlaybackStopped) {
  LoadTestPageAndWaitForPlayAndAudible("engagement_test.html");
  Advance(base::TimeDelta::FromSeconds(1));
  ExecuteScript("document.getElementById(\"media\").pause();");
  AdvanceMeaningfulPlaybackTime();
  ExpectScores(1, 0);
}

IN_PROC_BROWSER_TEST_F(MediaEngagementBrowserTest,
                       DoNotRecordEngagement_PlaybackStopped_AudioOnly) {
  LoadTestPageAndWaitForPlayAndAudible("engagement_test_audio.html");
  Advance(base::TimeDelta::FromSeconds(1));
  ExecuteScript("document.getElementById(\"media\").pause();");
  AdvanceMeaningfulPlaybackTime();
  ExpectScores(1, 0);
}

IN_PROC_BROWSER_TEST_F(MediaEngagementBrowserTest,
                       DoNotRecordEngagement_NotVisible) {
  LoadTestPageAndWaitForPlayAndAudible("engagement_test.html");
  OpenTab();
  AdvanceMeaningfulPlaybackTime();
  ExpectScores(1, 0);
}

IN_PROC_BROWSER_TEST_F(MediaEngagementBrowserTest,
                       DoNotRecordEngagement_NotVisible_AudioOnly) {
  LoadTestPageAndWaitForPlayAndAudible("engagement_test_audio.html");
  OpenTab();
  AdvanceMeaningfulPlaybackTime();
  ExpectScores(1, 0);
}

IN_PROC_BROWSER_TEST_F(MediaEngagementBrowserTest,
                       DoNotRecordEngagement_FrameSize) {
  LoadTestPageAndWaitForPlayAndAudible("engagement_test_small_frame_size.html");
  AdvanceMeaningfulPlaybackTime();
  ExpectScores(1, 0);
}

IN_PROC_BROWSER_TEST_F(MediaEngagementBrowserTest,
                       DoNotRecordEngagement_NoAudioTrack) {
  LoadTestPageAndWaitForPlay("engagement_test_no_audio_track.html");
  AdvanceMeaningfulPlaybackTime();
  ExpectScores(1, 0);
}

IN_PROC_BROWSER_TEST_F(MediaEngagementBrowserTest,
                       DoNotRecordEngagement_SilentAudioTrack) {
  LoadTestPageAndWaitForPlay("engagement_test_silent_audio_track.html");
  AdvanceMeaningfulPlaybackTime();
  ExpectScores(1, 0);
}

IN_PROC_BROWSER_TEST_F(MediaEngagementBrowserTest,
                       DoNotRecordEngagement_SilentAudioTrack_AudioOnly) {
  LoadTestPageAndWaitForPlay("engagement_test_silent_audio_track_audio.html");
  AdvanceMeaningfulPlaybackTime();
  ExpectScores(1, 0);
}

IN_PROC_BROWSER_TEST_F(MediaEngagementBrowserTest, IFrameDelegation) {
  LoadTestPage("engagement_test_iframe.html");
  LoadSubFrame("engagement_test_iframe_child.html");

  WaitForPlay();
  WaitForWasRecentlyAudible();
  AdvanceMeaningfulPlaybackTime();

  ExpectScores(1, 1);
  ExpectScoresSecondOrigin(0, 0);
}

IN_PROC_BROWSER_TEST_F(MediaEngagementBrowserTest, IFrameDelegation_AudioOnly) {
  LoadTestPage("engagement_test_iframe.html");
  LoadSubFrame("engagement_test_iframe_audio_child.html");

  WaitForPlay();
  WaitForWasRecentlyAudible();
  AdvanceMeaningfulPlaybackTime();

  ExpectScores(1, 1);
  ExpectScoresSecondOrigin(0, 0);
}
