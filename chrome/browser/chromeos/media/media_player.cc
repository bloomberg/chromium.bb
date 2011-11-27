// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
#include "content/browser/download/download_manager.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/user_metrics.h"
#include "content/public/browser/browser_thread.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request_job.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_CHROMEOS) && defined(TOOLKIT_USES_GTK)
#include "chrome/browser/chromeos/frame/panel_browser_view.h"
#endif

using content::BrowserThread;

static const char* kMediaPlayerAppName = "mediaplayer";
static const int kPopupLeft = 0;
static const int kPopupTop = 0;
static const int kPopupWidth = 350;
static const int kPopupHeight = 300;

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

// Allows InvokeLater without adding refcounting. This class is a Singleton and
// won't be deleted until it's last InvokeLater is run.
DISABLE_RUNNABLE_METHOD_REFCOUNT(MediaPlayer);

MediaPlayer::~MediaPlayer() {
}

// static
MediaPlayer* MediaPlayer::GetInstance() {
  return Singleton<MediaPlayer>::get();
}

void MediaPlayer::EnqueueMediaFile(Profile* profile,
                                   const FilePath& file_path) {
  GURL url;
  if (!file_manager_util::ConvertFileToFileSystemUrl(profile, file_path,
                                                     GetOriginUrl(), &url)) {
  }
  EnqueueMediaFileUrl(url);
}

void MediaPlayer::EnqueueMediaFileUrl(const GURL& url) {
  current_playlist_.push_back(MediaUrl(url));
  NotifyPlaylistChanged();
}

void MediaPlayer::ForcePlayMediaFile(Profile* profile,
                                     const FilePath& file_path) {
  GURL url;
  if (!file_manager_util::ConvertFileToFileSystemUrl(profile, file_path,
                                                     GetOriginUrl(), &url)) {
    return;
  }
  ForcePlayMediaURL(url);
}

void MediaPlayer::ForcePlayMediaURL(const GURL& url) {
  current_playlist_.clear();
  current_playlist_.push_back(MediaUrl(url));
  current_position_ = current_playlist_.size() - 1;
  pending_playback_request_ = true;
  NotifyPlaylistChanged();
}

void MediaPlayer::TogglePlaylistWindowVisible() {
  if (playlist_browser_) {
    ClosePlaylistWindow();
  } else {
    PopupPlaylist(NULL);
  }
}

void MediaPlayer::ClosePlaylistWindow() {
  if (playlist_browser_ != NULL) {
    playlist_browser_->window()->Close();
  }
}

void MediaPlayer::SetPlaylistPosition(int position) {
  const int playlist_size = current_playlist_.size();
  if (current_position_ < 0 || current_position_ > playlist_size)
    position = current_playlist_.size();
  if (current_position_ != position) {
    current_position_ = position;
    NotifyPlaylistChanged();
  }
}

void MediaPlayer::SetPlaybackError(GURL const& url) {
  for (size_t x = 0; x < current_playlist_.size(); x++) {
    if (current_playlist_[x].url == url) {
      current_playlist_[x].haderror = true;
    }
  }
  NotifyPlaylistChanged();
}

void MediaPlayer::Observe(int type,
                          const content::NotificationSource& source,
                          const content::NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_BROWSER_CLOSED);
  registrar_.Remove(this,
                    chrome::NOTIFICATION_BROWSER_CLOSED,
                    source);
  if (content::Source<Browser>(source).ptr() == mediaplayer_browser_) {
    mediaplayer_browser_ = NULL;
  } else if (content::Source<Browser>(source).ptr() == playlist_browser_) {
    playlist_browser_ = NULL;
  }
}

void MediaPlayer::NotifyPlaylistChanged() {
  ExtensionMediaPlayerEventRouter::GetInstance()->NotifyPlaylistChanged();
}

bool MediaPlayer::GetPendingPlayRequestAndReset() {
  bool result = pending_playback_request_;
  pending_playback_request_ = false;
  return result;
}

void MediaPlayer::SetPlaybackRequest() {
  pending_playback_request_ = true;
}

void MediaPlayer::ToggleFullscreen() {
  if (mediaplayer_browser_) {
    mediaplayer_browser_->ToggleFullscreenMode(false);
  }
}

void MediaPlayer::PopupPlaylist(Browser* creator) {
  if (playlist_browser_)
    return;  // Already opened.

  Profile* profile = BrowserList::GetLastActive()->profile();
  playlist_browser_ = Browser::CreateForApp(Browser::TYPE_PANEL,
                                            kMediaPlayerAppName,
                                            gfx::Rect(),
                                            profile);
  registrar_.Add(this,
                 chrome::NOTIFICATION_BROWSER_CLOSED,
                 content::Source<Browser>(playlist_browser_));
  playlist_browser_->AddSelectedTabWithURL(GetMediaplayerPlaylistUrl(),
                                           content::PAGE_TRANSITION_LINK);
  playlist_browser_->window()->SetBounds(gfx::Rect(kPopupLeft,
                                                   kPopupTop,
                                                   kPopupWidth,
                                                   kPopupHeight));
  playlist_browser_->window()->Show();
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
  if (mediaplayer_browser_)
    return;  // Already opened.

  Profile* profile = BrowserList::GetLastActive()->profile();
  mediaplayer_browser_ = Browser::CreateForApp(Browser::TYPE_PANEL,
                                               kMediaPlayerAppName,
                                               gfx::Rect(),
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
  mediaplayer_browser_->window()->SetBounds(gfx::Rect(kPopupLeft,
                                                      kPopupTop,
                                                      kPopupWidth,
                                                      kPopupHeight));
  mediaplayer_browser_->window()->Show();
}

net::URLRequestJob* MediaPlayer::MaybeIntercept(net::URLRequest* request) {
  // Don't attempt to intercept here as we want to wait until the mime
  // type is fully determined.
  return NULL;
}

// This is the list of mime types currently supported by the Google
// Document Viewer.
static const char* const supported_mime_type_list[] = {
  "audio/mpeg",
  "video/mp4",
  "audio/mp3"
};

net::URLRequestJob* MediaPlayer::MaybeInterceptResponse(
    net::URLRequest* request) {
  // Do not intercept this request if it is a download.
  if (request->load_flags() & net::LOAD_IS_DOWNLOAD) {
    return NULL;
  }

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

GURL MediaPlayer::GetMediaplayerPlaylistUrl() const {
  return file_manager_util::GetMediaPlayerPlaylistUrl();
}

GURL MediaPlayer::GetMediaPlayerUrl() const {
  return file_manager_util::GetMediaPlayerUrl();
}

MediaPlayer::MediaPlayer()
    : current_position_(0),
      pending_playback_request_(false),
      playlist_browser_(NULL),
      mediaplayer_browser_(NULL) {
  for (size_t i = 0; i < arraysize(supported_mime_type_list); ++i) {
    supported_mime_types_.insert(supported_mime_type_list[i]);
  }
};
