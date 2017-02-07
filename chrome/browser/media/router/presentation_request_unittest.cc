// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/presentation_request.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_router {

TEST(PresentationRequestTest, Equals) {
  url::Origin frame_origin(GURL("http://www.site.com/"));
  std::vector<GURL> presentation_urls = {
      GURL("http://www.example.com/presentation.html"),
      GURL("http://www.example.net/alternate.html")};

  PresentationRequest request1(RenderFrameHostId(1, 2), presentation_urls,
                               frame_origin);

  // Frame IDs are different.
  PresentationRequest request2(RenderFrameHostId(3, 4), presentation_urls,
                               frame_origin);
  EXPECT_FALSE(request1.Equals(request2));

  // Presentation URLs are different.
  PresentationRequest request3(
      RenderFrameHostId(1, 2),
      {GURL("http://www.example.net/presentation.html"),
       GURL("http://www.example.com/presentation.html")},
      frame_origin);
  EXPECT_FALSE(request1.Equals(request3));

  // Frame URLs are different.
  PresentationRequest request4(RenderFrameHostId(1, 2), presentation_urls,
                               url::Origin(GURL("http://www.site.net/")));
  EXPECT_FALSE(request1.Equals(request4));

  PresentationRequest request5(
      RenderFrameHostId(1, 2),
      {GURL("http://www.example.com/presentation.html"),
       GURL("http://www.example.net/alternate.html")},
      url::Origin(GURL("http://www.site.com/")));
  EXPECT_TRUE(request1.Equals(request5));
}

}  // namespace media_router
