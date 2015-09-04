// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/media_route.h"
#include "chrome/browser/media/router/media_sink.h"
#include "chrome/browser/media/router/media_source_helper.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {
const char kRouteId1[] =
    "urn:x-org.chromium:media:route:1/cast-sink1/http://foo.com";
const char kRouteId2[] =
    "urn:x-org.chromium:media:route:2/cast-sink2/http://foo.com";
}  // namespace

namespace media_router {

// Tests the == operator to ensure that only route ID equality is being checked.
TEST(MediaRouteTest, Equals) {
  MediaRoute route1(kRouteId1, MediaSourceForCastApp("DialApp"), "sinkId",
                    "Description", false, "", false);

  // Same as route1 with different sink ID.
  MediaRoute route2(kRouteId1, MediaSourceForCastApp("DialApp"),
                    "differentSinkId", "Description", false, "", false);
  EXPECT_TRUE(route1.Equals(route2));

  // Same as route1 with different description.
  MediaRoute route3(kRouteId1, MediaSourceForCastApp("DialApp"), "sinkId",
                    "differentDescription", false, "", false);
  EXPECT_TRUE(route1.Equals(route3));

  // Same as route1 with different is_local.
  MediaRoute route4(kRouteId1, MediaSourceForCastApp("DialApp"), "sinkId",
                    "Description", true, "", false);
  EXPECT_TRUE(route1.Equals(route4));

  // The ID is different from route1's.
  MediaRoute route5(kRouteId2, MediaSourceForCastApp("DialApp"), "sinkId",
                    "Description", false, "", false);
  EXPECT_FALSE(route1.Equals(route5));
}

}  // namespace media_router
