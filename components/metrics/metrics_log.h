// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines a set of user experience metrics data recorded by
// the MetricsService.  This is the unit of data that is sent to the server.

#ifndef COMPONENTS_METRICS_METRICS_LOG_H_
#define COMPONENTS_METRICS_METRICS_LOG_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/time/time.h"
#include "components/metrics/metrics_service_client.h"
#include "components/metrics/proto/chrome_user_metrics_extension.pb.h"

class PrefService;

namespace base {
class HistogramSamples;
}

namespace metrics {

namespace internal {
extern const int kOmniboxEventLimit;
extern const int kUserActionEventLimit;
}

class MetricsProvider;
class MetricsServiceClient;

class MetricsLog {
 public:
  enum LogType {
    INITIAL_STABILITY_LOG,  // The initial log containing stability stats.
    ONGOING_LOG,            // Subsequent logs in a session.
    INDEPENDENT_LOG,        // An independent log from a previous session.
  };

  // Creates a new metrics log of the specified type.
  // |client_id| is the identifier for this profile on this installation
  // |session_id| is an integer that's incremented on each application launch
  // |client| is used to interact with the embedder.
  // |local_state| is the PrefService that this instance should use.
  // Note: |this| instance does not take ownership of the |client|, but rather
  // stores a weak pointer to it. The caller should ensure that the |client| is
  // valid for the lifetime of this class.
  MetricsLog(const std::string& client_id,
             int session_id,
             LogType log_type,
             MetricsServiceClient* client,
             PrefService* local_state);
  virtual ~MetricsLog();

  // Registers local state prefs used by this class.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Computes the MD5 hash of the given string, and returns the first 8 bytes of
  // the hash.
  static uint64_t Hash(const std::string& value);

  // Get the GMT buildtime for the current binary, expressed in seconds since
  // January 1, 1970 GMT.
  // The value is used to identify when a new build is run, so that previous
  // reliability stats, from other builds, can be abandoned.
  static int64_t GetBuildTime();

  // Convenience function to return the current time at a resolution in seconds.
  // This wraps base::TimeTicks, and hence provides an abstract time that is
  // always incrementing for use in measuring time durations.
  static int64_t GetCurrentTime();

  // Record core profile settings into the SystemProfileProto.
  static void RecordCoreSystemProfile(MetricsServiceClient* client,
                                      SystemProfileProto* system_profile);

  // Records a user-initiated action.
  void RecordUserAction(const std::string& key);

  // Record any changes in a given histogram for transmission.
  void RecordHistogramDelta(const std::string& histogram_name,
                            const base::HistogramSamples& snapshot);


  // TODO(rkaplow): I think this can be a little refactored as it currently
  // records a pretty arbitrary set of things.
  // Records the current operating environment, including metrics provided by
  // the specified set of |metrics_providers|.  Takes the list of synthetic
  // trial IDs as a parameter. A synthetic trial is one that is set up
  // dynamically by code in Chrome. For example, a pref may be mapped to a
  // synthetic trial such that the group is determined by the pref value. The
  // current environment is returned serialized as a string.
  std::string RecordEnvironment(
      const std::vector<std::unique_ptr<MetricsProvider>>& metrics_providers,
      int64_t install_date,
      int64_t metrics_reporting_enabled_date);

  // Loads a saved system profile and the associated metrics into the log.
  // Returns true on success. Keep calling it with fresh logs until it returns
  // false.
  bool LoadIndependentMetrics(MetricsProvider* metrics_provider);

  // Loads the environment proto that was saved by the last RecordEnvironment()
  // call from prefs. On success, returns true and |app_version| contains the
  // recovered version. Otherwise (if there was no saved environment in prefs
  // or it could not be decoded), returns false and |app_version| is empty.
  bool LoadSavedEnvironmentFromPrefs(std::string* app_version);

  // Writes application stability metrics, including stability metrics provided
  // by the specified set of |metrics_providers|. The system profile portion of
  // the log must have already been filled in by a call to RecordEnvironment()
  // or LoadSavedEnvironmentFromPrefs().
  // NOTE: Has the side-effect of clearing the stability prefs..
  //
  // If this log is of type INITIAL_STABILITY_LOG, records additional info such
  // as number of incomplete shutdowns as well as extra breakpad and debugger
  // stats.
  void RecordStabilityMetrics(
      const std::vector<std::unique_ptr<MetricsProvider>>& metrics_providers,
      base::TimeDelta incremental_uptime,
      base::TimeDelta uptime);

  // Records general metrics based on the specified |metrics_providers|.
  void RecordGeneralMetrics(
      const std::vector<std::unique_ptr<MetricsProvider>>& metrics_providers);

  // Stop writing to this record and generate the encoded representation.
  // None of the Record* methods can be called after this is called.
  void CloseLog();

  // Truncate some of the fields within the log that we want to restrict in
  // size due to bandwidth concerns.
  void TruncateEvents();

  // Fills |encoded_log| with the serialized protobuf representation of the
  // record.  Must only be called after CloseLog() has been called.
  void GetEncodedLog(std::string* encoded_log);

  const base::TimeTicks& creation_time() const {
    return creation_time_;
  }

  LogType log_type() const { return log_type_; }

 protected:
  // Exposed for the sake of mocking/accessing in test code.

  ChromeUserMetricsExtension* uma_proto() { return &uma_proto_; }

  // Exposed to allow subclass to access to export the uma_proto. Can be used
  // by external components to export logs to Chrome.
  const ChromeUserMetricsExtension* uma_proto() const {
    return &uma_proto_;
  }

 private:
  // Returns true if the environment has already been filled in by a call to
  // RecordEnvironment() or LoadSavedEnvironmentFromPrefs().
  bool HasEnvironment() const;

  // Write the default state of the enable metrics checkbox.
  void WriteMetricsEnableDefault(EnableMetricsDefault metrics_default,
                                 SystemProfileProto* system_profile);

  // Returns true if the stability metrics have already been filled in by a
  // call to RecordStabilityMetrics().
  bool HasStabilityMetrics() const;

  // Within the stability group, write attributes that need to be updated asap
  // and can't be delayed until the user decides to restart chromium.
  // Delaying these stats would bias metrics away from happy long lived
  // chromium processes (ones that don't crash, and keep on running).
  void WriteRealtimeStabilityAttributes(base::TimeDelta incremental_uptime,
                                        base::TimeDelta uptime);

  // closed_ is true when record has been packed up for sending, and should
  // no longer be written to.  It is only used for sanity checking.
  bool closed_;

  // The type of the log, i.e. initial or ongoing.
  const LogType log_type_;

  // Stores the protocol buffer representation for this log.
  ChromeUserMetricsExtension uma_proto_;

  // Used to interact with the embedder. Weak pointer; must outlive |this|
  // instance.
  MetricsServiceClient* const client_;

  // The time when the current log was created.
  const base::TimeTicks creation_time_;

  PrefService* local_state_;

  DISALLOW_COPY_AND_ASSIGN(MetricsLog);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_METRICS_LOG_H_
