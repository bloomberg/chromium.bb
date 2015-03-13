// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_RAPPOR_RAPPOR_SERVICE_H_
#define COMPONENTS_RAPPOR_RAPPOR_SERVICE_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer/timer.h"
#include "components/metrics/daily_event.h"
#include "components/rappor/rappor_parameters.h"

class PrefRegistrySimple;
class PrefService;

namespace net {
class URLRequestContextGetter;
}

namespace rappor {

class LogUploaderInterface;
class RapporMetric;
class RapporReports;

// The type of data stored in a metric.
enum RapporType {
  // For sampling the eTLD+1 of a URL.
  ETLD_PLUS_ONE_RAPPOR_TYPE = 0,
  COARSE_RAPPOR_TYPE,
  NUM_RAPPOR_TYPES
};

// This class provides an interface for recording samples for rappor metrics,
// and periodically generates and uploads reports based on the collected data.
class RapporService {
 public:
  // Constructs a RapporService.
  // Calling code is responsible for ensuring that the lifetime of
  // |pref_service| is longer than the lifetime of RapporService.
  // |is_incognito_callback| will be called to test if incognito mode is active.
  RapporService(PrefService* pref_service,
                const base::Callback<bool(void)> is_incognito_callback);
  virtual ~RapporService();

  // Add an observer for collecting daily metrics.
  void AddDailyObserver(scoped_ptr<metrics::DailyEvent::Observer> observer);

  // Initializes the rappor service, including loading the cohort and secret
  // preferences from disk.
  void Initialize(net::URLRequestContextGetter* context);

  // Updates the settings for metric recording and uploading.
  // The RapporService must be initialized before this method is called.
  // If |recording_level| > REPORTING_DISABLED, periodic reports will be
  // generated and queued for upload.
  // If |may_upload| is true, reports will be uploaded from the queue.
  void Update(RecordingLevel recording_level, bool may_upload);

  // Records a sample of the rappor metric specified by |metric_name|.
  // Creates and initializes the metric, if it doesn't yet exist.
  virtual void RecordSample(const std::string& metric_name,
                            RapporType type,
                            const std::string& sample);

  // Registers the names of all of the preferences used by RapporService in the
  // provided PrefRegistry. This should be called before calling Start().
  static void RegisterPrefs(PrefRegistrySimple* registry);

 protected:
  // Initializes the state of the RapporService.
  void InitializeInternal(scoped_ptr<LogUploaderInterface> uploader,
                          int32_t cohort,
                          const std::string& secret);

  // Sets the recording level.
  void SetRecordingLevel(RecordingLevel parameters);

  // Cancels the next call to OnLogInterval.
  virtual void CancelNextLogRotation();

  // Schedules the next call to OnLogInterval.
  virtual void ScheduleNextLogRotation(base::TimeDelta interval);

  // Logs all of the collected metrics to the reports proto message and clears
  // the internal map. Exposed for tests. Returns true if any metrics were
  // recorded.
  bool ExportMetrics(RapporReports* reports);

 private:
  // Records a sample of the rappor metric specified by |parameters|.
  // Creates and initializes the metric, if it doesn't yet exist.
  // Exposed for tests.
  void RecordSampleInternal(const std::string& metric_name,
                            const RapporParameters& parameters,
                            const std::string& sample);

  // Checks if the service has been started successfully.
  bool IsInitialized() const;

  // Called whenever the logging interval elapses to generate a new log of
  // reports and pass it to the uploader.
  void OnLogInterval();

  // Finds a metric in the metrics_map_, creating it if it doesn't already
  // exist.
  RapporMetric* LookUpMetric(const std::string& metric_name,
                             const RapporParameters& parameters);

  // A weak pointer to the PrefService used to read and write preferences.
  PrefService* pref_service_;

  // A callback for testing if incognito mode is active;
  const base::Callback<bool(void)> is_incognito_callback_;

  // Client-side secret used to generate fake bits.
  std::string secret_;

  // The cohort this client is assigned to. -1 is uninitialized.
  int32_t cohort_;

  // Timer which schedules calls to OnLogInterval().
  base::OneShotTimer<RapporService> log_rotation_timer_;

  // A daily event for collecting metrics once a day.
  metrics::DailyEvent daily_event_;

  // A private LogUploader instance for sending reports to the server.
  scoped_ptr<LogUploaderInterface> uploader_;

  // What reporting level of metrics are being reported.
  RecordingLevel recording_level_;

  // We keep all registered metrics in a map, from name to metric.
  // The map owns the metrics it contains.
  std::map<std::string, RapporMetric*> metrics_map_;

  DISALLOW_COPY_AND_ASSIGN(RapporService);
};

}  // namespace rappor

#endif  // COMPONENTS_RAPPOR_RAPPOR_SERVICE_H_
