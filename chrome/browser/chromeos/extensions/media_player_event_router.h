// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_MEDIA_PLAYER_EVENT_ROUTER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_MEDIA_PLAYER_EVENT_ROUTER_H_
#pragma once

#include "base/basictypes.h"

class Profile;
template <typename T> struct DefaultSingletonTraits;

// Event router class for events related to Mediaplayer.
class ExtensionMediaPlayerEventRouter {
 public:
  static ExtensionMediaPlayerEventRouter* GetInstance();

  void Init(Profile* profile);

  void NotifyPlaylistChanged();

 private:
  Profile* profile_;

  ExtensionMediaPlayerEventRouter();
  friend struct DefaultSingletonTraits<ExtensionMediaPlayerEventRouter>;
  DISALLOW_COPY_AND_ASSIGN(ExtensionMediaPlayerEventRouter);
};

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_MEDIA_PLAYER_EVENT_ROUTER_H_
