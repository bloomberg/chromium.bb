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

#endif  // CHROME_BROWSER_CHROMEOS_MEDIA_MEDIA_PLAYER_EXTENSION_API_H_
