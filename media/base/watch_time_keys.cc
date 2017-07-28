// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/watch_time_keys.h"

namespace media {

// Audio+video watch time metrics.
const char kWatchTimeAudioVideoAll[] = "Media.WatchTime.AudioVideo.All";
const char kWatchTimeAudioVideoMse[] = "Media.WatchTime.AudioVideo.MSE";
const char kWatchTimeAudioVideoEme[] = "Media.WatchTime.AudioVideo.EME";
const char kWatchTimeAudioVideoSrc[] = "Media.WatchTime.AudioVideo.SRC";
const char kWatchTimeAudioVideoBattery[] = "Media.WatchTime.AudioVideo.Battery";
const char kWatchTimeAudioVideoAc[] = "Media.WatchTime.AudioVideo.AC";
const char kWatchTimeAudioVideoDisplayFullscreen[] =
    "Media.WatchTime.AudioVideo.DisplayFullscreen";
const char kWatchTimeAudioVideoDisplayInline[] =
    "Media.WatchTime.AudioVideo.DisplayInline";
const char kWatchTimeAudioVideoDisplayPictureInPicture[] =
    "Media.WatchTime.AudioVideo.DisplayPictureInPicture";
const char kWatchTimeAudioVideoEmbeddedExperience[] =
    "Media.WatchTime.AudioVideo.EmbeddedExperience";
const char kWatchTimeAudioVideoNativeControlsOn[] =
    "Media.WatchTime.AudioVideo.NativeControlsOn";
const char kWatchTimeAudioVideoNativeControlsOff[] =
    "Media.WatchTime.AudioVideo.NativeControlsOff";

// Audio only "watch time" metrics.
const char kWatchTimeAudioAll[] = "Media.WatchTime.Audio.All";
const char kWatchTimeAudioMse[] = "Media.WatchTime.Audio.MSE";
const char kWatchTimeAudioEme[] = "Media.WatchTime.Audio.EME";
const char kWatchTimeAudioSrc[] = "Media.WatchTime.Audio.SRC";
const char kWatchTimeAudioBattery[] = "Media.WatchTime.Audio.Battery";
const char kWatchTimeAudioAc[] = "Media.WatchTime.Audio.AC";
const char kWatchTimeAudioDisplayFullscreen[] =
    "Media.WatchTime.Audio.DisplayFullscreen";
const char kWatchTimeAudioDisplayInline[] =
    "Media.WatchTime.Audio.DisplayInline";
const char kWatchTimeAudioDisplayPictureInPicture[] =
    "Media.WatchTime.Audio.DisplayPictureInPicture";
const char kWatchTimeAudioEmbeddedExperience[] =
    "Media.WatchTime.Audio.EmbeddedExperience";
const char kWatchTimeAudioNativeControlsOn[] =
    "Media.WatchTime.Audio.NativeControlsOn";
const char kWatchTimeAudioNativeControlsOff[] =
    "Media.WatchTime.Audio.NativeControlsOff";

// Audio+video background watch time metrics.
const char kWatchTimeAudioVideoBackgroundAll[] =
    "Media.WatchTime.AudioVideo.Background.All";
const char kWatchTimeAudioVideoBackgroundMse[] =
    "Media.WatchTime.AudioVideo.Background.MSE";
const char kWatchTimeAudioVideoBackgroundEme[] =
    "Media.WatchTime.AudioVideo.Background.EME";
const char kWatchTimeAudioVideoBackgroundSrc[] =
    "Media.WatchTime.AudioVideo.Background.SRC";
const char kWatchTimeAudioVideoBackgroundBattery[] =
    "Media.WatchTime.AudioVideo.Background.Battery";
const char kWatchTimeAudioVideoBackgroundAc[] =
    "Media.WatchTime.AudioVideo.Background.AC";
const char kWatchTimeAudioVideoBackgroundEmbeddedExperience[] =
    "Media.WatchTime.AudioVideo.Background.EmbeddedExperience";

const char kWatchTimeFinalize[] = "FinalizeWatchTime";
const char kWatchTimeFinalizePower[] = "FinalizePowerWatchTime";
const char kWatchTimeFinalizeControls[] = "FinalizeControlsWatchTime";
const char kWatchTimeFinalizeDisplay[] = "FinalizeDisplayWatchTime";

const char kWatchTimeUnderflowCount[] = "UnderflowCount";

const char kMeanTimeBetweenRebuffersAudioSrc[] =
    "Media.MeanTimeBetweenRebuffers.Audio.SRC";
const char kMeanTimeBetweenRebuffersAudioMse[] =
    "Media.MeanTimeBetweenRebuffers.Audio.MSE";
const char kMeanTimeBetweenRebuffersAudioEme[] =
    "Media.MeanTimeBetweenRebuffers.Audio.EME";
const char kMeanTimeBetweenRebuffersAudioVideoSrc[] =
    "Media.MeanTimeBetweenRebuffers.AudioVideo.SRC";
const char kMeanTimeBetweenRebuffersAudioVideoMse[] =
    "Media.MeanTimeBetweenRebuffers.AudioVideo.MSE";
const char kMeanTimeBetweenRebuffersAudioVideoEme[] =
    "Media.MeanTimeBetweenRebuffers.AudioVideo.EME";

const char kRebuffersCountAudioSrc[] = "Media.RebuffersCount.Audio.SRC";
const char kRebuffersCountAudioMse[] = "Media.RebuffersCount.Audio.MSE";
const char kRebuffersCountAudioEme[] = "Media.RebuffersCount.Audio.EME";
const char kRebuffersCountAudioVideoSrc[] =
    "Media.RebuffersCount.AudioVideo.SRC";
const char kRebuffersCountAudioVideoMse[] =
    "Media.RebuffersCount.AudioVideo.MSE";
const char kRebuffersCountAudioVideoEme[] =
    "Media.RebuffersCount.AudioVideo.EME";

base::flat_set<base::StringPiece> GetWatchTimeKeys() {
  return base::flat_set<base::StringPiece>(
      {
          kWatchTimeAudioAll,
          kWatchTimeAudioMse,
          kWatchTimeAudioEme,
          kWatchTimeAudioSrc,
          kWatchTimeAudioBattery,
          kWatchTimeAudioAc,
          kWatchTimeAudioEmbeddedExperience,
          kWatchTimeAudioDisplayFullscreen,
          kWatchTimeAudioDisplayInline,
          kWatchTimeAudioDisplayPictureInPicture,
          kWatchTimeAudioNativeControlsOn,
          kWatchTimeAudioNativeControlsOff,
          kWatchTimeAudioVideoAll,
          kWatchTimeAudioVideoMse,
          kWatchTimeAudioVideoEme,
          kWatchTimeAudioVideoSrc,
          kWatchTimeAudioVideoBattery,
          kWatchTimeAudioVideoAc,
          kWatchTimeAudioVideoDisplayFullscreen,
          kWatchTimeAudioVideoDisplayInline,
          kWatchTimeAudioVideoDisplayPictureInPicture,
          kWatchTimeAudioVideoEmbeddedExperience,
          kWatchTimeAudioVideoNativeControlsOn,
          kWatchTimeAudioVideoNativeControlsOff,
          kWatchTimeAudioVideoBackgroundAll,
          kWatchTimeAudioVideoBackgroundMse,
          kWatchTimeAudioVideoBackgroundEme,
          kWatchTimeAudioVideoBackgroundSrc,
          kWatchTimeAudioVideoBackgroundBattery,
          kWatchTimeAudioVideoBackgroundAc,
          kWatchTimeAudioVideoBackgroundEmbeddedExperience,
      },
      base::KEEP_FIRST_OF_DUPES);
}

base::flat_set<base::StringPiece> GetWatchTimePowerKeys() {
  return base::flat_set<base::StringPiece>(
      {kWatchTimeAudioBattery, kWatchTimeAudioAc, kWatchTimeAudioVideoBattery,
       kWatchTimeAudioVideoAc, kWatchTimeAudioVideoBackgroundBattery,
       kWatchTimeAudioVideoBackgroundAc},
      base::KEEP_FIRST_OF_DUPES);
}

base::flat_set<base::StringPiece> GetWatchTimeControlsKeys() {
  return base::flat_set<base::StringPiece>(
      {kWatchTimeAudioNativeControlsOn, kWatchTimeAudioNativeControlsOff,
       kWatchTimeAudioVideoNativeControlsOn,
       kWatchTimeAudioVideoNativeControlsOff},
      base::KEEP_FIRST_OF_DUPES);
}

base::flat_set<base::StringPiece> GetWatchTimeDisplayKeys() {
  return base::flat_set<base::StringPiece>(
      {kWatchTimeAudioDisplayFullscreen, kWatchTimeAudioDisplayInline,
       kWatchTimeAudioDisplayPictureInPicture,
       kWatchTimeAudioVideoDisplayFullscreen, kWatchTimeAudioVideoDisplayInline,
       kWatchTimeAudioVideoDisplayPictureInPicture},
      base::KEEP_FIRST_OF_DUPES);
}

}  // namespace media
