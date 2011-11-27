// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_MEDIAPLAYER_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_MEDIAPLAYER_PRIVATE_API_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "chrome/browser/extensions/extension_function.h"

// Implements the chrome.mediaplayerPrivate.playAt method.
class PlayAtMediaplayerFunction
    : public SyncExtensionFunction {
 protected:
  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME(
      "mediaPlayerPrivate.playAt");
};

// Implements the chrome.mediaPlayerPrivate.getCurrentPlaylist method.
class GetPlaylistMediaplayerFunction : public SyncExtensionFunction {
 protected:
  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("mediaPlayerPrivate.getPlaylist");
};

// Implements the chrome.mediaPlayerPrivate.setPlaybackError method.
class SetPlaybackErrorMediaplayerFunction : public SyncExtensionFunction {
 protected:
  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("mediaPlayerPrivate.setPlaybackError");
};

// Implements the chrome.mediaPlayerPrivate.togglePlaylist method.
class TogglePlaylistPanelMediaplayerFunction : public SyncExtensionFunction {
 protected:
  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("mediaPlayerPrivate.togglePlaylistPanel");
};

// Implements the chrome.mediaPlayerPrivate.toggleFullscreen method.
class ToggleFullscreenMediaplayerFunction : public SyncExtensionFunction {
 protected:
  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("mediaPlayerPrivate.toggleFullscreen");
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_MEDIAPLAYER_PRIVATE_API_H_
