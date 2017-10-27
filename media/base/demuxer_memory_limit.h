// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_DEMUXER_MEMORY_LIMIT_H_
#define MEDIA_BASE_DEMUXER_MEMORY_LIMIT_H_

#include <stddef.h>

#include "media/base/media_export.h"

namespace media {

// The maximum amount of data (in bytes) a demuxer can keep in memory, for a
// particular type of stream.
extern const size_t kDemuxerStreamAudioMemoryLimit;
extern const size_t kDemuxerStreamVideoMemoryLimit;

// The maximum amount of data (in bytes) a demuxer can keep in memory overall.
extern const size_t kDemuxerMemoryLimit;

}  // namespace media

#endif  // MEDIA_BASE_DEMUXER_MEMORY_LIMIT_H_
