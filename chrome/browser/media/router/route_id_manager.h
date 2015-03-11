// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_ROUTE_ID_MANAGER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_ROUTE_ID_MANAGER_H_

#include <string>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/singleton.h"
#include "base/threading/non_thread_safe.h"

namespace media_router {

class MediaRoute;

// Shared singleton which coordinates the assignment of unique IDs to
// MediaRoute objects.
// Class is not threadsafe; IDs should be created on the same thread.
class RouteIDManager : public base::NonThreadSafe {
 public:
  static RouteIDManager* GetInstance();

  std::string ForLocalRoute();

 private:
  FRIEND_TEST_ALL_PREFIXES(RouteIDManagerTest, ForLocalRoute);
  friend struct DefaultSingletonTraits<RouteIDManager>;

  RouteIDManager();

  // Monotonically increasing ID number.
  uint64 next_local_id_;

  DISALLOW_COPY_AND_ASSIGN(RouteIDManager);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_ROUTE_ID_MANAGER_H_
