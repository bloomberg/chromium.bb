// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_MEDIA_MEDIA_PLAYER_EXTENSION_API_H_
#define CHROME_BROWSER_CHROMEOS_MEDIA_MEDIA_PLAYER_EXTENSION_API_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "chrome/browser/extensions/extension_function.h"

// Implements the chrome.mediaPlayerPrivate.play method.
class PlayMediaplayerFunction : public SyncExtensionFunction {
 protected:
  // SyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("mediaPlayerPrivate.play");
};

// Implements the chrome.mediaPlayerPrivate.getPlaylist method.
class GetPlaylistMediaplayerFunction : public SyncExtensionFunction {
 protected:
  // SyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("mediaPlayerPrivate.getPlaylist");
};

// Implements the chrome.mediaPlayerPrivate.setWindowHeight method.
class SetWindowHeightMediaplayerFunction : public SyncExtensionFunction {
 protected:
  // SyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("mediaPlayerPrivate.setWindowHeight");
};

// Implements the chrome.mediaPlayerPrivate.closeWindow method.
class CloseWindowMediaplayerFunction : public SyncExtensionFunction {
 protected:
  // SyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("mediaPlayerPrivate.closeWindow");
};

#endif  // CHROME_BROWSER_CHROMEOS_MEDIA_MEDIA_PLAYER_EXTENSION_API_H_
