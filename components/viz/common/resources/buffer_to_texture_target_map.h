// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_RESOURCES_BUFFER_TO_TEXTURE_TARGET_MAP_H_
#define COMPONENTS_VIZ_COMMON_RESOURCES_BUFFER_TO_TEXTURE_TARGET_MAP_H_

#include <map>
#include <string>
#include <vector>

#include "components/viz/common/viz_common_export.h"
#include "ui/gfx/buffer_types.h"

namespace viz {
// A pair containing GPU Memory Buffer usage and format.
using BufferUsageAndFormat = std::pair<gfx::BufferUsage, gfx::BufferFormat>;
using BufferUsageAndFormatList = std::vector<BufferUsageAndFormat>;

// Converts a serialized list of BufferUsageAndFormat back to the runtime
// format. Serialization takes the form:
//   "usage,format;usage,format;...;usage,format"
VIZ_COMMON_EXPORT BufferUsageAndFormatList
StringToBufferUsageAndFormatList(const std::string& str);

// Converts a vector of BufferUsageAndFormat to a string representation of the
// format:
//   "usage,format;usage,format;...;usage,format"
VIZ_COMMON_EXPORT std::string BufferUsageAndFormatListToString(
    const BufferUsageAndFormatList& usage_and_format);

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_RESOURCES_BUFFER_TO_TEXTURE_TARGET_MAP_H_
