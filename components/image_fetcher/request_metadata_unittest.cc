// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/request_metadata.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace image_fetcher {

TEST(RequestMetadataTest, Equality) {
  RequestMetadata rhs;
  RequestMetadata lhs;
  rhs.mime_type = "testMimeType";
  lhs.mime_type = "testMimeType";

  EXPECT_EQ(rhs, lhs);
}

TEST(RequestMetadataTest, NoEquality) {
  RequestMetadata rhs;
  RequestMetadata lhs;
  rhs.mime_type = "testMimeType";
  lhs.mime_type = "testOtherMimeType";

  EXPECT_NE(rhs, lhs);
}

}  // namespace image_fetcher
