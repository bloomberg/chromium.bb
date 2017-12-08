// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/watch_time_recorder.h"

#include <algorithm>

#include "base/metrics/histogram_functions.h"
#include "base/strings/string_piece.h"
#include "media/base/limits.h"
#include "media/base/watch_time_keys.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace media {

// The minimum amount of media playback which can elapse before we'll report
// watch time metrics for a playback.
constexpr base::TimeDelta kMinimumElapsedWatchTime =
    base::TimeDelta::FromSeconds(limits::kMinimumElapsedWatchTimeSecs);

static bool ShouldReportToUma(WatchTimeKey key) {
  switch (key) {
    // These keys are not currently reported to UMA, but are used for UKM metric
    // calculations. To report them in the future just add the keys to report to
    // the lower list and add histograms.xml entries for them.
    case WatchTimeKey::kVideoAll:
    case WatchTimeKey::kVideoMse:
    case WatchTimeKey::kVideoEme:
    case WatchTimeKey::kVideoSrc:
    case WatchTimeKey::kVideoBattery:
    case WatchTimeKey::kVideoAc:
    case WatchTimeKey::kVideoDisplayFullscreen:
    case WatchTimeKey::kVideoDisplayInline:
    case WatchTimeKey::kVideoDisplayPictureInPicture:
    case WatchTimeKey::kVideoEmbeddedExperience:
    case WatchTimeKey::kVideoNativeControlsOn:
    case WatchTimeKey::kVideoNativeControlsOff:
    case WatchTimeKey::kVideoBackgroundAll:
    case WatchTimeKey::kVideoBackgroundMse:
    case WatchTimeKey::kVideoBackgroundEme:
    case WatchTimeKey::kVideoBackgroundSrc:
    case WatchTimeKey::kVideoBackgroundBattery:
    case WatchTimeKey::kVideoBackgroundAc:
    case WatchTimeKey::kVideoBackgroundEmbeddedExperience:
      return false;

    case WatchTimeKey::kAudioAll:
    case WatchTimeKey::kAudioMse:
    case WatchTimeKey::kAudioEme:
    case WatchTimeKey::kAudioSrc:
    case WatchTimeKey::kAudioBattery:
    case WatchTimeKey::kAudioAc:
    case WatchTimeKey::kAudioEmbeddedExperience:
    case WatchTimeKey::kAudioNativeControlsOn:
    case WatchTimeKey::kAudioNativeControlsOff:
    case WatchTimeKey::kAudioBackgroundAll:
    case WatchTimeKey::kAudioBackgroundMse:
    case WatchTimeKey::kAudioBackgroundEme:
    case WatchTimeKey::kAudioBackgroundSrc:
    case WatchTimeKey::kAudioBackgroundBattery:
    case WatchTimeKey::kAudioBackgroundAc:
    case WatchTimeKey::kAudioBackgroundEmbeddedExperience:
    case WatchTimeKey::kAudioVideoAll:
    case WatchTimeKey::kAudioVideoMse:
    case WatchTimeKey::kAudioVideoEme:
    case WatchTimeKey::kAudioVideoSrc:
    case WatchTimeKey::kAudioVideoBattery:
    case WatchTimeKey::kAudioVideoAc:
    case WatchTimeKey::kAudioVideoDisplayFullscreen:
    case WatchTimeKey::kAudioVideoDisplayInline:
    case WatchTimeKey::kAudioVideoDisplayPictureInPicture:
    case WatchTimeKey::kAudioVideoEmbeddedExperience:
    case WatchTimeKey::kAudioVideoNativeControlsOn:
    case WatchTimeKey::kAudioVideoNativeControlsOff:
    case WatchTimeKey::kAudioVideoBackgroundAll:
    case WatchTimeKey::kAudioVideoBackgroundMse:
    case WatchTimeKey::kAudioVideoBackgroundEme:
    case WatchTimeKey::kAudioVideoBackgroundSrc:
    case WatchTimeKey::kAudioVideoBackgroundBattery:
    case WatchTimeKey::kAudioVideoBackgroundAc:
    case WatchTimeKey::kAudioVideoBackgroundEmbeddedExperience:
      return true;
  }

  NOTREACHED();
  return false;
}

