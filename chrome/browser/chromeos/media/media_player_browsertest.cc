// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/media/media_player.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"

class MediaPlayerBrowserTest : public InProcessBrowserTest {
 public:
  MediaPlayerBrowserTest() {}

  GURL GetMusicTestURL() {
    return GURL("http://localhost:1337/files/plugin/sample_mp3.mp3");
  }

  bool IsBrowserVisible(Browser* browser) {
    if (browser == NULL)
      return false;
    for (BrowserList::const_iterator it = BrowserList::begin();
         it != BrowserList::end(); ++it) {
      if ((*it)->is_type_panel() && (*it)->is_app() && (*it) == browser)
        return true;
    }
    return false;
  }

  bool IsPlayerVisible() {
    return IsBrowserVisible(MediaPlayer::GetInstance()->GetBrowser());
  }
};

IN_PROC_BROWSER_TEST_F(MediaPlayerBrowserTest, Popup) {
  ASSERT_TRUE(test_server()->Start());
  // Doing this so we have a valid profile.
  ui_test_utils::NavigateToURL(browser(),
                               GURL("chrome://downloads"));

  MediaPlayer* player = MediaPlayer::GetInstance();
  // Check that its not currently visible
  ASSERT_FALSE(IsPlayerVisible());

  player->PopupMediaPlayer();
  player->ForcePlayMediaURL(GetMusicTestURL());

  ASSERT_TRUE(IsPlayerVisible());
}
