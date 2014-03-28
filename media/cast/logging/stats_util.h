// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_LOGGING_STATS_UTIL_H_
#define MEDIA_CAST_LOGGING_STATS_UTIL_H_

#include "base/memory/scoped_ptr.h"
#include "media/cast/logging/logging_defines.h"

namespace base {
class DictionaryValue;
}

namespace media {
namespace cast {

// Converts stats provided in |frame_stats_map| and |packet_stats_map| to
// base::DictionaryValue format. See .cc file for the exact structure.
scoped_ptr<base::DictionaryValue> ConvertStats(
    const FrameStatsMap& frame_stats_map,
    const PacketStatsMap& packet_stats_map);

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_LOGGING_STATS_UTIL_H_
