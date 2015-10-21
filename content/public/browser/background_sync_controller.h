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

  // Register the |registrant|'s interest (or disinterest) in starting the
  // browser the next time the device goes online after the browser has closed.
  // This only needs to be implemented by browsers; WebView and other embedders
  // which should not have their application relaunched by Background Sync can
  // use the default implentation.
  virtual void LaunchBrowserWhenNextOnline(
      const BackgroundSyncManager* registrant,
      bool launch_when_next_online) {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BACKGROUND_SYNC_CONTROLLER_H_
