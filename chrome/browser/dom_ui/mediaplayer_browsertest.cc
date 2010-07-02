// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/ref_counted.h"
#include "base/utf_string_conversions.h"
#include "chrome/test/automation/dom_element_proxy.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/dom_ui/mediaplayer_ui.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"

namespace {

class MediaPlayerBrowserTest : public InProcessBrowserTest {
 public:
  MediaPlayerBrowserTest() {}

  GURL GetMusicTestURL() {
    return GURL("http://localhost:1337/files/plugin/sample_mp3.mp3");
  }

  bool IsPlayerVisible() {
    for (BrowserList::const_iterator it = BrowserList::begin();
         it != BrowserList::end(); ++it) {
      if ((*it)->type() == Browser::TYPE_POPUP) {
        const GURL& url =
            (*it)->GetTabContentsAt((*it)->selected_index())->GetURL();

        if (url.SchemeIs(chrome::kChromeUIScheme) &&
            url.host() == chrome::kChromeUIMediaplayerHost) {
          return true;
        }
      }
    }
    return false;
  }

  bool IsPlaylistVisible() {
    for (BrowserList::const_iterator it = BrowserList::begin();
         it != BrowserList::end(); ++it) {
      if ((*it)->type() == Browser::TYPE_POPUP) {
        const GURL& url =
            (*it)->GetTabContentsAt((*it)->selected_index())->GetURL();

        if (url.SchemeIs(chrome::kChromeUIScheme) &&
            url.host() == chrome::kChromeUIMediaplayerHost &&
            url.ref() == "playlist") {
          return true;
        }
      }
    }
    return false;
  }
};

IN_PROC_BROWSER_TEST_F(MediaPlayerBrowserTest, Popup) {
  ASSERT_TRUE(StartHTTPServer());
  // Doing this so we have a valid profile.
  ui_test_utils::NavigateToURL(browser(),
                               GURL("chrome://downloads"));

  PrefService* pref_service = browser()->profile()->GetPrefs();
  pref_service->SetBoolean(prefs::kLabsMediaplayerEnabled, true);

  MediaPlayer* player = MediaPlayer::Get();
  // Check that its not currently visible
  ASSERT_FALSE(IsPlayerVisible());

  player->EnqueueMediaURL(GetMusicTestURL(), NULL);

  ASSERT_TRUE(IsPlayerVisible());
}

IN_PROC_BROWSER_TEST_F(MediaPlayerBrowserTest, PopupPlaylist) {
  ASSERT_TRUE(StartHTTPServer());
  // Doing this so we have a valid profile.
  ui_test_utils::NavigateToURL(browser(),
                               GURL("chrome://downloads"));


  PrefService* pref_service = browser()->profile()->GetPrefs();
  pref_service->SetBoolean(prefs::kLabsMediaplayerEnabled, true);

  MediaPlayer* player = MediaPlayer::Get();

  player->EnqueueMediaURL(GetMusicTestURL(), NULL);

  EXPECT_FALSE(IsPlaylistVisible());

  player->TogglePlaylistWindowVisible();

  EXPECT_TRUE(IsPlaylistVisible());
}

}  // namespace
