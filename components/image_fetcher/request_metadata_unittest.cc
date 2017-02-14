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
  rhs.response_code = 1;
  lhs.response_code = 1;

  EXPECT_EQ(rhs, lhs);
}

TEST(RequestMetadataTest, NoEquality) {
  RequestMetadata rhs;
  RequestMetadata lhs;
  rhs.mime_type = "testMimeType";
  lhs.mime_type = "testMimeType";
  rhs.response_code = 1;
  lhs.response_code = 1;

  lhs.mime_type = "testOtherMimeType";
  EXPECT_NE(rhs, lhs);
  lhs.mime_type = "testMimeType";

  lhs.response_code = 2;
  EXPECT_NE(rhs, lhs);
  lhs.response_code = 1;
}

}  // namespace image_fetcher
