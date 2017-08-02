// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_WATCH_TIME_KEYS_H_
#define MEDIA_BASE_WATCH_TIME_KEYS_H_

#include "base/containers/flat_set.h"
#include "base/strings/string_piece.h"
#include "media/base/media_export.h"

namespace media {

enum class WatchTimeKey : int {
  kAudioAll = 0,
  kAudioMse,
  kAudioEme,
  kAudioSrc,
  kAudioBattery,
  kAudioAc,
  kAudioEmbeddedExperience,
  kAudioNativeControlsOn,
  kAudioNativeControlsOff,
  kAudioVideoAll,
  kAudioVideoMse,
  kAudioVideoEme,
  kAudioVideoSrc,
  kAudioVideoBattery,
  kAudioVideoAc,
  kAudioVideoDisplayFullscreen,
  kAudioVideoDisplayInline,
  kAudioVideoDisplayPictureInPicture,
  kAudioVideoEmbeddedExperience,
  kAudioVideoNativeControlsOn,
  kAudioVideoNativeControlsOff,
  kAudioVideoBackgroundAll,
  kAudioVideoBackgroundMse,
  kAudioVideoBackgroundEme,
  kAudioVideoBackgroundSrc,
  kAudioVideoBackgroundBattery,
  kAudioVideoBackgroundAc,
  kAudioVideoBackgroundEmbeddedExperience,
  kWatchTimeKeyMax = kAudioVideoBackgroundEmbeddedExperience
};

// Histogram names used for reporting; also double as MediaLog key names.
// NOTE: If you add to this list you must update GetWatchTimeKeys() and if
// necessary, GetWatchTimePowerKeys().
MEDIA_EXPORT extern const char kWatchTimeAudioAll[];
MEDIA_EXPORT extern const char kWatchTimeAudioMse[];
MEDIA_EXPORT extern const char kWatchTimeAudioEme[];
MEDIA_EXPORT extern const char kWatchTimeAudioSrc[];
MEDIA_EXPORT extern const char kWatchTimeAudioBattery[];
MEDIA_EXPORT extern const char kWatchTimeAudioAc[];
MEDIA_EXPORT extern const char kWatchTimeAudioEmbeddedExperience[];
MEDIA_EXPORT extern const char kWatchTimeAudioNativeControlsOn[];
MEDIA_EXPORT extern const char kWatchTimeAudioNativeControlsOff[];
MEDIA_EXPORT extern const char kWatchTimeAudioVideoAll[];
MEDIA_EXPORT extern const char kWatchTimeAudioVideoMse[];
MEDIA_EXPORT extern const char kWatchTimeAudioVideoEme[];
MEDIA_EXPORT extern const char kWatchTimeAudioVideoSrc[];
MEDIA_EXPORT extern const char kWatchTimeAudioVideoBattery[];
MEDIA_EXPORT extern const char kWatchTimeAudioVideoAc[];
MEDIA_EXPORT extern const char kWatchTimeAudioVideoDisplayFullscreen[];
MEDIA_EXPORT extern const char kWatchTimeAudioVideoDisplayInline[];
MEDIA_EXPORT extern const char kWatchTimeAudioVideoDisplayPictureInPicture[];
MEDIA_EXPORT extern const char kWatchTimeAudioVideoEmbeddedExperience[];
MEDIA_EXPORT extern const char kWatchTimeAudioVideoNativeControlsOn[];
MEDIA_EXPORT extern const char kWatchTimeAudioVideoNativeControlsOff[];
MEDIA_EXPORT extern const char kWatchTimeAudioVideoBackgroundAll[];
MEDIA_EXPORT extern const char kWatchTimeAudioVideoBackgroundMse[];
MEDIA_EXPORT extern const char kWatchTimeAudioVideoBackgroundEme[];
MEDIA_EXPORT extern const char kWatchTimeAudioVideoBackgroundSrc[];
MEDIA_EXPORT extern const char kWatchTimeAudioVideoBackgroundBattery[];
MEDIA_EXPORT extern const char kWatchTimeAudioVideoBackgroundAc[];
MEDIA_EXPORT extern const char
    kWatchTimeAudioVideoBackgroundEmbeddedExperience[];
// **** If adding any line above this see the toplevel comment! ****

// Count of the number of underflow events during a media session.
MEDIA_EXPORT extern const char kWatchTimeUnderflowCount[];

// UMA keys for MTBR samples.
MEDIA_EXPORT extern const char kMeanTimeBetweenRebuffersAudioSrc[];
MEDIA_EXPORT extern const char kMeanTimeBetweenRebuffersAudioMse[];
MEDIA_EXPORT extern const char kMeanTimeBetweenRebuffersAudioEme[];
MEDIA_EXPORT extern const char kMeanTimeBetweenRebuffersAudioVideoSrc[];
MEDIA_EXPORT extern const char kMeanTimeBetweenRebuffersAudioVideoMse[];
MEDIA_EXPORT extern const char kMeanTimeBetweenRebuffersAudioVideoEme[];

// Whether there were any rebuffers within a given watch time report.
MEDIA_EXPORT extern const char kRebuffersCountAudioSrc[];
MEDIA_EXPORT extern const char kRebuffersCountAudioMse[];
MEDIA_EXPORT extern const char kRebuffersCountAudioEme[];
MEDIA_EXPORT extern const char kRebuffersCountAudioVideoSrc[];
MEDIA_EXPORT extern const char kRebuffersCountAudioVideoMse[];
MEDIA_EXPORT extern const char kRebuffersCountAudioVideoEme[];

MEDIA_EXPORT base::StringPiece WatchTimeKeyToString(WatchTimeKey key);

}  // namespace media

#endif  // MEDIA_BASE_WATCH_TIME_KEYS_H_
