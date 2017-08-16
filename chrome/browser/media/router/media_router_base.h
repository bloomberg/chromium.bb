// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_BASE_H_
#define CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_BASE_H_

#include <set>
#include <unordered_map>
#include <vector>

#include "base/callback_list.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/media/router/media_router.h"
#include "chrome/browser/media/router/media_routes_observer.h"
#include "chrome/common/media_router/media_route.h"

namespace media_router {

class MediaRouterBase : public MediaRouter {
 public:
  ~MediaRouterBase() override;

  std::unique_ptr<PresentationConnectionStateSubscription>
  AddPresentationConnectionStateChangedCallback(
      const MediaRoute::Id& route_id,
      const content::PresentationConnectionStateChangedCallback& callback)
      override;

  // Called when the incognito profile for this instance is being shut down.
  // This will terminate all incognito media routes.
  void OnIncognitoProfileShutdown() override;

  std::vector<MediaRoute> GetCurrentRoutes() const override;

#if !defined(OS_ANDROID)
  scoped_refptr<MediaRouteController> GetRouteController(
      const MediaRoute::Id& route_id) override;
#endif  // !defined(OS_ANDROID)

  void RegisterRemotingSource(int32_t tab_id,
                              CastRemotingConnector* remoting_source) override;
  void UnregisterRemotingSource(int32_t tab_id) override;

 protected:
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoImplTest,
                           PresentationConnectionStateChangedCallback);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoImplTest,
                           PresentationConnectionStateChangedCallbackRemoved);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterBaseTest, CreatePresentationIds);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterBaseTest, NotifyCallbacks);

  MediaRouterBase();

  // Generates a unique presentation id. Shared between Android and desktop.
  static std::string CreatePresentationId();

  void NotifyPresentationConnectionStateChange(
      const MediaRoute::Id& route_id,
      content::PresentationConnectionState state);
  void NotifyPresentationConnectionClose(
      const MediaRoute::Id& route_id,
      content::PresentationConnectionCloseReason reason,
      const std::string& message);

  // Returns true when there is at least one MediaRoute that can be returned by
  // JoinRoute().
  bool HasJoinableRoute() const;

  // Returns true if there is a route with the ID in the current list of routes.
  bool IsRouteKnown(const std::string& route_id) const;

  using PresentationConnectionStateChangedCallbacks = base::CallbackList<void(
      const content::PresentationConnectionStateChangeInfo&)>;

  std::unordered_map<
      MediaRoute::Id,
      std::unique_ptr<PresentationConnectionStateChangedCallbacks>>
      presentation_connection_state_callbacks_;

  // Stores CastRemotingConnectors that can be connected to the MediaRemoter
  // for media remoting when MediaRemoter is started. The map uses the tab ID
  // as the key.
  std::unordered_map<int32_t, CastRemotingConnector*> remoting_sources_;

 private:
  friend class MediaRouterBaseTest;
  friend class MediaRouterFactory;
  friend class MediaRouterMojoTest;

  class InternalMediaRoutesObserver;

  // Must be called before invoking any other method.
  void Initialize();

  // Called when a PresentationConnectionStateChangedCallback associated with
  // |route_id| is removed from |presentation_connection_state_callbacks_|.
  void OnPresentationConnectionStateCallbackRemoved(
      const MediaRoute::Id& route_id);

  // KeyedService
  void Shutdown() override;

#if !defined(OS_ANDROID)
  // MediaRouter
  void DetachRouteController(const MediaRoute::Id& route_id,
                             MediaRouteController* controller) override;
#endif  // !defined(OS_ANDROID)

  std::unique_ptr<InternalMediaRoutesObserver> internal_routes_observer_;
  bool initialized_;

  DISALLOW_COPY_AND_ASSIGN(MediaRouterBase);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_BASE_H_
