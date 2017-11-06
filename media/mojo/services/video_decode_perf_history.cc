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

VideoDecodePerfHistory::VideoDecodePerfHistory(
    std::unique_ptr<VideoDecodeStatsDBFactory> db_factory)
    : db_factory_(std::move(db_factory)),
      db_init_status_(UNINITIALIZED),
      weak_ptr_factory_(this) {
  DVLOG(2) << __func__;
}

VideoDecodePerfHistory::~VideoDecodePerfHistory() {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void VideoDecodePerfHistory::BindRequest(
    mojom::VideoDecodePerfHistoryRequest request) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bindings_.AddBinding(this, std::move(request));
}

void VideoDecodePerfHistory::InitDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (db_init_status_ == PENDING)
    return;

  db_ = db_factory_->CreateDB();
  db_->Initialize(base::BindOnce(&VideoDecodePerfHistory::OnDatabaseInit,
                                 weak_ptr_factory_.GetWeakPtr()));
  db_init_status_ = PENDING;
}

void VideoDecodePerfHistory::OnDatabaseInit(bool success) {
  DVLOG(2) << __func__ << " " << success;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(db_init_status_, PENDING);

  db_init_status_ = success ? COMPLETE : FAILED;

  // Run all the deferred API calls as if they're just now coming in.
  for (auto& deferred_call : init_deferred_api_calls_) {
    std::move(deferred_call).Run();
  }
  init_deferred_api_calls_.clear();
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

  if (db_init_status_ == FAILED) {
    // Optimistically claim perf is both smooth and power efficient.
    std::move(callback).Run(true, true);
    return;
  }

  // Defer this request until the DB is initialized.
  if (db_init_status_ != COMPLETE) {
    init_deferred_api_calls_.push_back(base::BindOnce(
        &VideoDecodePerfHistory::GetPerfInfo, weak_ptr_factory_.GetWeakPtr(),
        profile, natural_size, frame_rate, std::move(callback)));
    InitDatabase();
    return;
  }

  VideoDecodeStatsDB::VideoDescKey video_key(profile, natural_size, frame_rate);

  db_->GetDecodeStats(
      video_key, base::BindOnce(&VideoDecodePerfHistory::OnGotStatsForRequest,
                                weak_ptr_factory_.GetWeakPtr(), video_key,
                                std::move(callback)));
}

void VideoDecodePerfHistory::OnGotStatsForRequest(
    const VideoDecodeStatsDB::VideoDescKey& video_key,
    GetPerfInfoCallback mojo_cb,
    bool database_success,
    std::unique_ptr<VideoDecodeStatsDB::DecodeStatsEntry> stats) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!mojo_cb.is_null());
  DCHECK_EQ(db_init_status_, COMPLETE);

  bool is_power_efficient;
  bool is_smooth;
  double percent_dropped = 0;
  double percent_power_efficient = 0;

  if (stats) {
    DCHECK(database_success);
    percent_dropped =
        static_cast<double>(stats->frames_dropped) / stats->frames_decoded;
    percent_power_efficient =
        static_cast<double>(stats->frames_decoded_power_efficient) /
        stats->frames_decoded;

    is_power_efficient =
        percent_power_efficient >= kMinPowerEfficientDecodedFramePercent;
    is_smooth = percent_dropped <= kMaxSmoothDroppedFramesPercent;
  } else {
    // TODO(chcunningham/mlamouri): Refactor database API to give us nearby
    // stats whenever we don't have a perfect match. If higher
    // resolutions/frame rates are known to be smooth, we can report this as
    /// smooth. If lower resolutions/frames are known to be janky, we can assume
    // this will be janky.

    // No stats? Lets be optimistic.
    is_power_efficient = true;
    is_smooth = true;
  }

  DVLOG(3) << __func__
           << base::StringPrintf(
                  " profile:%s size:%s fps:%d --> ",
                  GetProfileName(video_key.codec_profile).c_str(),
                  video_key.size.ToString().c_str(), video_key.frame_rate)
           << (stats.get()
                   ? base::StringPrintf(
                         "smooth:%d frames_decoded:%" PRIu64 " pcnt_dropped:%f"
                         " pcnt_power_efficent:%f",
                         is_smooth, stats->frames_decoded, percent_dropped,
                         percent_power_efficient)
                   : (database_success ? "no info" : "query FAILED"));

  std::move(mojo_cb).Run(is_smooth, is_power_efficient);
}

void VideoDecodePerfHistory::SavePerfRecord(
    VideoCodecProfile profile,
    const gfx::Size& natural_size,
    int frame_rate,
    uint32_t frames_decoded,
    uint32_t frames_dropped,
    uint32_t frames_decoded_power_efficient) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (db_init_status_ == FAILED) {
    DVLOG(3) << __func__ << " Can't save stats. No DB!";
    return;
  }

  // Defer this request until the DB is initialized.
  if (db_init_status_ != COMPLETE) {
    init_deferred_api_calls_.push_back(base::BindOnce(
        &VideoDecodePerfHistory::SavePerfRecord, weak_ptr_factory_.GetWeakPtr(),
        profile, natural_size, frame_rate, frames_decoded, frames_dropped,
        frames_decoded_power_efficient));
    InitDatabase();
    return;
  }

  VideoDecodeStatsDB::VideoDescKey video_key(profile, natural_size, frame_rate);
  VideoDecodeStatsDB::DecodeStatsEntry new_stats(
      frames_decoded, frames_dropped, frames_decoded_power_efficient);

  // Get past perf info and report UKM metrics before saving this record.
  db_->GetDecodeStats(
      video_key,
      base::BindOnce(&VideoDecodePerfHistory::OnGotStatsForSave,
                     weak_ptr_factory_.GetWeakPtr(), video_key, new_stats));
}

void VideoDecodePerfHistory::OnGotStatsForSave(
    const VideoDecodeStatsDB::VideoDescKey& video_key,
    const VideoDecodeStatsDB::DecodeStatsEntry& new_stats,
    bool success,
    std::unique_ptr<VideoDecodeStatsDB::DecodeStatsEntry> past_stats) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(db_init_status_, COMPLETE);

  if (!success) {
    DVLOG(3) << __func__ << " FAILED! Aborting save.";
    return;
  }

  ReportUkmMetrics(video_key, new_stats, past_stats.get());

  db_->AppendDecodeStats(video_key, new_stats);
}

void VideoDecodePerfHistory::ReportUkmMetrics(
    const VideoDecodeStatsDB::VideoDescKey& video_key,
    const VideoDecodeStatsDB::DecodeStatsEntry& new_stats,
    VideoDecodeStatsDB::DecodeStatsEntry* past_stats) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(chcunningham): Report metrics.
}

}  // namespace media
