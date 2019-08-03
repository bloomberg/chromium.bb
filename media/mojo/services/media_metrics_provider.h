// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MEDIA_METRICS_PROVIDER_H_
#define MEDIA_MOJO_SERVICES_MEDIA_METRICS_PROVIDER_H_

#include <stdint.h>
#include <string>

#include "media/base/container_names.h"
#include "media/base/pipeline_status.h"
#include "media/base/timestamp_constants.h"
#include "media/learning/common/value.h"
#include "media/mojo/mojom/media_metrics_provider.mojom.h"
#include "media/mojo/services/media_mojo_export.h"
#include "media/mojo/services/video_decode_perf_history.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace media {

class VideoDecodePerfHistory;

// See mojom::MediaMetricsProvider for documentation.
class MEDIA_MOJO_EXPORT MediaMetricsProvider
    : public mojom::MediaMetricsProvider {
 public:
  enum class BrowsingMode : bool { kIncognito, kNormal };

  enum class FrameStatus : bool { kTopFrame, kNotTopFrame };

  MediaMetricsProvider(BrowsingMode is_incognito,
                       FrameStatus is_top_frame,
                       ukm::SourceId source_id,
                       learning::FeatureValue origin,
                       VideoDecodePerfHistory::SaveCallback save_cb);
  ~MediaMetricsProvider() override;

  // Callback for retrieving a ukm::SourceId.
  using GetSourceIdCallback = base::RepeatingCallback<ukm::SourceId(void)>;

  // TODO(liberato): This should be from a FeatureProvider, but the way in which
  // we attach LearningHelper more or less precludes it.  Per-frame task
  // controllers would make this easy, but we bypass that here.
  using GetOriginCallback =
      base::RepeatingCallback<learning::FeatureValue(void)>;

  // Creates a MediaMetricsProvider, |perf_history| may be nullptr if perf
  // history database recording is disabled.
  //
  // |get_source_id_cb| and |get_origin_cb| may not be run after this function
  // returns.  The intention is that they'll be run to produce the constructor
  // arguments for MediaMetricsProvider synchronously.  They should not be
  // copied or moved for later.
  static void Create(BrowsingMode is_incognito,
                     FrameStatus is_top_frame,
                     GetSourceIdCallback get_source_id_cb,
                     GetOriginCallback get_origin_cb,
                     VideoDecodePerfHistory::SaveCallback save_cb,
                     mojom::MediaMetricsProviderRequest request);

 private:
  struct PipelineInfo {
    PipelineInfo(bool is_incognito);
    ~PipelineInfo();
    bool is_incognito;
    bool has_ever_played = false;
    bool has_reached_have_enough = false;
    bool has_audio = false;
    bool has_video = false;
    bool is_eme = false;
    bool video_decoder_changed = false;
    AudioCodec audio_codec;
    VideoCodec video_codec;
    PipelineDecoderInfo video_pipeline_info;
    PipelineDecoderInfo audio_pipeline_info;
    PipelineStatus last_pipeline_status = PIPELINE_OK;
  };

  // mojom::MediaMetricsProvider implementation:
  void Initialize(bool is_mse, mojom::MediaURLScheme url_scheme) override;
  void OnError(PipelineStatus status) override;
  void SetAudioPipelineInfo(const PipelineDecoderInfo& info) override;
  void SetContainerName(
      container_names::MediaContainerName container_name) override;
  void SetHasAudio(AudioCodec audio_codec) override;
  void SetHasPlayed() override;
  void SetHasVideo(VideoCodec video_codec) override;
  void SetHaveEnough() override;
  void SetIsAdMedia() override;
  void SetIsEME() override;
  void SetTimeToMetadata(base::TimeDelta elapsed) override;
  void SetTimeToFirstFrame(base::TimeDelta elapsed) override;
  void SetTimeToPlayReady(base::TimeDelta elapsed) override;
  void SetVideoPipelineInfo(const PipelineDecoderInfo& info) override;

  void AcquireWatchTimeRecorder(
      mojom::PlaybackPropertiesPtr properties,
      mojom::WatchTimeRecorderRequest request) override;
  void AcquireVideoDecodeStatsRecorder(
      mojom::VideoDecodeStatsRecorderRequest request) override;
  void AddBytesReceived(uint64_t bytes_received) override;

  void ReportPipelineUMA();
  std::string GetUMANameForAVStream(const PipelineInfo& player_info);

  // Session unique ID which maps to a given WebMediaPlayerImpl instances. Used
  // to coordinate multiply logged events with a singly logged metric.
  const uint64_t player_id_;

  // Are UKM reports for the main frame or for a subframe?
  const bool is_top_frame_;

  const ukm::SourceId source_id_;
  const learning::FeatureValue origin_;

  const VideoDecodePerfHistory::SaveCallback save_cb_;

  // UMA pipeline packaged data
  PipelineInfo uma_info_;

  // These values are not always sent but have known defaults.
  bool is_ad_media_ = false;

  // The values below are only set if |initialized_| is true.
  bool initialized_ = false;
  bool is_mse_;
  mojom::MediaURLScheme url_scheme_;

  // Total number of bytes received by the media player so far.
  uint64_t total_bytes_received_ = 0;

  base::TimeDelta time_to_metadata_ = kNoTimestamp;
  base::TimeDelta time_to_first_frame_ = kNoTimestamp;
  base::TimeDelta time_to_play_ready_ = kNoTimestamp;

  base::Optional<container_names::MediaContainerName> container_name_;

  DISALLOW_COPY_AND_ASSIGN(MediaMetricsProvider);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MEDIA_METRICS_PROVIDER_H_
