// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_IMPL_H_
#define CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_IMPL_H_

#include <map>
#include <queue>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/hash_tables.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/media/router/media_router.h"
#include "chrome/browser/media/router/media_sinks_observer.h"
#include "components/keyed_service/core/keyed_service.h"

namespace base {
template <typename T>
struct DefaultLazyInstanceTraits;
}  // namespace base

namespace media_router {

class MediaRoute;
class MediaRouterApiImpl;
class MediaRoutesObserver;
struct RoutesQueryResult;

// Delegates calls to another implementation of MediaRouter, which in turn
// handles communication with the Media Route Provider Manager (MRPM).
// Instances of this class are created by MediaRouterImplFactory.
// MediaRouterImpl is a profile-keyed service.
// MediaRouterImpl runs on the browser UI thread. All observers/callbacks
// registered to this class must also run in the UI thread.
class MediaRouterImpl : public MediaRouter, public KeyedService {
 public:
  ~MediaRouterImpl() override;

  // |provider_manager_host_| must be null at the time this is called.
  // |provider_manager_host| must not be null.
  // Does not take ownership of |provider_manager_host_|.
  // This function can only be called once.
  void Initialize(MediaRouter* provider_manager_host);

  // MediaRouter implementation.
  void RequestRoute(const MediaSourceId& source,
                    const MediaSinkId& sink_id,
                    const MediaRouteResponseCallback& callback) override;
  void CloseRoute(const MediaRouteId& route_id) override;
  void PostMessage(const MediaRouteId& route_id,
                   const std::string& message) override;

 private:
  friend class MediaRouterImplFactory;

  // Use MediaRouterImplFactory::GetForBrowserContext().
  MediaRouterImpl();

  // MediaRouter implementation.
  bool RegisterMediaSinksObserver(MediaSinksObserver* observer) override;
  void UnregisterMediaSinksObserver(MediaSinksObserver* observer) override;
  bool RegisterMediaRoutesObserver(MediaRoutesObserver* observer) override;
  void UnregisterMediaRoutesObserver(MediaRoutesObserver* observer) override;

  // MRPM host to forward calls to MRPM to.
  // This class does not own provider_manager_host_.
  MediaRouter* provider_manager_host_;

  DISALLOW_COPY_AND_ASSIGN(MediaRouterImpl);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_IMPL_H_
