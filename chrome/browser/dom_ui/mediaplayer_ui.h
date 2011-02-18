// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_MEDIAPLAYER_UI_H_
#define CHROME_BROWSER_DOM_UI_MEDIAPLAYER_UI_H_
#pragma once

#include <set>
#include <vector>

#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/browser/webui/web_ui.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "net/base/directory_lister.h"
#include "net/url_request/url_request.h"

template <typename T> struct DefaultSingletonTraits;
class GURL;
class MediaplayerHandler;
class Browser;

class MediaPlayer : public NotificationObserver,
                    public net::URLRequest::Interceptor {
 public:
  ~MediaPlayer();

  // Enqueues this url into the current playlist.  If the mediaplayer is
  // not currently visible, show it, and play the given url.
  void EnqueueMediaURL(const GURL& url, Browser* creator);

  // Clears out the current playlist, and start playback of the given url.
  // If there is no mediaplayer currently, show it, and play the given url.
  void ForcePlayMediaURL(const GURL& url, Browser* creator);

  // Toggle the visibility of the playlist window.
  void TogglePlaylistWindowVisible();

  // Force the playlist window to be shown.
  void ShowPlaylistWindow();

  // Toggle the mediaplayer between fullscreen and windowed.
  void ToggleFullscreen();

  // Force the playlist window to be closed.
  void ClosePlaylistWindow();

  // Sets the currently playing element to the given offset.
  void SetPlaylistOffset(int offset);

  // Set a new playback handler to give events to, along with the
  // tab contents of the page which holds the mediaplayer. it is expected
  // That only one of these will exist at any given time.
  void SetNewHandler(MediaplayerHandler* handler,
                     TabContents* contents);

  // Removes the handler.
  void RemoveHandler(MediaplayerHandler* handler);

  // Registers a new playlist handler which receives events from the
  // mediaplayer, along with the tab contents which has the playlist in it.
  void RegisterNewPlaylistHandler(MediaplayerHandler* handler,
                                  TabContents* contents);

  // Removes the playlist handler.
  void RemovePlaylistHandler(MediaplayerHandler* handler);

  // Notfiys the mediaplayer that the playlist changed. This could be
  // called from the mediaplayer itself for example.
  void NotifyPlaylistChanged();

  // Always returns NULL because we don't want to attempt a redirect
  // before seeing the detected mime type of the request.
  // Implementation of net::URLRequest::Interceptor.
  virtual net::URLRequestJob* MaybeIntercept(net::URLRequest* request);

  // Determines if the requested document can be viewed by the
  // MediaPlayer.  If it can, returns a net::URLRequestJob that
  // redirects the browser to the view URL.
  // Implementation of net::URLRequest::Interceptor.
  virtual net::URLRequestJob* MaybeInterceptResponse(net::URLRequest* request);

  // Used to detect when the mediaplayer is closed.
  void Observe(NotificationType type,
               const NotificationSource& source,
               const NotificationDetails& details);

  // Getter for the singleton.
  static MediaPlayer* GetInstance();

 private:
  friend struct DefaultSingletonTraits<MediaPlayer>;

  MediaPlayer();

  // Popup the mediaplayer, this shows the browser, and sets up its
  // locations correctly.
  void PopupMediaPlayer(Browser* creator);

  // Checks to see the the mediaplayer is currently enabled
  bool Enabled();

  // Popup the playlist.  Shows the browser, sets it up to point at
  // chrome://mediaplayer#playlist
  void PopupPlaylist(Browser* creator);

  // Registers the listeners for the close events on the browser windows.
  void RegisterListeners();

  // Set when the register handler is called.  When the media player is
  // closed, this pointer is set back to NULL.
  MediaplayerHandler* handler_;

  // Set when the register playlist handler is called. When the playlist
  // is closed, this pointer is set back to NULL.
  MediaplayerHandler* playlist_;

  // Browser containing the playlist. Used to force closes. This is created
  // By the PopupPlaylist call, and is NULLed out when the window is closed.
  Browser* playlist_browser_;

  // Browser containing the Mediaplayer.  Used to force closes. This is
  // created by the PopupMediaplayer call, and is NULLed out when the window
  // is closed.
  Browser* mediaplayer_browser_;

  // List of URLs that were enqueued during the time that the mediaplayer
  // had not poped up yet. This is claered out after the mediaplayer pops up.
  std::vector<GURL> unhandled_urls_;

  // Used to register for events on the windows, like to listen for closes.
  NotificationRegistrar registrar_;

  // Tab contents of the mediaplayer.  Used to listen for events
  // which would cause the mediaplayer to be closed.  These are cleared out
  // when the mediaplayer is closed.
  TabContents* mediaplayer_tab_;

  // Tab contents of the playlist tab. used to listen for events which would
  // cause the mediaplayer to be closed.  These are cleared out when the
  // playlist is closed.
  TabContents* playlist_tab_;

  // List of mimetypes that the mediaplayer should listen to.  Used for
  // interceptions of url GETs.
  std::set<std::string> supported_mime_types_;
  DISALLOW_COPY_AND_ASSIGN(MediaPlayer);
};

class MediaplayerUI : public WebUI {
 public:
  explicit MediaplayerUI(TabContents* contents);

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaplayerUI);
};

#endif  // CHROME_BROWSER_DOM_UI_MEDIAPLAYER_UI_H_
