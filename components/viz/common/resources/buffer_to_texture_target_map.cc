// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/viz/common/resources/buffer_to_texture_target_map.h"

#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"

namespace viz {

BufferUsageAndFormatList StringToBufferUsageAndFormatList(
    const std::string& str) {
  BufferUsageAndFormatList usage_format_list;
  std::vector<std::string> entries =
      base::SplitString(str, ";", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  for (const auto& entry : entries) {
    std::vector<std::string> fields = base::SplitString(
        entry, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    CHECK_EQ(fields.size(), 2u);
    uint32_t usage = 0;
    uint32_t format = 0;
    bool succeeded = base::StringToUint(fields[0], &usage) &&
                     base::StringToUint(fields[1], &format);
    CHECK(succeeded);
    CHECK_LE(usage, static_cast<uint32_t>(gfx::BufferUsage::LAST));
    CHECK_LE(format, static_cast<uint32_t>(gfx::BufferFormat::LAST));
    usage_format_list.push_back(
        std::make_pair(static_cast<gfx::BufferUsage>(usage),
                       static_cast<gfx::BufferFormat>(format)));
  }
  return usage_format_list;
}

std::string BufferUsageAndFormatListToString(
    const BufferUsageAndFormatList& usage_format_list) {
  std::string str;
  for (const auto& entry : usage_format_list) {
    if (!str.empty())
      str += ";";
    str += base::UintToString(static_cast<uint32_t>(entry.first));
    str += ",";
    str += base::UintToString(static_cast<uint32_t>(entry.second));
  }
  return str;
}

}  // namespace viz
