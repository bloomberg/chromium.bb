// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PRESENTATION_SESSION_STATE_OBSERVER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PRESENTATION_SESSION_STATE_OBSERVER_H_

#include <vector>

#include "base/gtest_prod_util.h"
#include "chrome/browser/media/router/media_route.h"
#include "chrome/browser/media/router/media_routes_observer.h"
#include "content/public/browser/presentation_service_delegate.h"

namespace media_router {

class MediaRouter;

// TODO(imcheng): Remove this class and use PresentationConnectionStateObserver
// instead (crbug.com/529893).
// MediaRoutesObserver implementation that allows tracking state changes from
// multiple presentation sessions and notifying the client via
// |state_changed_callback|.
// Clients provide the callback on construction, and incrementally add route
// IDs to track by calling |OnPresentationSessionConnected| as they are created,
// which also has the effect of invoking the callback that the session has
// become connected.
// When a presentation is disconnected, the callback will be invoked and it
// will be removed from the tracked set.
// When the routes no longer need to tracked, |Reset| can be called.
class PresentationSessionStateObserver : public MediaRoutesObserver {
 public:
  PresentationSessionStateObserver(
      const content::SessionStateChangedCallback& state_changed_callback,
      const MediaRouteIdToPresentationSessionMapping* route_id_to_presentation,
      MediaRouter* router);
  ~PresentationSessionStateObserver() override;

  // Called when a presentation with |route_id| has been created in connected
  // state and invoke the callback.
  void OnPresentationSessionConnected(const MediaRoute::Id& route_id);

  // Clears the set of presentations being tracked.
  void Reset();

 private:
  FRIEND_TEST_ALL_PREFIXES(PresentationSessionStateObserverTest,
                           InvokeCallbackWithDisconnected);
  FRIEND_TEST_ALL_PREFIXES(PresentationSessionStateObserverTest, Reset);

  // MediaRoutesObserver override
  void OnRoutesUpdated(const std::vector<MediaRoute>& routes) override;

  // Invokes |state_changed_callback_| with PresentationSessionInfo derived from
  // |route_id| and |new_state|, if |route_id| is valid.
  void InvokeCallback(const MediaRoute::Id& route_id,
                      content::PresentationConnectionState new_state);

  // Route IDs of presentations that are being tracked for state changes.
  // It is built by adding entries when a presentation is started,
  // and removing entries when the presentation is no longer in subsequent
  // route list updates.
  std::vector<MediaRoute::Id> tracked_route_ids_;

  // Stores previous list of routes observed from MediaRouter.
  // It is compared against the new observed list of routes for disconnected
  // routes that are in |tracked_route_ids_|.
  // TODO(imcheng): A more explicit set of MediaRoutesObserver APIs (e.g.,
  // OnRoutesClosed) would be helpful here. (crbug.com/508751)
  std::vector<MediaRoute::Id> previous_route_ids_;

  // Callback to invoke when state change occurs.
  content::SessionStateChangedCallback state_changed_callback_;

  // Note that while it is declared const to prevent this class from modifying
  // it, its contents can be changed by the PresentationFrame that owns it.
  const MediaRouteIdToPresentationSessionMapping* const
      route_id_to_presentation_;

  DISALLOW_COPY_AND_ASSIGN(PresentationSessionStateObserver);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PRESENTATION_SESSION_STATE_OBSERVER_H_
