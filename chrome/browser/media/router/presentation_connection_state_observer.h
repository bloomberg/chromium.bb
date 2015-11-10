// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PRESENTATION_CONNECTION_STATE_OBSERVER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PRESENTATION_CONNECTION_STATE_OBSERVER_H_

#include "base/macros.h"
#include "chrome/browser/media/router/media_route.h"

namespace media_router {

class MediaRouter;

// Base class for observing state changes on a PresentationConnection.
class PresentationConnectionStateObserver {
 public:
  // |router|: The MediaRouter instance to register this instance with.
  // |route_id|: ID of MediaRoute connected to the PresentationConnection to
  // be observed.
  PresentationConnectionStateObserver(MediaRouter* router,
                                      const MediaRoute::Id& route_id);
  virtual ~PresentationConnectionStateObserver();

  MediaRoute::Id route_id() const { return route_id_; }

  // Invoked when the state of PresentationConnection represented by |route_id_|
  // has changed to |state|.
  virtual void OnStateChanged(content::PresentationConnectionState state) {}

 private:
  MediaRouter* const router_;
  const MediaRoute::Id route_id_;

  DISALLOW_COPY_AND_ASSIGN(PresentationConnectionStateObserver);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PRESENTATION_CONNECTION_STATE_OBSERVER_H_