static void RecordWatchTimeInternal(
    base::StringPiece key,
    base::TimeDelta value,
    base::TimeDelta minimum = kMinimumElapsedWatchTime) {
  DCHECK(!key.empty());
  base::UmaHistogramCustomTimes(key.as_string(), value, minimum,
                                base::TimeDelta::FromHours(10), 50);
}

static void RecordMeanTimeBetweenRebuffers(base::StringPiece key,
                                           base::TimeDelta value) {
  DCHECK(!key.empty());

  // There are a maximum of 5 underflow events possible in a given 7s watch time
  // period, so the minimum value is 1.4s.
  RecordWatchTimeInternal(key, value, base::TimeDelta::FromSecondsD(1.4));
}

static void RecordDiscardedWatchTime(base::StringPiece key,
                                     base::TimeDelta value) {
  DCHECK(!key.empty());
  base::UmaHistogramCustomTimes(key.as_string(), value, base::TimeDelta(),
                                kMinimumElapsedWatchTime, 50);
}

static void RecordRebuffersCount(base::StringPiece key, int underflow_count) {
  DCHECK(!key.empty());
  base::UmaHistogramCounts100(key.as_string(), underflow_count);
}

class WatchTimeRecorderProvider : public mojom::WatchTimeRecorderProvider {
 public:
  WatchTimeRecorderProvider() = default;
  ~WatchTimeRecorderProvider() override = default;

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

WatchTimeRecorder::WatchTimeRecorder(mojom::PlaybackPropertiesPtr properties)
    : properties_(std::move(properties)),
      extended_metrics_keys_(
          {{WatchTimeKey::kAudioSrc, kMeanTimeBetweenRebuffersAudioSrc,
            kRebuffersCountAudioSrc, kDiscardedWatchTimeAudioSrc},
           {WatchTimeKey::kAudioMse, kMeanTimeBetweenRebuffersAudioMse,
            kRebuffersCountAudioMse, kDiscardedWatchTimeAudioMse},
           {WatchTimeKey::kAudioEme, kMeanTimeBetweenRebuffersAudioEme,
            kRebuffersCountAudioEme, kDiscardedWatchTimeAudioEme},
           {WatchTimeKey::kAudioVideoSrc,
            kMeanTimeBetweenRebuffersAudioVideoSrc,
            kRebuffersCountAudioVideoSrc, kDiscardedWatchTimeAudioVideoSrc},
           {WatchTimeKey::kAudioVideoMse,
            kMeanTimeBetweenRebuffersAudioVideoMse,
            kRebuffersCountAudioVideoMse, kDiscardedWatchTimeAudioVideoMse},
           {WatchTimeKey::kAudioVideoEme,
            kMeanTimeBetweenRebuffersAudioVideoEme,
            kRebuffersCountAudioVideoEme, kDiscardedWatchTimeAudioVideoEme}}) {}

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

  // Record metrics to be finalized, but do not erase them yet; they are still
  // needed by for UKM and MTBR recording below.
  for (auto& kv : watch_time_info_) {
    if (!should_finalize_everything &&
        std::find(keys_to_finalize.begin(), keys_to_finalize.end(), kv.first) ==
            keys_to_finalize.end()) {
      continue;
    }

    // Report only certain keys to UMA and only if they have at met the minimum
    // watch time requirement. Otherwise, for SRC/MSE/EME keys, log them to the
    // discard metric.
    if (ShouldReportToUma(kv.first)) {
      if (kv.second >= kMinimumElapsedWatchTime) {
        RecordWatchTimeInternal(WatchTimeKeyToString(kv.first), kv.second);
      } else if (kv.second > base::TimeDelta()) {
        auto it = std::find_if(extended_metrics_keys_.begin(),
                               extended_metrics_keys_.end(),
                               [kv](const ExtendedMetricsKeyMap& map) {
                                 return map.watch_time_key == kv.first;
                               });
        if (it != extended_metrics_keys_.end())
          RecordDiscardedWatchTime(it->discard_key, kv.second);
      }
    }

    // At finalize, update the aggregate entry.
    aggregate_watch_time_info_[kv.first] += kv.second;
  }

