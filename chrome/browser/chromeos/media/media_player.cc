// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/media/media_player.h"

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/threading/thread.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/chromeos/extensions/file_manager_util.h"
#include "chrome/browser/chromeos/extensions/media_player_event_router.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/time_format.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/user_metrics.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request_job.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/screen.h"

#if defined(OS_CHROMEOS) && defined(TOOLKIT_USES_GTK)
#include "chrome/browser/chromeos/frame/panel_browser_view.h"
#endif

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

void MediaPlayer::ForcePlayMediaFile(Profile* profile,
                                     const FilePath& file_path) {
  GURL url;
  if (!file_manager_util::ConvertFileToFileSystemUrl(profile, file_path,
                                                     GetOriginUrl(), &url))
    return;
  ForcePlayMediaURL(url);
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

void MediaPlayer::PopupMediaPlayer(Browser* creator) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&MediaPlayer::PopupMediaPlayer,
                   base::Unretained(this),  // this class is a singleton.
                   static_cast<Browser*>(NULL)));
    return;
  }
  if (mediaplayer_browser_) {  // Already opened.
    mediaplayer_browser_->window()->Show();
    return;
  }

  const gfx::Size screen = gfx::Screen::GetPrimaryMonitorSize();
  const gfx::Rect bounds(screen.width() - kPopupRight - kPopupWidth,
                         screen.height() - kPopupBottom - kPopupHeight,
                         kPopupWidth,
                         kPopupHeight);

  Profile* profile = BrowserList::GetLastActive()->profile();
  mediaplayer_browser_ = Browser::CreateForApp(Browser::TYPE_PANEL,
                                               kMediaPlayerAppName,
                                               bounds,
                                               profile);
  registrar_.Add(this,
                 chrome::NOTIFICATION_BROWSER_CLOSED,
                 content::Source<Browser>(mediaplayer_browser_));

#if defined(OS_CHROMEOS) && defined(TOOLKIT_USES_GTK)
  // Since we are on chromeos, popups should be a PanelBrowserView,
  // so we can just cast it.
  if (creator) {
    chromeos::PanelBrowserView* creatorview =
        static_cast<chromeos::PanelBrowserView*>(creator->window());
    chromeos::PanelBrowserView* view =
        static_cast<chromeos::PanelBrowserView*>(
            mediaplayer_browser_->window());
    view->SetCreatorView(creatorview);
  }
#endif
  mediaplayer_browser_->AddSelectedTabWithURL(GetMediaPlayerUrl(),
                                              content::PAGE_TRANSITION_LINK);
  mediaplayer_browser_->window()->Show();
}

net::URLRequestJob* MediaPlayer::MaybeIntercept(net::URLRequest* request) {
  // Don't attempt to intercept here as we want to wait until the mime
  // type is fully determined.
  return NULL;
}

// This is the list of mime types currently supported by the Media Player.
static const char* const supported_mime_type_list[] = {
  "audio/mpeg",
  "audio/mp3"
};

net::URLRequestJob* MediaPlayer::MaybeInterceptResponse(
    net::URLRequest* request) {
  // Do not intercept this request if it is a download.
  if (request->load_flags() & net::LOAD_IS_DOWNLOAD)
    return NULL;

  std::string mime_type;
  request->GetMimeType(&mime_type);
  // If it is in our list of known URLs, enqueue the url then
  // Cancel the request so the mediaplayer can handle it when
  // it hits it in the playlist.
  if (supported_mime_types_.find(mime_type) != supported_mime_types_.end()) {
    if (request->referrer() != chrome::kChromeUIMediaplayerURL &&
        !request->referrer().empty()) {
      PopupMediaPlayer(NULL);
      ForcePlayMediaURL(request->url());
      request->Cancel();
    }
  }
  return NULL;
}

GURL MediaPlayer::GetOriginUrl() const {
  return file_manager_util::GetMediaPlayerUrl().GetOrigin();
}

GURL MediaPlayer::GetMediaPlayerUrl() const {
  return file_manager_util::GetMediaPlayerUrl();
}

MediaPlayer::MediaPlayer()
    : current_position_(0),
      mediaplayer_browser_(NULL) {
  for (size_t i = 0; i < arraysize(supported_mime_type_list); ++i) {
    supported_mime_types_.insert(supported_mime_type_list[i]);
  }
};
