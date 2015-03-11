// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/route_id_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media_router {

TEST(RouteIDManagerTest, ForLocalRoute) {
  // Singleton instance is left unused for better test isolation.
  RouteIDManager manager1;
  EXPECT_EQ("route-local-0", manager1.ForLocalRoute());
  EXPECT_EQ("route-local-1", manager1.ForLocalRoute());

  RouteIDManager manager2;
  EXPECT_EQ("route-local-0", manager2.ForLocalRoute());
}

}  // namespace media_router
