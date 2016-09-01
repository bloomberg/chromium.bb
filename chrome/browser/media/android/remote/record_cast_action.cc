// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/android/remote/record_cast_action.h"

#include <jni.h>

#include "base/metrics/histogram_macros.h"
#include "content/public/browser/user_metrics.h"
#include "jni/RecordCastAction_jni.h"
#include "media/base/container_names.h"

using base::UserMetricsAction;
using base::android::JavaParamRef;
using content::RecordAction;

namespace {

// When updating these values, remember to also update
// tools/histograms/histograms.xml.
enum CastPlayBackState {
  YT_PLAYER_SUCCESS = 0,
  YT_PLAYER_FAILURE = 1,
  DEFAULT_PLAYER_SUCCESS = 2,
  DEFAULT_PLAYER_FAILURE = 3,
  CAST_PLAYBACK_STATE_COUNT = 4
};

// When updating these values, remember to also update
// tools/histograms/histograms.xml.

// This is actually a misnomer, it should be RemotePlaybackPlayerType, but it is
// more important that it matches the histogram name in histograms.xml.
// TODO(aberent) Change this once we are upstream, when can change it both here
// and in histogram.xml in the same CL.
enum RemotePlaybackDeviceType {
  CAST_GENERIC = 0,
  CAST_YOUTUBE = 1,
  NON_CAST_YOUTUBE = 2,
  REMOTE_PLAYBACK_DEVICE_TYPE_COUNT = 3
};

}  // namespace

namespace remote_media {
static void RecordRemotePlaybackDeviceSelected(JNIEnv*,
                                               const JavaParamRef<jclass>&,
                                               jint device_type) {
  UMA_HISTOGRAM_ENUMERATION(
      "Cast.Sender.DeviceType", device_type, REMOTE_PLAYBACK_DEVICE_TYPE_COUNT);
}

static void RecordCastPlayRequested(JNIEnv*, const JavaParamRef<jclass>&) {
  RecordAction(UserMetricsAction("Cast_Sender_CastPlayRequested"));
}

static void RecordCastDefaultPlayerResult(JNIEnv*,
                                          const JavaParamRef<jclass>&,
                                          jboolean cast_success) {
  if (cast_success) {
    UMA_HISTOGRAM_ENUMERATION("Cast.Sender.CastPlayerResult",
                              DEFAULT_PLAYER_SUCCESS,
                              CAST_PLAYBACK_STATE_COUNT);
  } else {
    UMA_HISTOGRAM_ENUMERATION("Cast.Sender.CastPlayerResult",
                              DEFAULT_PLAYER_FAILURE,
                              CAST_PLAYBACK_STATE_COUNT);
  }
}

static void RecordCastYouTubePlayerResult(JNIEnv*,
                                          const JavaParamRef<jclass>&,
                                          jboolean cast_success) {
  if (cast_success) {
    UMA_HISTOGRAM_ENUMERATION("Cast.Sender.CastPlayerResult", YT_PLAYER_SUCCESS,
                              CAST_PLAYBACK_STATE_COUNT);
  } else {
    UMA_HISTOGRAM_ENUMERATION("Cast.Sender.CastPlayerResult", YT_PLAYER_FAILURE,
                              CAST_PLAYBACK_STATE_COUNT);
  }
}

static void RecordCastMediaType(JNIEnv*,
                                const JavaParamRef<jclass>&,
                                jint media_type) {
  UMA_HISTOGRAM_ENUMERATION("Cast.Sender.CastMediaType", media_type,
      media::container_names::CONTAINER_MAX);
}

static void RecordCastEndedTimeRemaining(JNIEnv*,
                                         const JavaParamRef<jclass>&,
                                         jint video_total_time,
                                         jint time_left_in_video) {
  int percent_remaining = 100;
  if (video_total_time > 0) {
    // Get the percentage of video remaining, but bucketize into groups of 10
    // since we don't really need that granular of data.
    percent_remaining = static_cast<int>(
        10.0 * time_left_in_video / video_total_time) * 10;
  }

  UMA_HISTOGRAM_ENUMERATION("Cast.Sender.CastTimeRemainingPercentage",
      percent_remaining, 101);
}

// Register native methods
bool RegisterRecordCastAction(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace remote_media
