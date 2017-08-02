// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_WATCH_TIME_RECORDER_H_
#define MEDIA_MOJO_SERVICES_WATCH_TIME_RECORDER_H_

#include <stdint.h>
#include <string>

#include "base/compiler_specific.h"
#include "base/containers/flat_map.h"
#include "base/time/time.h"
#include "media/base/audio_codecs.h"
#include "media/base/video_codecs.h"
#include "media/mojo/interfaces/watch_time_recorder.mojom.h"
#include "media/mojo/services/media_mojo_export.h"
#include "url/origin.h"

namespace media {

// See mojom::WatchTimeRecorder for documentation.
class MEDIA_MOJO_EXPORT WatchTimeRecorder
    : NON_EXPORTED_BASE(public mojom::WatchTimeRecorder) {
 public:
  explicit WatchTimeRecorder(mojom::PlaybackPropertiesPtr properties);
  ~WatchTimeRecorder() override;

  static void CreateWatchTimeRecorderProvider(
      mojom::WatchTimeRecorderProviderRequest request);

  // mojom::WatchTimeRecorder implementation:
  void RecordWatchTime(WatchTimeKey key, base::TimeDelta watch_time) override;
  void FinalizeWatchTime(
      const std::vector<WatchTimeKey>& watch_time_keys) override;
  void UpdateUnderflowCount(int32_t count) override;

  static const char kWatchTimeUkmEvent[];

 private:
  const mojom::PlaybackPropertiesPtr properties_;

  // Mapping of WatchTime metric keys to MeanTimeBetweenRebuffers (MTBR) and
  // smooth rate (had zero rebuffers) keys.
  struct RebufferMapping {
    RebufferMapping(const RebufferMapping& copy);
    RebufferMapping(WatchTimeKey watch_time_key,
                    base::StringPiece mtbr_key,
                    base::StringPiece smooth_rate_key);
    const WatchTimeKey watch_time_key;
    const base::StringPiece mtbr_key;
    const base::StringPiece smooth_rate_key;
  };
  const std::vector<RebufferMapping> rebuffer_keys_;

  base::flat_map<WatchTimeKey, base::TimeDelta> watch_time_info_;

  int underflow_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(WatchTimeRecorder);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_WATCH_TIME_RECORDER_H_
