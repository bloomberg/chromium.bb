// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/image_utils.h"

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace zucchini {

TEST(ImageUtilsTest, Bitness) {
  EXPECT_EQ(4U, WidthOf(kBit32));
  EXPECT_EQ(8U, WidthOf(kBit64));
}

}  // namespace zucchini
