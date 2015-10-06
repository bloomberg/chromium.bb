// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_LOCAL_MEDIA_ROUTES_OBSERVER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_LOCAL_MEDIA_ROUTES_OBSERVER_H_

#include "base/macros.h"

namespace media_router {

class MediaRouter;

// Base class for observing whether local routes exist in the Media Router.
class LocalMediaRoutesObserver {
 public:
  explicit LocalMediaRoutesObserver(MediaRouter* router);
  virtual ~LocalMediaRoutesObserver();

  // Called when a local route has been successfully created or if there has
  // been an update to the list of routes.
  virtual void OnHasLocalRouteUpdated(bool has_local_route) {}

 private:
  MediaRouter* const router_;

  DISALLOW_COPY_AND_ASSIGN(LocalMediaRoutesObserver);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_LOCAL_MEDIA_ROUTES_OBSERVER_H_
