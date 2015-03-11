// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/media_route.h"
#include "chrome/browser/media/router/media_sink.h"
#include "chrome/browser/media/router/media_source_helper.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media_router {

// Tests the == operator to ensure that only route ID equality is being checked.
TEST(MediaRouteTest, Equals) {
  MediaRoute route1("routeId1", ForCastAppMediaSource("DialApp"),
                    MediaSink("sinkId", "sinkName"),
                    "Description", false);

  // Same as route1 with different sink ID.
  MediaRoute route2("routeId1", ForCastAppMediaSource("DialApp"),
                    MediaSink("differentSinkId", "different sink"),
                    "Description", false);
  EXPECT_TRUE(route1.Equals(route2));

  // Same as route1 with different description.
  MediaRoute route3("routeId1", ForCastAppMediaSource("DialApp"),
                    MediaSink("sinkId", "sinkName"),
                    "differentDescription", false);
  EXPECT_TRUE(route1.Equals(route3));

  // Same as route1 with different is_local.
  MediaRoute route4("routeId1", ForCastAppMediaSource("DialApp"),
                    MediaSink("sinkId", "sinkName"),
                    "Description", true);
  EXPECT_TRUE(route1.Equals(route4));

  // The ID is different from route1's.
  MediaRoute route5("routeId2", ForCastAppMediaSource("DialApp"),
                    MediaSink("sinkId", "sinkName"),
                    "Description", false);
  EXPECT_FALSE(route1.Equals(route5));
}

}  // namespace media_router
