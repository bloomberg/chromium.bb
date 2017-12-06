// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/resources/buffer_to_texture_target_map.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/buffer_types.h"

namespace viz {
namespace {

// Ensures that a list populated with various values can be serialized to/from
// string successfully.
TEST(BuffferUsageAndFormatList, SerializeRoundTrip) {
  BufferUsageAndFormatList test_list;
  for (int usage_idx = 0; usage_idx <= static_cast<int>(gfx::BufferUsage::LAST);
       ++usage_idx) {
    gfx::BufferUsage usage = static_cast<gfx::BufferUsage>(usage_idx);
    for (int format_idx = 0;
         format_idx <= static_cast<int>(gfx::BufferFormat::LAST);
         ++format_idx) {
      gfx::BufferFormat format = static_cast<gfx::BufferFormat>(format_idx);
      test_list.push_back(std::make_pair(usage, format));
    }
  }

  std::string serialized_list = BufferUsageAndFormatListToString(test_list);
  BufferUsageAndFormatList deserialized_list =
      StringToBufferUsageAndFormatList(serialized_list);
  EXPECT_EQ(test_list, deserialized_list);
}

}  // namespace
}  // namespace viz
