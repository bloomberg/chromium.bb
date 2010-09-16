// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/path_service.h"
#include "base/utf_string_conversions.h"
#include "base/string_util.h"
#include "chrome/browser/view_ids.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "chrome/test/ui/ui_layout_test.h"
#include "net/base/net_util.h"

namespace {

const wchar_t kPlaying[] = L"PLAYING";
const wchar_t kReady[] = L"READY";
const wchar_t kFullScreen[] = L"FULLSCREEN";

}  // namespace

class MediaBrowserTest : public InProcessBrowserTest {
 protected:
  MediaBrowserTest() {
    set_show_window(true);
  }

  virtual void SetUpCommandLine(CommandLine* command_line) {
    command_line->AppendSwitch(switches::kEnableVideoFullscreen);
  }

  void PlayMedia(const char* tag, const char* media_file) {
    FilePath test_file;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_file));
    test_file = test_file.AppendASCII("media/player.html");

    GURL player_gurl = net::FilePathToFileURL(test_file);
    std::string url = StringPrintf("%s?%s=%s",
                                   player_gurl.spec().c_str(),
                                   tag,
                                   media_file);

    TabContents* tab_contents = browser()->GetSelectedTabContents();

    ui_test_utils::WindowedNotificationObserver<TabContents>
        observer(NotificationType::TAB_CONTENTS_TITLE_UPDATED, tab_contents);
    ui_test_utils::NavigateToURL(browser(), GURL(url));
    observer.Wait();

    string16 title;
    ui_test_utils::GetCurrentTabTitle(browser(), &title);
    EXPECT_EQ(WideToUTF16(kPlaying), title);
  }

  void PlayAudio(const char* url) {
    PlayMedia("audio", url);
  }

  void PlayVideo(const char* url) {
    PlayMedia("video", url);
  }

  void PlayVideoFullScreen(const char* media_file) {
    FilePath test_file;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_file));
    test_file = test_file.AppendASCII("media/player_fullscreen.html");

    GURL player_gurl = net::FilePathToFileURL(test_file);
    std::string url = StringPrintf("%s?%s",
                                   player_gurl.spec().c_str(),
                                   media_file);

    TabContents* tab_contents = browser()->GetSelectedTabContents();

    ui_test_utils::WindowedNotificationObserver<TabContents>
        observer(NotificationType::TAB_CONTENTS_TITLE_UPDATED, tab_contents);
    ui_test_utils::NavigateToURL(browser(), GURL(url));
    observer.Wait();

    string16 title;
    ui_test_utils::GetCurrentTabTitle(browser(), &title);
    EXPECT_EQ(WideToUTF16(kReady), title);

    ui_test_utils::ClickOnView(browser(), VIEW_ID_TAB_CONTAINER);
    ui_test_utils::WaitForNotification(
        NotificationType::TAB_CONTENTS_TITLE_UPDATED);
    ui_test_utils::GetCurrentTabTitle(browser(), &title);
    EXPECT_EQ(WideToUTF16(kFullScreen), title);
  }
};

#if defined(OS_WIN)

// Tests may fail on windows:  http://crbug.com/55477
#define MAYBE_VideoBearTheora FLAKY_VideoBearTheora
#define MAYBE_VideoBearSilentTheora FLAKY_VideoBearSilentTheora
#define MAYBE_VideoBearWebm FLAKY_VideoBearWebm
#define MAYBE_VideoBearSilentWebm FLAKY_VideoBearSilentWebm
#define MAYBE_VideoBearMp4 FLAKY_VideoBearMp4
#define MAYBE_VideoBearSilentMp4 FLAKY_VideoBearSilentMp4
#define MAYBE_MediaUILayoutTest FLAKY_MediaUILayoutTest

#else

#define MAYBE_VideoBearTheora VideoBearTheora
#define MAYBE_VideoBearSilentTheora VideoBearSilentTheora
#define MAYBE_VideoBearWebm VideoBearWebm
#define MAYBE_VideoBearSilentWebm VideoBearSilentWebm
#define MAYBE_VideoBearMp4 VideoBearMp4
#define MAYBE_VideoBearSilentMp4 VideoBearSilentMp4
#define MAYBE_MediaUILayoutTest MediaUILayoutTest

#endif

IN_PROC_BROWSER_TEST_F(MediaBrowserTest, MAYBE_VideoBearTheora) {
  PlayVideo("bear.ogv");
}

IN_PROC_BROWSER_TEST_F(MediaBrowserTest, MAYBE_VideoBearSilentTheora) {
  PlayVideo("bear_silent.ogv");
}

IN_PROC_BROWSER_TEST_F(MediaBrowserTest, MAYBE_VideoBearWebm) {
  PlayVideo("bear.webm");
}

IN_PROC_BROWSER_TEST_F(MediaBrowserTest, MAYBE_VideoBearSilentWebm) {
  PlayVideo("bear_silent.webm");
}

#if defined(GOOGLE_CHROME_BUILD) || defined(USE_PROPRIETARY_CODECS)
IN_PROC_BROWSER_TEST_F(MediaBrowserTest, MAYBE_VideoBearMp4) {
  PlayVideo("bear.mp4");
}

IN_PROC_BROWSER_TEST_F(MediaBrowserTest, MAYBE_VideoBearSilentMp4) {
  PlayVideo("bear_silent.mp4");
}
#endif

// TODO(imcheng): Disabled until fullscreen Webkit patch is here - See
// http://crbug.com/54838.
// (Does Mac have fullscreen?)
#if defined(OS_WIN) || defined(OS_LINUX)
IN_PROC_BROWSER_TEST_F(MediaBrowserTest, DISABLED_VideoBearFullScreen) {
  PlayVideoFullScreen("bear.ogv");
}
#endif  // defined(OS_WIN) || defined(OS_LINUX)

TEST_F(UILayoutTest, MAYBE_MediaUILayoutTest) {
  static const char* kResources[] = {
    "content",
    "media-file.js",
    "media-fullscreen.js",
    "video-paint-test.js",
    "video-played.js",
    "video-test.js",
  };

  static const char* kMediaTests[] = {
    "video-autoplay.html",
    // "video-loop.html", disabled due to 52887.
    "video-no-autoplay.html",
    // TODO(sergeyu): Add more tests here.
  };

  FilePath test_dir;
  FilePath media_test_dir;
  media_test_dir = media_test_dir.AppendASCII("media");
  InitializeForLayoutTest(test_dir, media_test_dir, kNoHttpPort);

  // Copy resources first.
  for (size_t i = 0; i < arraysize(kResources); ++i)
    AddResourceForLayoutTest(
        test_dir, media_test_dir.AppendASCII(kResources[i]));

  for (size_t i = 0; i < arraysize(kMediaTests); ++i)
    RunLayoutTest(kMediaTests[i], kNoHttpPort);
}
