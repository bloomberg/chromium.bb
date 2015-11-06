// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BACKGROUND_SYNC_CONTROLLER_H_
#define CONTENT_PUBLIC_BROWSER_BACKGROUND_SYNC_CONTROLLER_H_

#include "content/common/content_export.h"
#include "url/gurl.h"

namespace content {

class BackgroundSyncManager;

// An interface that the Background Sync API uses to access services from the
// embedder. Must only be used on the UI thread.
class CONTENT_EXPORT BackgroundSyncController {
 public:
  virtual ~BackgroundSyncController() {}

  // Notification that a service worker registration with origin |origin| just
  // registered a background sync event.
  virtual void NotifyBackgroundSyncRegistered(const GURL& origin) {}

  // If |enabled|, ensures that the browser is running when the device next goes
  // online. The behavior is platform dependent:
  // * Android: Registers a GCM task which verifies that the browser is running
  // the next time the device goes online. If it's not, it starts it.
  //
  // * Other Platforms: (UNIMPLEMENTED) Keeps the browser alive via
  // BackgroundModeManager until called with |enabled| = false.
  virtual void RunInBackground(bool enabled) {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BACKGROUND_SYNC_CONTROLLER_H_
