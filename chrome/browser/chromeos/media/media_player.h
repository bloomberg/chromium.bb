// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_MEDIA_MEDIA_PLAYER_H_
#define CHROME_BROWSER_CHROMEOS_MEDIA_MEDIA_PLAYER_H_
#pragma once

#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/singleton.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"

template <typename T> struct DefaultSingletonTraits;

class Browser;
class FilePath;
class GURL;
class Profile;

class MediaPlayer : public content::NotificationObserver {
 public:
  typedef std::vector<GURL> UrlVector;

  virtual ~MediaPlayer();

  // Sets the mediaplayer window height.
  void SetWindowHeight(int height);

  // Forces the mediaplayer window to be closed.
  void CloseWindow();

  // Clears the current playlist.
  void ClearPlaylist();

  // Enqueues this fileschema url into the current playlist.
  void EnqueueMediaFileUrl(const GURL& url);

  // Clears out the current playlist, and start playback of the given url.
  void ForcePlayMediaURL(const GURL& url);

  // Popup the mediaplayer, this shows the browser, and sets up its
  // locations correctly.
  void PopupMediaPlayer();

  // Sets the currently playing element to the given positions.
  void SetPlaylistPosition(int position);

  // Returns current playlist.
  const UrlVector& GetPlaylist() const;

  // Returns current playlist position.
  int GetPlaylistPosition() const;

  // Notfies the mediaplayer that the playlist changed. This could be
  // called from the mediaplayer itself for example.
  void NotifyPlaylistChanged();

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

  GURL GetMediaPlayerUrl() const;

  // Browser containing the Mediaplayer.  Used to force closes. This is
  // created by the PopupMediaplayer call, and is NULLed out when the window
  // is closed.
  Browser* mediaplayer_browser_;

  // Used to register for events on the windows, like to listen for closes.
  content::NotificationRegistrar registrar_;

  friend class MediaPlayerBrowserTest;
  DISALLOW_COPY_AND_ASSIGN(MediaPlayer);
};

#endif  // CHROME_BROWSER_CHROMEOS_MEDIA_MEDIA_PLAYER_H_
