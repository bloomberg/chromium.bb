// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/video_decode_stats_db.h"

namespace media {

VideoDecodeStatsDB::VideoDescKey::VideoDescKey(VideoCodecProfile codec_profile,
                                               const gfx::Size& size,
                                               int frame_rate)
    : codec_profile(codec_profile), size(size), frame_rate(frame_rate) {}

VideoDecodeStatsDB::DecodeStatsEntry::DecodeStatsEntry(uint64_t frames_decoded,
                                                       uint64_t frames_dropped)
    : frames_decoded(frames_decoded), frames_dropped(frames_dropped) {}

}  // namespace media
