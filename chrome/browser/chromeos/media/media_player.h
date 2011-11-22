// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_MEDIA_MEDIA_PLAYER_H_
#define CHROME_BROWSER_CHROMEOS_MEDIA_MEDIA_PLAYER_H_
#pragma once

#include <set>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/singleton.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "net/url_request/url_request.h"

template <typename T> struct DefaultSingletonTraits;

class Browser;
class GURL;
class Profile;

class MediaPlayer : public content::NotificationObserver,
                    public net::URLRequest::Interceptor {
 public:
  struct MediaUrl;
  typedef std::vector<MediaUrl> UrlVector;

  virtual ~MediaPlayer();

  // Enqueues this file into the current playlist.
  void EnqueueMediaFile(Profile* profile, const FilePath& file_path);

  // Enqueues this fileschema url into the current playlist.
  void EnqueueMediaFileUrl(const GURL& url);

  // Clears out the current playlist, and start playback of the given
  // |file_path|.
  void ForcePlayMediaFile(Profile* profile, const FilePath& file_path);

  // Clears out the current playlist, and start playback of the given url.
  void ForcePlayMediaURL(const GURL& url);

  // Toggle the visibility of the playlist window.
  void TogglePlaylistWindowVisible();

  // Popup the mediaplayer, this shows the browser, and sets up its
  // locations correctly.
  void PopupMediaPlayer(Browser* creator);

  // Popup the playlist.  Shows the browser, sets it up to point at
  // chrome://mediaplayer#playlist
  void PopupPlaylist(Browser* creator);

  // Toggle the mediaplayer between fullscreen and windowed.
  void ToggleFullscreen();

  // Force the playlist window to be closed.
  void ClosePlaylistWindow();

  // Sets the currently playing element to the given positions.
  void SetPlaylistPosition(int position);

  // Returns current playlist.
  const UrlVector& GetPlaylist() const;

  // Returns current playlist position.
  int GetPlaylistPosition() const;

  // Set flag that error occuires while playing the url.
  void SetPlaybackError(GURL const& url);

  // Notfies the mediaplayer that the playlist changed. This could be
  // called from the mediaplayer itself for example.
  void NotifyPlaylistChanged();

  // Retuen true if playback requested. Resets this flag.
  bool GetPendingPlayRequestAndReset();

  // Requests starting playback of the current playlist item when the
  // mediaplayer get the playlist updated.
  void SetPlaybackRequest();

  // Always returns NULL because we don't want to attempt a redirect
  // before seeing the detected mime type of the request.
  // Implementation of net::URLRequest::Interceptor.
  virtual net::URLRequestJob* MaybeIntercept(
      net::URLRequest* request) OVERRIDE;

  // Determines if the requested document can be viewed by the
  // MediaPlayer.  If it can, returns a net::URLRequestJob that
  // redirects the browser to the view URL.
  // Implementation of net::URLRequest::Interceptor.
  virtual net::URLRequestJob* MaybeInterceptResponse(
      net::URLRequest* request) OVERRIDE;

  // Used to detect when the mediaplayer is closed.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Getter for the singleton.
  static MediaPlayer* GetInstance();

 private:
  friend struct DefaultSingletonTraits<MediaPlayer>;

  // The current playlist of urls.
  UrlVector current_playlist_;
  // The position into the current_playlist_ of the currently playing item.
  int current_position_;

  bool pending_playback_request_;

  MediaPlayer();

  GURL GetOriginUrl() const;
  GURL GetMediaplayerPlaylistUrl() const;
  GURL GetMediaPlayerUrl() const;

  // Browser containing the playlist. Used to force closes. This is created
  // By the PopupPlaylist call, and is NULLed out when the window is closed.
  Browser* playlist_browser_;

  // Browser containing the Mediaplayer.  Used to force closes. This is
  // created by the PopupMediaplayer call, and is NULLed out when the window
  // is closed.
  Browser* mediaplayer_browser_;

  // Used to register for events on the windows, like to listen for closes.
  content::NotificationRegistrar registrar_;

  // List of mimetypes that the mediaplayer should listen to.  Used for
  // interceptions of url GETs.
  std::set<std::string> supported_mime_types_;
  friend class MediaPlayerBrowserTest;
  DISALLOW_COPY_AND_ASSIGN(MediaPlayer);
};

struct MediaPlayer::MediaUrl {
  MediaUrl() {}
  explicit MediaUrl(const GURL& newurl)
      : url(newurl),
        haderror(false) {}
  GURL url;
  bool haderror;
};

#endif  // CHROME_BROWSER_CHROMEOS_MEDIA_MEDIA_PLAYER_H_
