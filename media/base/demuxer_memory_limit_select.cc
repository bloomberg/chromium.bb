// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/demuxer_memory_limit.h"

#include "base/sys_info.h"

namespace media {

size_t GetDemuxerStreamAudioMemoryLimit() {
  static size_t limit = base::SysInfo::IsLowEndDevice()
                            ? internal::kDemuxerStreamAudioMemoryLimitLow
                            : internal::kDemuxerStreamAudioMemoryLimitDefault;
  return limit;
}

size_t GetDemuxerStreamVideoMemoryLimit() {
  static size_t limit = base::SysInfo::IsLowEndDevice()
                            ? internal::kDemuxerStreamVideoMemoryLimitLow
                            : internal::kDemuxerStreamVideoMemoryLimitDefault;
  return limit;
}

size_t GetDemuxerMemoryLimit() {
  return GetDemuxerStreamAudioMemoryLimit() +
         GetDemuxerStreamVideoMemoryLimit();
}

}  // namespace media
