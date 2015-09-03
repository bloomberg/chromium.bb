// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/media_sink.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media_router {

TEST(MediaSinkTest, Equals) {
  MediaSink sink1("sinkId", "Sink", MediaSink::CAST);

  // No name, same as sink1.
  MediaSink sink2("sinkId", "", MediaSink::CAST);
  EXPECT_TRUE(sink1.Equals(sink2));

  // Sink name is different from sink1's.
  MediaSink sink3("sinkId", "Other Sink", MediaSink::CAST);
  EXPECT_TRUE(sink1.Equals(sink3));

  // Sink ID is diffrent from sink1's.
  MediaSink sink4("otherSinkId", "Sink", MediaSink::CAST);
  EXPECT_FALSE(sink1.Equals(sink4));

  // Sink icon type is diffrent from sink1's.
  MediaSink sink5("otherSinkId", "Sink", MediaSink::GENERIC);
  EXPECT_FALSE(sink1.Equals(sink5));
}

}  // namespace media_router
