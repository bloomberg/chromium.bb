// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/media_capabilities_database.h"

namespace media {

MediaCapabilitiesDatabase::Entry::Entry(VideoCodecProfile codec_profile,
                                        const gfx::Size& size,
                                        int frame_rate)
    : codec_profile_(codec_profile), size_(size), frame_rate_(frame_rate) {}

MediaCapabilitiesDatabase::Info::Info(uint32_t frames_decoded,
                                      uint32_t frames_dropped)
    : frames_decoded(frames_decoded), frames_dropped(frames_dropped) {}

}  // namespace media