// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cronet/Cronet.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace cronet {

TEST(QuicTest, InvalidQuicHost) {
  BOOL success =
      [Cronet addQuicHint:@"https://test.example.com/" port:443 altPort:443];

  EXPECT_FALSE(success);
}

TEST(QuicTest, ValidQuicHost) {
  BOOL success = [Cronet addQuicHint:@"test.example.com" port:443 altPort:443];

  EXPECT_TRUE(success);
}
}
