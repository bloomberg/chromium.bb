// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_MEDIA_PLAYER_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_MEDIA_PLAYER_API_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/api/profile_keyed_api_factory.h"
#include "chrome/browser/extensions/extension_function.h"

class Profile;

namespace extensions {
class MediaPlayerEventRouter;

// Implements the chrome.mediaPlayerPrivate.play method.
class PlayMediaplayerFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("mediaPlayerPrivate.play");

 protected:
  virtual ~PlayMediaplayerFunction() {}

  // SyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;
};

// Implements the chrome.mediaPlayerPrivate.getPlaylist method.
class GetPlaylistMediaplayerFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("mediaPlayerPrivate.getPlaylist");

 protected:
  virtual ~GetPlaylistMediaplayerFunction() {}

  // SyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;
};

// Implements the chrome.mediaPlayerPrivate.setWindowHeight method.
class SetWindowHeightMediaplayerFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("mediaPlayerPrivate.setWindowHeight");

 protected:
  virtual ~SetWindowHeightMediaplayerFunction() {}

  // SyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;
};

// Implements the chrome.mediaPlayerPrivate.closeWindow method.
class CloseWindowMediaplayerFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("mediaPlayerPrivate.closeWindow");

 protected:
  virtual ~CloseWindowMediaplayerFunction() {}

  // SyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;
};

class MediaPlayerAPI : public ProfileKeyedAPI {
 public:
  explicit MediaPlayerAPI(Profile* profile);
  virtual ~MediaPlayerAPI();

  // Convenience method to get the MediaPlayerAPI for a profile.
  static MediaPlayerAPI* Get(Profile* profile);

  MediaPlayerEventRouter* media_player_event_router();

  // ProfileKeyedAPI implementation.
  static ProfileKeyedAPIFactory<MediaPlayerAPI>* GetFactoryInstance();

 private:
  friend class ProfileKeyedAPIFactory<MediaPlayerAPI>;

  Profile* const profile_;

  // ProfileKeyedAPI implementation.
  static const char* service_name() {
    return "MediaPlayerAPI";
  }
  static const bool kServiceIsNULLWhileTesting = true;

  scoped_ptr<MediaPlayerEventRouter> media_player_event_router_;

  DISALLOW_COPY_AND_ASSIGN(MediaPlayerAPI);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_MEDIA_PLAYER_API_H_
