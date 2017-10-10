// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/video_decode_perf_history.h"

#include "base/callback.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "media/base/video_codecs.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace media {

VideoDecodeStatsDB* g_database = nullptr;

// static
void VideoDecodePerfHistory::Initialize(VideoDecodeStatsDB* db_instance) {
  DVLOG(2) << __func__;
  g_database = db_instance;
}

// static
void VideoDecodePerfHistory::BindRequest(
    mojom::VideoDecodePerfHistoryRequest request) {
  DVLOG(2) << __func__;

  // Single static instance should serve all requests.
  static VideoDecodePerfHistory* instance = new VideoDecodePerfHistory();

  instance->BindRequestInternal(std::move(request));
}

VideoDecodePerfHistory::VideoDecodePerfHistory() {
  DVLOG(2) << __func__;
}

VideoDecodePerfHistory::~VideoDecodePerfHistory() {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void VideoDecodePerfHistory::BindRequestInternal(
    mojom::VideoDecodePerfHistoryRequest request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bindings_.AddBinding(this, std::move(request));
}

void VideoDecodePerfHistory::OnGotStatsEntry(
    const VideoDecodeStatsDB::VideoDescKey& key,
    GetPerfInfoCallback mojo_cb,
    bool database_success,
    std::unique_ptr<VideoDecodeStatsDB::DecodeStatsEntry> entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!mojo_cb.is_null());

  bool is_power_efficient;
  bool is_smooth;
  double percent_dropped = 0;

  if (entry.get()) {
    DCHECK(database_success);
    percent_dropped =
        static_cast<double>(entry->frames_dropped) / entry->frames_decoded;

    // TODO(chcunningham): add statistics for power efficiency to database.
    is_power_efficient = true;
    is_smooth = percent_dropped <= kMaxSmoothDroppedFramesPercent;
  } else {
    // TODO(chcunningham/mlamouri): Refactor database API to give us nearby
    // entry entry whenever we don't have a perfect match. If higher
    // resolutions/framerates are known to be smooth, we can report this as
    // smooth. If lower resolutions/frames are known to be janky, we can assume
    // this will be janky.

    // No entry? Lets be optimistic.
    is_power_efficient = true;
    is_smooth = true;
  }

  DVLOG(3) << __func__
           << base::StringPrintf(" profile:%s size:%s fps:%d --> ",
                                 GetProfileName(key.codec_profile).c_str(),
                                 key.size.ToString().c_str(), key.frame_rate)
           << (entry.get()
                   ? base::StringPrintf(
                         "smooth:%d frames_decoded:%" PRIu64 " pcnt_dropped:%f",
                         is_smooth, entry->frames_decoded, percent_dropped)
                   : (database_success ? "no info" : "query FAILED"));

  std::move(mojo_cb).Run(is_smooth, is_power_efficient);
}

void VideoDecodePerfHistory::GetPerfInfo(VideoCodecProfile profile,
                                         const gfx::Size& natural_size,
                                         int frame_rate,
                                         GetPerfInfoCallback callback) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK_NE(profile, VIDEO_CODEC_PROFILE_UNKNOWN);
  DCHECK_GT(frame_rate, 0);
  DCHECK(natural_size.width() > 0 && natural_size.height() > 0);

  // TODO(chcunningham): Make this an error condition once the database impl
  // has landed. Hardcoding results for now.
  if (!g_database) {
    DVLOG(2) << __func__ << " No database! Assuming smooth/efficient perf.";
    std::move(callback).Run(true, true);
    return;
  }

  VideoDecodeStatsDB::VideoDescKey key(profile, natural_size, frame_rate);

  // Unretained is safe because this is a leaky singleton.
  g_database->GetDecodeStats(
      key, base::BindOnce(&VideoDecodePerfHistory::OnGotStatsEntry,
                          base::Unretained(this), key, std::move(callback)));
}

}  // namespace media