  // If we're not finalizing everything, we're done after removing keys.
  if (!should_finalize_everything) {
    for (auto key : keys_to_finalize)
      watch_time_info_.erase(key);
    return;
  }

  // This must be done before |underflow_count_| is cleared. Will only be
  // recorded if a Media.WatchTime.*.All entry exists.
  RecordUkmPlaybackData();

  // Check for watch times entries that have corresponding MTBR entries and
  // report the MTBR value using watch_time / |underflow_count|. Do this only
  // for foreground reporters since we only have UMA keys for foreground.
  if (!properties_->is_background) {
    for (auto& mapping : extended_metrics_keys_) {
      auto it = watch_time_info_.find(mapping.watch_time_key);
      if (it == watch_time_info_.end() || it->second < kMinimumElapsedWatchTime)
        continue;

      if (underflow_count_) {
        RecordMeanTimeBetweenRebuffers(mapping.mtbr_key,
                                       it->second / underflow_count_);
      }

      RecordRebuffersCount(mapping.smooth_rate_key, underflow_count_);
    }
  }

  underflow_count_ = 0;
  watch_time_info_.clear();
}

void WatchTimeRecorder::OnError(PipelineStatus status) {
  pipeline_status_ = status;
}

void WatchTimeRecorder::UpdateUnderflowCount(int32_t count) {
  underflow_count_ = count;
}

// static
bool WatchTimeRecorder::ShouldReportUmaForTesting(WatchTimeKey key) {
  return ShouldReportToUma(key);
}

