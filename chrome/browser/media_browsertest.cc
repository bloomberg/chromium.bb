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

    ui_test_utils::NavigateToURL(browser(), GURL(url));

    // Allow the media file to be loaded.
    ui_test_utils::WaitForNotification(
        NotificationType::TAB_CONTENTS_TITLE_UPDATED);

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

    ui_test_utils::NavigateToURL(browser(), GURL(url));
    ui_test_utils::WaitForNotification(
        NotificationType::TAB_CONTENTS_TITLE_UPDATED);
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

IN_PROC_BROWSER_TEST_F(MediaBrowserTest, VideoBearTheora) {
  PlayVideo("bear.ogv");
}

IN_PROC_BROWSER_TEST_F(MediaBrowserTest, VideoBearSilentTheora) {
  PlayVideo("bear_silent.ogv");
}

IN_PROC_BROWSER_TEST_F(MediaBrowserTest, VideoBearWebm) {
  PlayVideo("bear.webm");
}

IN_PROC_BROWSER_TEST_F(MediaBrowserTest, VideoBearSilentWebm) {
  PlayVideo("bear_silent.webm");
}

#if defined(GOOGLE_CHROME_BUILD) || defined(USE_PROPRIETARY_CODECS)
TEST_F(MediaBrowserTest, VideoBearMp4) {
  PlayVideo("bear.mp4");
}

TEST_F(MediaBrowserTest, VideoBearSilentMp4) {
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

TEST_F(UILayoutTest, MediaUILayoutTest) {
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
