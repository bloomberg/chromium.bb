// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_BASE_H_
#define CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_BASE_H_

#include <set>
#include <vector>

#include "base/callback_list.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/media/router/media_route.h"
#include "chrome/browser/media/router/media_router.h"
#include "chrome/browser/media/router/media_routes_observer.h"

namespace media_router {

class MediaRouterBase : public MediaRouter {
 public:
  ~MediaRouterBase() override;

  std::unique_ptr<PresentationConnectionStateSubscription>
  AddPresentationConnectionStateChangedCallback(
      const MediaRoute::Id& route_id,
      const content::PresentationConnectionStateChangedCallback& callback)
      override;

  // Called when the off the record (incognito) profile for this instance is
  // being shut down.  This will terminate all off the record media routes.
  void OnOffTheRecordProfileShutdown() override;

 protected:
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoImplTest,
                           PresentationConnectionStateChangedCallback);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoImplTest,
                           PresentationConnectionStateChangedCallbackRemoved);

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

  // Returns true when there is at least one MediaRoute with is_local = true.
  bool HasLocalRoute() const;

  using PresentationConnectionStateChangedCallbacks = base::CallbackList<void(
      const content::PresentationConnectionStateChangeInfo&)>;

  base::ScopedPtrHashMap<
      MediaRoute::Id,
      std::unique_ptr<PresentationConnectionStateChangedCallbacks>>
      presentation_connection_state_callbacks_;

  base::ThreadChecker thread_checker_;

 private:
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

  std::unique_ptr<InternalMediaRoutesObserver> internal_routes_observer_;
  bool initialized_;

  DISALLOW_COPY_AND_ASSIGN(MediaRouterBase);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_BASE_H_
