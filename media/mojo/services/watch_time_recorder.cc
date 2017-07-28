// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/watch_time_recorder.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/string_piece.h"
#include "media/base/watch_time_keys.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/metrics/public/cpp/ukm_entry_builder.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace media {

static void RecordWatchTimeInternal(base::StringPiece key,
                                    base::TimeDelta value,
                                    bool is_mtbr = false) {
  base::UmaHistogramCustomTimes(
      key.as_string(), value,
      // There are a maximum of 5 underflow events possible in a given 7s
      // watch time period, so the minimum value is 1.4s.
      is_mtbr ? base::TimeDelta::FromSecondsD(1.4)
              : base::TimeDelta::FromSeconds(7),
      base::TimeDelta::FromHours(10), 50);
}

static void RecordMeanTimeBetweenRebuffers(base::StringPiece key,
                                           base::TimeDelta value) {
  RecordWatchTimeInternal(key, value, true);
}

static void RecordRebuffersCount(base::StringPiece key, int underflow_count) {
  base::UmaHistogramCounts100(key.as_string(), underflow_count);
}

static bool ShouldReportUkmWatchTime(base::StringPiece key) {
  // EmbeddedExperience is always a file:// URL, so skip reporting.
  return !key.ends_with("EmbeddedExperience");
}

class WatchTimeRecorderProvider : public mojom::WatchTimeRecorderProvider {
 public:
  WatchTimeRecorderProvider() {}
  ~WatchTimeRecorderProvider() override {}

 private:
  // mojom::WatchTimeRecorderProvider implementation:
  void AcquireWatchTimeRecorder(
      mojom::PlaybackPropertiesPtr properties,
      mojom::WatchTimeRecorderRequest request) override {
    mojo::MakeStrongBinding(
        base::MakeUnique<WatchTimeRecorder>(std::move(properties)),
        std::move(request));
  }

  DISALLOW_COPY_AND_ASSIGN(WatchTimeRecorderProvider);
};

const char WatchTimeRecorder::kWatchTimeUkmEvent[] = "Media.WatchTime";

WatchTimeRecorder::WatchTimeRecorder(mojom::PlaybackPropertiesPtr properties)
    : properties_(std::move(properties)),
      rebuffer_keys_(
          {{WatchTimeKey::kAudioSrc, kMeanTimeBetweenRebuffersAudioSrc,
            kRebuffersCountAudioSrc},
           {WatchTimeKey::kAudioMse, kMeanTimeBetweenRebuffersAudioMse,
            kRebuffersCountAudioMse},
           {WatchTimeKey::kAudioEme, kMeanTimeBetweenRebuffersAudioEme,
            kRebuffersCountAudioEme},
           {WatchTimeKey::kAudioVideoSrc,
            kMeanTimeBetweenRebuffersAudioVideoSrc,
            kRebuffersCountAudioVideoSrc},
           {WatchTimeKey::kAudioVideoMse,
            kMeanTimeBetweenRebuffersAudioVideoMse,
            kRebuffersCountAudioVideoMse},
           {WatchTimeKey::kAudioVideoEme,
            kMeanTimeBetweenRebuffersAudioVideoEme,
            kRebuffersCountAudioVideoEme}}) {}

WatchTimeRecorder::~WatchTimeRecorder() {
  FinalizeWatchTime({});
}

// static
void WatchTimeRecorder::CreateWatchTimeRecorderProvider(
    mojom::WatchTimeRecorderProviderRequest request) {
  mojo::MakeStrongBinding(base::MakeUnique<WatchTimeRecorderProvider>(),
                          std::move(request));
}

void WatchTimeRecorder::RecordWatchTime(WatchTimeKey key,
                                        base::TimeDelta watch_time) {
  watch_time_info_[key] = watch_time;
}

void WatchTimeRecorder::FinalizeWatchTime(
    const std::vector<WatchTimeKey>& keys_to_finalize) {
  // If the filter set is empty, treat that as finalizing all keys; otherwise
  // only the listed keys will be finalized.
  const bool should_finalize_everything = keys_to_finalize.empty();

  // Only report underflow count when finalizing everything.
  if (should_finalize_everything) {
    // Check for watch times entries that have corresponding MTBR entries and
    // report the MTBR value using watch_time / |underflow_count|.
    for (auto& mapping : rebuffer_keys_) {
      auto it = watch_time_info_.find(mapping.watch_time_key);
      if (it == watch_time_info_.end())
        continue;

      if (underflow_count_) {
        RecordMeanTimeBetweenRebuffers(mapping.mtbr_key,
                                       it->second / underflow_count_);
      }

      RecordRebuffersCount(mapping.smooth_rate_key, underflow_count_);
    }

    underflow_count_ = 0;
  }

  // UKM may be unavailable in content_shell or other non-chrome/ builds; it
  // may also be unavailable if browser shutdown has started; so this may be a
  // nullptr. If it's unavailable, UKM reporting will be skipped.
  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  std::unique_ptr<ukm::UkmEntryBuilder> builder;

  for (auto it = watch_time_info_.begin(); it != watch_time_info_.end();) {
    if (!should_finalize_everything &&
        std::find(keys_to_finalize.begin(), keys_to_finalize.end(),
                  it->first) == keys_to_finalize.end()) {
      ++it;
      continue;
    }

    const base::StringPiece metric_name = WatchTimeKeyToString(it->first);
    RecordWatchTimeInternal(metric_name, it->second);

    if (ukm_recorder && ShouldReportUkmWatchTime(metric_name)) {
      if (!builder) {
        const int32_t source_id = ukm_recorder->GetNewSourceID();
        ukm_recorder->UpdateSourceURL(source_id, properties_->origin.GetURL());
        builder = ukm_recorder->GetEntryBuilder(source_id, kWatchTimeUkmEvent);
      }

      // Strip Media.WatchTime. prefix for UKM since they're already grouped;
      // arraysize() includes \0, so no +1 necessary for trailing period.
      builder->AddMetric(
          metric_name.substr(arraysize(kWatchTimeUkmEvent)).data(),
          it->second.InMilliseconds());
    }

    it = watch_time_info_.erase(it);
  }
}

void WatchTimeRecorder::UpdateUnderflowCount(int32_t count) {
  underflow_count_ = count;
}

WatchTimeRecorder::RebufferMapping::RebufferMapping(const RebufferMapping& copy)
    : RebufferMapping(copy.watch_time_key,
                      copy.mtbr_key,
                      copy.smooth_rate_key) {}

WatchTimeRecorder::RebufferMapping::RebufferMapping(
    WatchTimeKey watch_time_key,
    base::StringPiece mtbr_key,
    base::StringPiece smooth_rate_key)
    : watch_time_key(watch_time_key),
      mtbr_key(mtbr_key),
      smooth_rate_key(smooth_rate_key) {}

}  // namespace media
