// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/media/media_player.h"

#include <string>

#include "base/bind.h"
#include "chrome/browser/chromeos/extensions/file_manager_util.h"
#include "chrome/browser/chromeos/extensions/media_player_event_router.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/user_metrics.h"
#include "net/url_request/url_request_job.h"
#include "ui/gfx/screen.h"

using content::BrowserThread;
using content::UserMetricsAction;

static const char* kMediaPlayerAppName = "mediaplayer";
static const int kPopupRight = 20;
static const int kPopupBottom = 50;
static const int kPopupWidth = 280;

// Set the initial height to the minimum possible height. Keep the constants
// in sync with chrome/browser/resources/file_manager/css/audio_player.css.
// SetWindowHeight will be called soon after the popup creation with the correct
// height which will cause a nice slide-up animation.
// TODO(kaznacheev): Remove kTitleHeight when MediaPlayer becomes chromeless.
static const int kTitleHeight = 24;
static const int kTrackHeight = 58;
static const int kControlsHeight = 35;
static const int kPopupHeight = kTitleHeight + kTrackHeight + kControlsHeight;

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

void MediaPlayer::SetWindowHeight(int content_height) {
  if (mediaplayer_browser_ != NULL) {
    int window_height = content_height + kTitleHeight;
    gfx::Rect bounds = mediaplayer_browser_->window()->GetBounds();
    mediaplayer_browser_->window()->SetBounds(gfx::Rect(
        bounds.x(),
        std::max(0, bounds.bottom() - window_height),
        bounds.width(),
        window_height));
  }
}

void MediaPlayer::CloseWindow() {
  if (mediaplayer_browser_ != NULL) {
    mediaplayer_browser_->window()->Close();
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

void MediaPlayer::Observe(int type,
                          const content::NotificationSource& source,
                          const content::NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_BROWSER_CLOSED);
  registrar_.Remove(this, chrome::NOTIFICATION_BROWSER_CLOSED, source);

  if (content::Source<Browser>(source).ptr() == mediaplayer_browser_)
    mediaplayer_browser_ = NULL;
}

void MediaPlayer::NotifyPlaylistChanged() {
  ExtensionMediaPlayerEventRouter::GetInstance()->NotifyPlaylistChanged();
}

void MediaPlayer::PopupMediaPlayer() {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&MediaPlayer::PopupMediaPlayer,
                   base::Unretained(this) /*this class is a singleton*/));
    return;
  }
  if (mediaplayer_browser_) {  // Already opened.
    mediaplayer_browser_->window()->Show();
    return;
  }

  const gfx::Size screen = gfx::Screen::GetPrimaryDisplay().size();
  const gfx::Rect bounds(screen.width() - kPopupRight - kPopupWidth,
                         screen.height() - kPopupBottom - kPopupHeight,
                         kPopupWidth,
                         kPopupHeight);

  Profile* profile = ProfileManager::GetDefaultProfileOrOffTheRecord();
  mediaplayer_browser_ = Browser::CreateWithParams(
      Browser::CreateParams::CreateForApp(Browser::TYPE_PANEL,
                                          kMediaPlayerAppName,
                                          bounds,
                                          profile));
  registrar_.Add(this,
                 chrome::NOTIFICATION_BROWSER_CLOSED,
                 content::Source<Browser>(mediaplayer_browser_));

  mediaplayer_browser_->AddSelectedTabWithURL(GetMediaPlayerUrl(),
                                              content::PAGE_TRANSITION_LINK);
  mediaplayer_browser_->window()->Show();
}

GURL MediaPlayer::GetMediaPlayerUrl() const {
  return file_manager_util::GetMediaPlayerUrl();
}

MediaPlayer::MediaPlayer()
    : current_position_(0),
      mediaplayer_browser_(NULL) {
};
