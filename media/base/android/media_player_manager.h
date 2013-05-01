// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MEDIA_PLAYER_MANAGER_H_
#define MEDIA_BASE_ANDROID_MEDIA_PLAYER_MANAGER_H_

#include "media/base/media_export.h"

namespace media {

class MediaPlayerBridge;
class MediaResourceGetter;

// This class is responsible for managing active MediaPlayerBridge objects.
// It is implemented by content::MediaPlayerManagerImpl.
class MEDIA_EXPORT MediaPlayerManager {
 public:
  virtual ~MediaPlayerManager();

  // Called by a MediaPlayerBridge object when it is going to decode
  // media streams. This helps the manager object maintain an array
  // of active MediaPlayerBridge objects and release the resources
  // when needed.
  virtual void RequestMediaResources(MediaPlayerBridge* player) = 0;

  // Called when a MediaPlayerBridge object releases all its decoding
  // resources.
  virtual void ReleaseMediaResources(MediaPlayerBridge* player) = 0;

  // Return a pointer to the MediaResourceGetter object.
  virtual MediaResourceGetter* GetMediaResourceGetter() = 0;
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MEDIA_PLAYER_MANAGER_H_
