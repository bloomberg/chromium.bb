// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BASE_METRICS_CAST_METRICS_HELPER_H_
#define CHROMECAST_BASE_METRICS_CAST_METRICS_HELPER_H_

#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"

namespace base {
class MessageLoopProxy;
}

namespace chromecast {
namespace metrics {

// Helper class for tracking complex metrics. This particularly includes
// playback metrics that span events across time, such as "time from app launch
// to video being rendered."
class CastMetricsHelper {
 public:
  enum BufferingType {
    kInitialBuffering,
    kBufferingAfterUnderrun,
    kAbortedBuffering,
  };

  typedef base::Callback<void(const std::string&)> RecordActionCallback;

  class MetricsSink {
   public:
    virtual ~MetricsSink() {}

    virtual void OnAction(const std::string& action) = 0;
    virtual void OnEnumerationEvent(const std::string& name,
                                    int value, int num_buckets) = 0;
    virtual void OnTimeEvent(const std::string& name,
                             const base::TimeDelta& value,
                             const base::TimeDelta& min,
                             const base::TimeDelta& max,
                             int num_buckets) = 0;
  };

  // Decodes action_name/app_id/session_id/sdk_version from metrics name.
  // Return false if the metrics name is not generated from
  // EncodeAppInfoIntoMetricsName() with correct format.
  static bool DecodeAppInfoFromMetricsName(
      const std::string& metrics_name,
      std::string* action_name,
      std::string* app_id,
      std::string* session_id,
      std::string* sdk_version);

  static CastMetricsHelper* GetInstance();

  explicit CastMetricsHelper(
      scoped_refptr<base::MessageLoopProxy> message_loop_proxy);
  virtual ~CastMetricsHelper();

  // This function updates the info and stores the startup time of the current
  // active application
  virtual void UpdateCurrentAppInfo(const std::string& app_id,
                                    const std::string& session_id);
  // This function updates the sdk version of the current active application
  virtual void UpdateSDKInfo(const std::string& sdk_version);

  // Logs UMA record for media play/pause user actions.
  virtual void LogMediaPlay();
  virtual void LogMediaPause();

  // Logs a simple UMA user action.
  // This is used as an in-place replacement of content::RecordComputedAction().
  virtual void RecordSimpleAction(const std::string& action);

  // Logs UMA record of the time the app made its first paint.
  virtual void LogTimeToFirstPaint();

  // Logs UMA record of the elapsed time from the app launch
  // to the time first video frame is displayed.
  virtual void LogTimeToDisplayVideo();

  // Logs UMA record of the time needed to re-buffer A/V.
  virtual void LogTimeToBufferAv(BufferingType buffering_type,
                                 base::TimeDelta time);

  virtual void ResetVideoFrameSampling();

  // Logs UMA statistics for video decoder and rendering data.
  // Negative values are considered invalid and will not be logged.
  virtual void LogFramesPer5Seconds(int displayed_frames, int dropped_frames,
                                    int delayed_frames, int error_frames);

  // Returns metrics name with app name between prefix and suffix.
  virtual std::string GetMetricsNameWithAppName(
      const std::string& prefix,
      const std::string& suffix) const;

  // Provides a MetricsSink instance to delegate UMA event logging.
  // Once the delegate interface is set, CastMetricsHelper will not log UMA
  // events internally unless SetMetricsSink(NULL) is called.
  // CastMetricsHelper can only hold one MetricsSink instance.
  // Caller retains ownership of MetricsSink.
  virtual void SetMetricsSink(MetricsSink* delegate);

  // Sets a default callback to record user action when MetricsSink is not set.
  // This function could be called multiple times (in unittests), and
  // CastMetricsHelper only honors the last one.
  virtual void SetRecordActionCallback(const RecordActionCallback& callback);

 protected:
  // Creates a CastMetricsHelper instance with no MessageLoopProxy. This should
  // only be used by tests, since invoking any non-overridden methods on this
  // instance will cause a failure.
  CastMetricsHelper();

 private:
  static std::string EncodeAppInfoIntoMetricsName(
      const std::string& action_name,
      const std::string& app_id,
      const std::string& session_id,
      const std::string& sdk_version);

  void LogEnumerationHistogramEvent(const std::string& name,
                                    int value, int num_buckets);
  void LogTimeHistogramEvent(const std::string& name,
                             const base::TimeDelta& value,
                             const base::TimeDelta& min,
                             const base::TimeDelta& max,
                             int num_buckets);
  void LogMediumTimeHistogramEvent(const std::string& name,
                                   const base::TimeDelta& value);

  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;

  // Start time of the most recent app.
  base::TimeTicks app_start_time_;

  // Currently running app id. Used to construct histogram name.
  std::string app_id_;
  std::string session_id_;
  std::string sdk_version_;

  // Whether a new app start time has been stored but not recorded.
  // After the startup time has been used to generate an UMA event,
  // this is set to false.
  bool new_startup_time_;

  base::TimeTicks previous_video_stat_sample_time_;

  MetricsSink* metrics_sink_;
  // Default RecordAction callback when metrics_sink_ is not set.
  RecordActionCallback record_action_callback_;

  DISALLOW_COPY_AND_ASSIGN(CastMetricsHelper);
};

}  // namespace metrics
}  // namespace chromecast

#endif  // CHROMECAST_BASE_METRICS_CAST_METRICS_HELPER_H_
