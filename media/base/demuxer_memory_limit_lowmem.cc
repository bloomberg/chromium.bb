// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/demuxer_memory_limit.h"

namespace media {

// 2MB: approximately 1 minute of 256Kbps content.
const size_t kDemuxerStreamAudioMemoryLimit = 2 * 1024 * 1024;

// 30MB: approximately 1 minute of 4Mbps content.
const size_t kDemuxerStreamVideoMemoryLimit = 30 * 1024 * 1024;

const size_t kDemuxerMemoryLimit =
    kDemuxerStreamAudioMemoryLimit + kDemuxerStreamVideoMemoryLimit;

}  // namespace media
