// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_ANDROID_MEDIA_WEB_CONTENTS_OBSERVER_ANDROID_H_
#define CONTENT_BROWSER_MEDIA_ANDROID_MEDIA_WEB_CONTENTS_OBSERVER_ANDROID_H_

#include <stdint.h>

#include <memory>
#include <unordered_map>

#include "base/macros.h"
#include "content/browser/media/media_web_contents_observer.h"
#include "content/common/content_export.h"

namespace media {
enum class MediaContentType;
}  // namespace media

namespace content {

// This class adds Android specific extensions to the MediaWebContentsObserver.
class CONTENT_EXPORT MediaWebContentsObserverAndroid
    : public MediaWebContentsObserver {
 public:
  explicit MediaWebContentsObserverAndroid(WebContents* web_contents);
  ~MediaWebContentsObserverAndroid() override;

  // Returns the android specific observer for a given web contents.
  static MediaWebContentsObserverAndroid* FromWebContents(
      WebContents* web_contents);

  // Called by the WebContents when a tab has been closed but may still be
  // available for "undo" -- indicates that all media players (even audio only
  // players typically allowed background audio) bound to this WebContents must
  // be suspended.
  void SuspendAllMediaPlayers();
 private:
  DISALLOW_COPY_AND_ASSIGN(MediaWebContentsObserverAndroid);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_ANDROID_MEDIA_WEB_CONTENTS_OBSERVER_ANDROID_H_
