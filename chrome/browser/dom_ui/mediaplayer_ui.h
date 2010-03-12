// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_MEDIAPLAYER_UI_H_
#define CHROME_BROWSER_DOM_UI_MEDIAPLAYER_UI_H_

#include <vector>

#include "base/file_path.h"
#include "base/scoped_ptr.h"
#include "base/singleton.h"
#include "base/values.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/browser/dom_ui/dom_ui.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/notification_registrar.h"
#include "net/base/directory_lister.h"
#include "net/url_request/url_request.h"

class GURL;
class MediaplayerHandler;
class Browser;

class MediaPlayer : public NotificationObserver,
                    public URLRequest::Interceptor {
 public:
  ~MediaPlayer() {}
  void EnqueueMediaURL(const GURL& url);
  void ForcePlayMediaURL(const GURL& url);
  void TogglePlaylistWindowVisible();
  void ShowPlaylistWindow();
  void ClosePlaylistWindow();
  void SetPlaylistOffset(int offset);
  void RegisterNewHandler(MediaplayerHandler* handler,
                          TabContents* contents);
  void RemoveHandler(MediaplayerHandler* handler);
  void RegisterNewPlaylistHandler(MediaplayerHandler* handler,
                                  TabContents* contents);
  void RemovePlaylistHandler(MediaplayerHandler* handler);
  void NotifyPlaylistChanged();

  // Always returns NULL because we don't want to attempt a redirect
  // before seeing the detected mime type of the request.
  virtual URLRequestJob* MaybeIntercept(URLRequest* request);

  // Determines if the requested document can be viewed by the
  // MediaPlayer.  If it can, returns a URLRequestJob that
  // redirects the browser to the view URL.
  virtual URLRequestJob* MaybeInterceptResponse(URLRequest* request);

  // Used to detect when the mediaplayer is closed
  void Observe(NotificationType type,
               const NotificationSource& source,
               const NotificationDetails& details);

  static MediaPlayer* Get() {
    return Singleton<MediaPlayer>::get();
  }
  void set_profile(Profile* profile) { profile_ = profile; }

 private:
  MediaPlayer();
  void PopupMediaPlayer();
  void PopupPlaylist();
  void RegisterListeners();

  Profile* profile_;
  MediaplayerHandler* handler_;
  MediaplayerHandler* playlist_;
  Browser* playlist_browser_;
  Browser* mediaplayer_browser_;
  std::vector<GURL> unhandled_urls_;
  NotificationRegistrar registrar_;
  TabContents* mediaplayer_tab_;
  TabContents* playlist_tab_;
  base::hash_set<std::string> supported_mime_types_;
  friend struct DefaultSingletonTraits<MediaPlayer>;
  DISALLOW_COPY_AND_ASSIGN(MediaPlayer);
};

class MediaplayerUI : public DOMUI {
 public:
  explicit MediaplayerUI(TabContents* contents);

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaplayerUI);
};

#endif  // CHROME_BROWSER_DOM_UI_MEDIAPLAYER_UI_H_