void WatchTimeRecorder::RecordUkmPlaybackData() {
  // UKM may be unavailable in content_shell or other non-chrome/ builds; it
  // may also be unavailable if browser shutdown has started; so this may be a
  // nullptr. If it's unavailable, UKM reporting will be skipped.
  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  if (!ukm_recorder)
    return;

  const int32_t source_id = ukm_recorder->GetNewSourceID();

  // TODO(crbug.com/787209): Stop getting origin from the renderer.
  ukm_recorder->UpdateSourceURL(source_id,
                                properties_->untrusted_top_origin.GetURL());
  ukm::builders::Media_BasicPlayback builder(source_id);

  builder.SetIsTopFrame(properties_->is_top_frame);
  builder.SetIsBackground(properties_->is_background);

  bool recorded_all_metric = false;
  for (auto& kv : aggregate_watch_time_info_) {
    if (kv.first == WatchTimeKey::kAudioAll ||
        kv.first == WatchTimeKey::kAudioBackgroundAll ||
        kv.first == WatchTimeKey::kAudioVideoAll ||
        kv.first == WatchTimeKey::kAudioVideoBackgroundAll ||
        kv.first == WatchTimeKey::kVideoAll ||
        kv.first == WatchTimeKey::kVideoBackgroundAll) {
      // Only one of these keys should be present in a given finalize.
      DCHECK(!recorded_all_metric);
      recorded_all_metric = true;

      builder.SetWatchTime(kv.second.InMilliseconds());
      if (underflow_count_) {
        builder.SetMeanTimeBetweenRebuffers(
            (kv.second / underflow_count_).InMilliseconds());
      }
    } else if (kv.first == WatchTimeKey::kAudioAc ||
               kv.first == WatchTimeKey::kAudioBackgroundAc ||
               kv.first == WatchTimeKey::kAudioVideoAc ||
               kv.first == WatchTimeKey::kAudioVideoBackgroundAc ||
               kv.first == WatchTimeKey::kVideoAc ||
               kv.first == WatchTimeKey::kVideoBackgroundAc) {
      builder.SetWatchTime_AC(kv.second.InMilliseconds());
    } else if (kv.first == WatchTimeKey::kAudioBattery ||
               kv.first == WatchTimeKey::kAudioBackgroundBattery ||
               kv.first == WatchTimeKey::kAudioVideoBattery ||
               kv.first == WatchTimeKey::kAudioVideoBackgroundBattery ||
               kv.first == WatchTimeKey::kVideoBattery ||
               kv.first == WatchTimeKey::kVideoBackgroundBattery) {
      builder.SetWatchTime_Battery(kv.second.InMilliseconds());
    } else if (kv.first == WatchTimeKey::kAudioNativeControlsOn ||
               kv.first == WatchTimeKey::kAudioVideoNativeControlsOn ||
               kv.first == WatchTimeKey::kVideoNativeControlsOn) {
      builder.SetWatchTime_NativeControlsOn(kv.second.InMilliseconds());
    } else if (kv.first == WatchTimeKey::kAudioNativeControlsOff ||
               kv.first == WatchTimeKey::kAudioVideoNativeControlsOff ||
               kv.first == WatchTimeKey::kVideoNativeControlsOff) {
      builder.SetWatchTime_NativeControlsOff(kv.second.InMilliseconds());
    } else if (kv.first == WatchTimeKey::kAudioVideoDisplayFullscreen ||
               kv.first == WatchTimeKey::kVideoDisplayFullscreen) {
      builder.SetWatchTime_DisplayFullscreen(kv.second.InMilliseconds());
    } else if (kv.first == WatchTimeKey::kAudioVideoDisplayInline ||
               kv.first == WatchTimeKey::kVideoDisplayInline) {
      builder.SetWatchTime_DisplayInline(kv.second.InMilliseconds());
    } else if (kv.first == WatchTimeKey::kAudioVideoDisplayPictureInPicture ||
               kv.first == WatchTimeKey::kVideoDisplayPictureInPicture) {
      builder.SetWatchTime_DisplayPictureInPicture(kv.second.InMilliseconds());
    }
  }

  // See note in mojom::PlaybackProperties about why we have both of these.
  builder.SetAudioCodec(properties_->audio_codec);
  builder.SetVideoCodec(properties_->video_codec);
  builder.SetHasAudio(properties_->has_audio);
  builder.SetHasVideo(properties_->has_video);

  builder.SetIsEME(properties_->is_eme);
  builder.SetIsMSE(properties_->is_mse);
  builder.SetLastPipelineStatus(pipeline_status_);
  builder.SetRebuffersCount(underflow_count_);
  builder.SetVideoNaturalWidth(properties_->natural_size.width());
  builder.SetVideoNaturalHeight(properties_->natural_size.height());
  builder.Record(ukm_recorder);
  aggregate_watch_time_info_.clear();
}

WatchTimeRecorder::ExtendedMetricsKeyMap::ExtendedMetricsKeyMap(
    const ExtendedMetricsKeyMap& copy)
    : ExtendedMetricsKeyMap(copy.watch_time_key,
                            copy.mtbr_key,
                            copy.smooth_rate_key,
                            copy.discard_key) {}

WatchTimeRecorder::ExtendedMetricsKeyMap::ExtendedMetricsKeyMap(
    WatchTimeKey watch_time_key,
    base::StringPiece mtbr_key,
    base::StringPiece smooth_rate_key,
    base::StringPiece discard_key)
    : watch_time_key(watch_time_key),
      mtbr_key(mtbr_key),
      smooth_rate_key(smooth_rate_key),
      discard_key(discard_key) {}

}  // namespace media
