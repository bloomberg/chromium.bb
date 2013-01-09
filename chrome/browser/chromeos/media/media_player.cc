// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/media/media_player.h"

#include <string>

#include "ash/shell.h"
#include "base/bind.h"
#include "chrome/browser/chromeos/extensions/file_manager_util.h"
#include "chrome/browser/chromeos/extensions/media_player_api.h"
#include "chrome/browser/chromeos/extensions/media_player_event_router.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/screen.h"

using content::BrowserThread;

namespace {

const char kMediaPlayerAppName[] = "mediaplayer";
const int kPopupRight = 20;
const int kPopupBottom = 80;
const int kPopupWidth = 280;

// Set the initial height to the minimum possible height. Keep the constants
// in sync with chrome/browser/resources/file_manager/css/audio_player.css.
// SetWindowHeight will be called soon after the popup creation with the correct
// height which will cause a nice slide-up animation.
// TODO(kaznacheev): Remove kTitleHeight when MediaPlayer becomes chromeless.
// kTitleHeight is an approximate value. May be different for touch-enabled UI.
const int kTitleHeight = 35;
const int kTrackHeight = 58;
const int kControlsHeight = 35;
const int kPopupHeight = kTitleHeight + kTrackHeight + kControlsHeight;

}  // namespace

const MediaPlayer::UrlVector& MediaPlayer::GetPlaylist() const {
  return current_playlist_;
}

int MediaPlayer::GetPlaylistPosition() const {
  return current_position_;
}

////////////////////////////////////////////////////////////////////////////////
//
// Mediaplayer
//
////////////////////////////////////////////////////////////////////////////////

MediaPlayer::~MediaPlayer() {
}

// static
MediaPlayer* MediaPlayer::GetInstance() {
  return Singleton<MediaPlayer>::get();
}

// The client knows how high the client part of the window should be but
// cannot translate it to the window height (because the window title bar height
// is unknown). Instead it passes the height difference which this method
// applies to the window height.
void MediaPlayer::AdjustWindowHeight(int height_diff) {
  Browser* browser = GetBrowser();
  if (browser != NULL) {
    gfx::Rect bounds = browser->window()->GetBounds();
    int window_height = bounds.height() + height_diff;
    browser->window()->SetBounds(gfx::Rect(
        bounds.x(),
        std::max(0, bounds.bottom() - window_height),
        bounds.width(),
        window_height));
  }
}

void MediaPlayer::CloseWindow() {
  Browser* browser = GetBrowser();
  if (browser != NULL) {
    browser->window()->Close();
  }
}

void MediaPlayer::ClearPlaylist() {
  current_playlist_.clear();
}

void MediaPlayer::EnqueueMediaFileUrl(const GURL& url) {
  current_playlist_.push_back(url);
}

void MediaPlayer::ForcePlayMediaURL(const GURL& url) {
  ClearPlaylist();
  EnqueueMediaFileUrl(url);
  SetPlaylistPosition(0);
  NotifyPlaylistChanged();
}

void MediaPlayer::SetPlaylistPosition(int position) {
  current_position_ = position;
}

void MediaPlayer::NotifyPlaylistChanged() {
  Browser* browser = GetBrowser();
  if (browser) {
    extensions::MediaPlayerAPI::Get(browser->profile())->
        media_player_event_router()->NotifyPlaylistChanged();
  }
}

void MediaPlayer::PopupMediaPlayer() {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&MediaPlayer::PopupMediaPlayer,
                   base::Unretained(this) /*this class is a singleton*/));
    return;
  }

  Browser* browser = GetBrowser();
  if (!browser) {
    const gfx::Size screen =
        ash::Shell::GetScreen()->GetPrimaryDisplay().size();
    const gfx::Rect bounds(screen.width() - kPopupRight - kPopupWidth,
                           screen.height() - kPopupBottom - kPopupHeight,
                           kPopupWidth,
                           kPopupHeight);

    Profile* profile = ProfileManager::GetDefaultProfileOrOffTheRecord();
    Browser::CreateParams params(Browser::TYPE_POPUP, profile);
    params.app_name = kMediaPlayerAppName;
    params.initial_bounds = bounds;
    browser = new Browser(params);

    chrome::AddSelectedTabWithURL(browser, GetMediaPlayerUrl(),
                                  content::PAGE_TRANSITION_LINK);
  }
  browser->window()->Show();
}

GURL MediaPlayer::GetMediaPlayerUrl() {
  return file_manager_util::GetMediaPlayerUrl();
}

Browser* MediaPlayer::GetBrowser() {
  for (BrowserList::const_iterator browser_iterator = BrowserList::begin();
       browser_iterator != BrowserList::end(); ++browser_iterator) {
    Browser* browser = *browser_iterator;
    TabStripModel* tab_strip = browser->tab_strip_model();
    for (int idx = 0; idx < tab_strip->count(); idx++) {
      const GURL& url = tab_strip->GetWebContentsAt(idx)->GetURL();
      GURL base_url(url.GetOrigin().spec() + url.path().substr(1));
      if (base_url == GetMediaPlayerUrl())
        return browser;
    }
  }
  return NULL;
}

MediaPlayer::MediaPlayer()
    : current_position_(0) {
};
