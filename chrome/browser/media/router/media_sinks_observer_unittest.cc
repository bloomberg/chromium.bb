// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/media_source_helper.h"
#include "chrome/browser/media/router/mock_media_router.h"
#include "chrome/browser/media/router/test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_router {

TEST(MediaSinksObserverTest, OriginMatching) {
  MockMediaRouter router;
  MediaSource source(
      MediaSourceForPresentationUrl(GURL("https://presentation.com")));
  url::Origin origin{GURL("https://origin.com")};
  std::vector<url::Origin> origin_list({origin});
  std::vector<MediaSink> sink_list;
  sink_list.push_back(MediaSink("sinkId", "Sink", MediaSink::IconType::CAST));
  MockMediaSinksObserver observer(&router, source, origin);

  EXPECT_CALL(observer, OnSinksReceived(SequenceEquals(sink_list)));
  observer.OnSinksUpdated(sink_list, origin_list);

  EXPECT_CALL(observer, OnSinksReceived(SequenceEquals(sink_list)));
  observer.OnSinksUpdated(sink_list, std::vector<url::Origin>());

  url::Origin origin2{GURL("https://differentOrigin.com")};
  origin_list.clear();
  origin_list.push_back(origin2);
  EXPECT_CALL(observer, OnSinksReceived(testing::IsEmpty()));
  observer.OnSinksUpdated(sink_list, origin_list);
}

}  // namespace media_router
