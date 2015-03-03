// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/metrics/cast_metrics_helper.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/metrics/histogram.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chromecast/base/metrics/cast_histograms.h"
#include "chromecast/base/metrics/grouped_histogram.h"

namespace chromecast {
namespace metrics {

// A useful macro to make sure current member function runs on the valid thread.
#define MAKE_SURE_THREAD(callback, ...)                    \
  if (!message_loop_proxy_->BelongsToCurrentThread()) {    \
    message_loop_proxy_->PostTask(FROM_HERE, base::Bind(   \
        &CastMetricsHelper::callback,                      \
        base::Unretained(this), ##__VA_ARGS__));           \
    return;                                                \
  }

namespace {

CastMetricsHelper* g_instance = NULL;

// Displayed frames are logged in frames per second (but sampling can be over
// a longer period of time, e.g. 5 seconds).
const int kDisplayedFramesPerSecondPeriod = 1000000;

// Sample every 5 seconds, represented in microseconds.
const int kNominalVideoSamplePeriod = 5000000;

const char kMetricsNameAppInfoDelimiter = '#';

}  // namespace

// static

// NOTE(gfhuang): This is a hacky way to encode/decode app infos into a
// string. Mainly because it's hard to add another metrics serialization type
// into components/metrics/serialization/.
// static
bool CastMetricsHelper::DecodeAppInfoFromMetricsName(
    const std::string& metrics_name,
    std::string* action_name,
    std::string* app_id,
    std::string* session_id,
    std::string* sdk_version) {
  DCHECK(action_name);
  DCHECK(app_id);
  DCHECK(session_id);
  DCHECK(sdk_version);
  if (metrics_name.find(kMetricsNameAppInfoDelimiter) == std::string::npos)
    return false;

  std::vector<std::string> tokens;
  base::SplitString(metrics_name, kMetricsNameAppInfoDelimiter, &tokens);
  DCHECK_EQ(tokens.size(), 4u);
  // The order of tokens should match EncodeAppInfoIntoMetricsName().
  *action_name = tokens[0];
  *app_id = tokens[1];
  *session_id = tokens[2];
  *sdk_version = tokens[3];
  return true;
}

// static
std::string CastMetricsHelper::EncodeAppInfoIntoMetricsName(
    const std::string& action_name,
    const std::string& app_id,
    const std::string& session_id,
    const std::string& sdk_version) {
  std::vector<std::string> parts;
  parts.push_back(action_name);
  parts.push_back(app_id);
  parts.push_back(session_id);
  parts.push_back(sdk_version);
  return JoinString(parts, kMetricsNameAppInfoDelimiter);
}

// static
CastMetricsHelper* CastMetricsHelper::GetInstance() {
  DCHECK(g_instance);
  return g_instance;
}

CastMetricsHelper::CastMetricsHelper(
    scoped_refptr<base::MessageLoopProxy> message_loop_proxy)
    : message_loop_proxy_(message_loop_proxy),
      metrics_sink_(NULL),
      record_action_callback_(base::Bind(&base::RecordComputedAction)) {
  DCHECK(message_loop_proxy_.get());
  DCHECK(!g_instance);
  g_instance = this;
}

CastMetricsHelper::CastMetricsHelper()
    : metrics_sink_(NULL) {
  DCHECK(!g_instance);
  g_instance = this;
}

CastMetricsHelper::~CastMetricsHelper() {
  DCHECK_EQ(g_instance, this);
  g_instance = NULL;
}

void CastMetricsHelper::UpdateCurrentAppInfo(const std::string& app_id,
                                             const std::string& session_id) {
  MAKE_SURE_THREAD(UpdateCurrentAppInfo, app_id, session_id);
  app_id_ = app_id;
  session_id_ = session_id;
  app_start_time_ = base::TimeTicks::Now();
  new_startup_time_ = true;
  TagAppStartForGroupedHistograms(app_id_);
  sdk_version_.clear();
}

void CastMetricsHelper::UpdateSDKInfo(const std::string& sdk_version) {
  MAKE_SURE_THREAD(UpdateSDKInfo, sdk_version);
  sdk_version_ = sdk_version;
}

void CastMetricsHelper::LogMediaPlay() {
  MAKE_SURE_THREAD(LogMediaPlay);
  RecordSimpleAction(EncodeAppInfoIntoMetricsName(
      "MediaPlay",
      app_id_,
      session_id_,
      sdk_version_));
}

void CastMetricsHelper::LogMediaPause() {
  MAKE_SURE_THREAD(LogMediaPause);
  RecordSimpleAction(EncodeAppInfoIntoMetricsName(
      "MediaPause",
      app_id_,
      session_id_,
      sdk_version_));
}

void CastMetricsHelper::LogTimeToFirstPaint() {
  MAKE_SURE_THREAD(LogTimeToFirstPaint);
  if (app_id_.empty())
    return;
  base::TimeDelta launch_time = base::TimeTicks::Now() - app_start_time_;
  const std::string uma_name(GetMetricsNameWithAppName("Startup",
                                                       "TimeToFirstPaint"));
  LogMediumTimeHistogramEvent(uma_name, launch_time);
  LOG(INFO) << uma_name << " is " << launch_time.InSecondsF() << " seconds.";
}

void CastMetricsHelper::LogTimeToDisplayVideo() {
  if (!new_startup_time_) {  // For faster check.
    return;
  }
  MAKE_SURE_THREAD(LogTimeToDisplayVideo);
  new_startup_time_ = false;
  base::TimeDelta launch_time = base::TimeTicks::Now() - app_start_time_;
  const std::string uma_name(GetMetricsNameWithAppName("Startup",
                                                       "TimeToDisplayVideo"));
  LogMediumTimeHistogramEvent(uma_name, launch_time);
  LOG(INFO) << uma_name << " is " << launch_time.InSecondsF() << " seconds.";
}

void CastMetricsHelper::LogTimeToBufferAv(BufferingType buffering_type,
                                          base::TimeDelta time) {
  MAKE_SURE_THREAD(LogTimeToBufferAv, buffering_type, time);
  if (time < base::TimeDelta::FromSeconds(0)) {
    LOG(WARNING) << "Negative time";
    return;
  }

  const std::string uma_name(GetMetricsNameWithAppName(
      "Media",
      (buffering_type == kInitialBuffering ? "TimeToBufferAv" :
       buffering_type == kBufferingAfterUnderrun ?
           "TimeToBufferAvAfterUnderrun" :
       buffering_type == kAbortedBuffering ? "TimeToBufferAvAfterAbort" : "")));

  // Histogram from 250ms to 30s with 50 buckets.
  // The ratio between 2 consecutive buckets is:
  // exp( (ln(30000) - ln(250)) / 50 ) = 1.1
  LogTimeHistogramEvent(
      uma_name,
      time,
      base::TimeDelta::FromMilliseconds(250),
      base::TimeDelta::FromMilliseconds(30000),
      50);
}

void CastMetricsHelper::ResetVideoFrameSampling() {
  MAKE_SURE_THREAD(ResetVideoFrameSampling);
  previous_video_stat_sample_time_ = base::TimeTicks();
}

void CastMetricsHelper::LogFramesPer5Seconds(int displayed_frames,
                                             int dropped_frames,
                                             int delayed_frames,
                                             int error_frames) {
  MAKE_SURE_THREAD(LogFramesPer5Seconds, displayed_frames, dropped_frames,
                   delayed_frames, error_frames);
  base::TimeTicks sample_time = base::TimeTicks::Now();

  if (!previous_video_stat_sample_time_.is_null()) {
    base::TimeDelta time_diff = sample_time - previous_video_stat_sample_time_;
    int value = 0;
    const int64 rounding = time_diff.InMicroseconds() / 2;

    if (displayed_frames >= 0) {
      value = (displayed_frames * kDisplayedFramesPerSecondPeriod + rounding) /
          time_diff.InMicroseconds();
      LogEnumerationHistogramEvent(
          GetMetricsNameWithAppName("Media", "DisplayedFramesPerSecond"),
          value, 50);
    }
    if (delayed_frames >= 0) {
      value = (delayed_frames * kNominalVideoSamplePeriod + rounding) /
          time_diff.InMicroseconds();
      LogEnumerationHistogramEvent(
          GetMetricsNameWithAppName("Media", "DelayedVideoFramesPer5Sec"),
          value, 50);
    }
    if (dropped_frames >= 0) {
      value = (dropped_frames * kNominalVideoSamplePeriod + rounding) /
          time_diff.InMicroseconds();
      LogEnumerationHistogramEvent(
          GetMetricsNameWithAppName("Media", "DroppedVideoFramesPer5Sec"),
          value, 50);
    }
    if (error_frames >= 0) {
      value = (error_frames * kNominalVideoSamplePeriod + rounding) /
          time_diff.InMicroseconds();
      LogEnumerationHistogramEvent(
          GetMetricsNameWithAppName("Media", "ErrorVideoFramesPer5Sec"),
          value, 50);
    }
  }

  previous_video_stat_sample_time_ = sample_time;
}

std::string CastMetricsHelper::GetMetricsNameWithAppName(
    const std::string& prefix,
    const std::string& suffix) const {
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  std::string metrics_name(prefix);
  if (!app_id_.empty()) {
    if (!metrics_name.empty())
      metrics_name.push_back('.');
    metrics_name.append(app_id_);
  }
  if (!suffix.empty()) {
    if (!metrics_name.empty())
      metrics_name.push_back('.');
    metrics_name.append(suffix);
  }
  return metrics_name;
}

void CastMetricsHelper::SetMetricsSink(MetricsSink* delegate) {
  MAKE_SURE_THREAD(SetMetricsSink, delegate);
  metrics_sink_ = delegate;
}

void CastMetricsHelper::SetRecordActionCallback(
      const RecordActionCallback& callback) {
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  record_action_callback_ = callback;
}

void CastMetricsHelper::RecordSimpleAction(const std::string& action) {
  MAKE_SURE_THREAD(RecordSimpleAction, action);

  if (metrics_sink_) {
    metrics_sink_->OnAction(action);
  } else {
    record_action_callback_.Run(action);
  }
}

void CastMetricsHelper::LogEnumerationHistogramEvent(
    const std::string& name, int value, int num_buckets) {
  MAKE_SURE_THREAD(LogEnumerationHistogramEvent, name, value, num_buckets);

  if (metrics_sink_) {
    metrics_sink_->OnEnumerationEvent(name, value, num_buckets);
  } else {
    UMA_HISTOGRAM_ENUMERATION_NO_CACHE(name, value, num_buckets);
  }
}

void CastMetricsHelper::LogTimeHistogramEvent(const std::string& name,
                                              const base::TimeDelta& value,
                                              const base::TimeDelta& min,
                                              const base::TimeDelta& max,
                                              int num_buckets) {
  MAKE_SURE_THREAD(LogTimeHistogramEvent, name, value, min, max, num_buckets);

  if (metrics_sink_) {
    metrics_sink_->OnTimeEvent(name, value, min, max, num_buckets);
  } else {
    UMA_HISTOGRAM_CUSTOM_TIMES_NO_CACHE(name, value, min, max, num_buckets);
  }
}

void CastMetricsHelper::LogMediumTimeHistogramEvent(
    const std::string& name,
    const base::TimeDelta& value) {
  // Follow UMA_HISTOGRAM_MEDIUM_TIMES definition.
  LogTimeHistogramEvent(name, value,
                        base::TimeDelta::FromMilliseconds(10),
                        base::TimeDelta::FromMinutes(3),
                        50);
}

}  // namespace metrics
}  // namespace chromecast
