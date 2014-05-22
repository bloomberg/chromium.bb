// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines a set of user experience metrics data recorded by
// the MetricsService.  This is the unit of data that is sent to the server.

#ifndef CHROME_BROWSER_METRICS_METRICS_LOG_H_
#define CHROME_BROWSER_METRICS_METRICS_LOG_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "chrome/browser/metrics/extension_metrics.h"
#include "chrome/browser/metrics/metrics_network_observer.h"
#include "chrome/common/variations/variations_util.h"
#include "chrome/installer/util/google_update_settings.h"
#include "components/metrics/metrics_log_base.h"
#include "ui/gfx/size.h"

class HashedExtensionMetrics;
class MetricsNetworkObserver;
class PrefService;
class PrefRegistrySimple;

#if defined(OS_CHROMEOS)
class MetricsLogChromeOS;
#endif

namespace base {
class DictionaryValue;
}

namespace content {
struct WebPluginInfo;
}

namespace metrics {
class MetricsProvider;
class MetricsServiceClient;
}

namespace tracked_objects {
struct ProcessDataSnapshot;
}

namespace variations {
struct ActiveGroupId;
}

class MetricsLog : public metrics::MetricsLogBase {
 public:
  // Creates a new metrics log of the specified type.
  // |client_id| is the identifier for this profile on this installation
  // |session_id| is an integer that's incremented on each application launch
  // |client| is used to interact with the embedder.
  // Note: |this| instance does not take ownership of the |client|, but rather
  // stores a weak pointer to it. The caller should ensure that the |client| is
  // valid for the lifetime of this class.
  MetricsLog(const std::string& client_id,
             int session_id,
             LogType log_type,
             metrics::MetricsServiceClient* client);
  virtual ~MetricsLog();

  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Records the current operating environment, including metrics provided by
  // the specified set of |metrics_providers|.  Takes the list of installed
  // plugins, Google Update statistics, and synthetic trial IDs as parameters
  // because those can't be obtained synchronously from the UI thread.
  // A synthetic trial is one that is set up dynamically by code in Chrome. For
  // example, a pref may be mapped to a synthetic trial such that the group
  // is determined by the pref value.
  void RecordEnvironment(
      const std::vector<metrics::MetricsProvider*>& metrics_providers,
      const std::vector<content::WebPluginInfo>& plugin_list,
      const std::vector<variations::ActiveGroupId>& synthetic_trials);

  // Loads the environment proto that was saved by the last RecordEnvironment()
  // call from prefs and clears the pref value. Returns true on success or false
  // if there was no saved environment in prefs or it could not be decoded.
  bool LoadSavedEnvironmentFromPrefs();

  // Records the passed profiled data, which should be a snapshot of the
  // browser's profiled performance during startup for a single process.
  void RecordProfilerData(
      const tracked_objects::ProcessDataSnapshot& process_data,
      int process_type);

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
      const std::vector<metrics::MetricsProvider*>& metrics_providers,
      base::TimeDelta incremental_uptime,
      base::TimeDelta uptime);

  // Records general metrics based on the specified |metrics_providers|.
  void RecordGeneralMetrics(
      const std::vector<metrics::MetricsProvider*>& metrics_providers);

  const base::TimeTicks& creation_time() const {
    return creation_time_;
  }

 protected:
  // Exposed for the sake of mocking in test code.

  // Returns the PrefService from which to log metrics data.
  virtual PrefService* GetPrefService();

  // Returns the screen size for the primary monitor.
  virtual gfx::Size GetScreenSize() const;

  // Returns the device scale factor for the primary monitor.
  virtual float GetScreenDeviceScaleFactor() const;

  // Returns the number of monitors the user is using.
  virtual int GetScreenCount() const;

  // Fills |field_trial_ids| with the list of initialized field trials name and
  // group ids.
  virtual void GetFieldTrialIds(
      std::vector<variations::ActiveGroupId>* field_trial_ids) const;

  // Exposed to allow dependency injection for tests.
#if defined(OS_CHROMEOS)
  scoped_ptr<MetricsLogChromeOS> metrics_log_chromeos_;
#endif

 private:
  FRIEND_TEST_ALL_PREFIXES(MetricsLogTest, ChromeOSStabilityData);

  // Returns true if the environment has already been filled in by a call to
  // RecordEnvironment() or LoadSavedEnvironmentFromPrefs().
  bool HasEnvironment() const;

  // Returns true if the stability metrics have already been filled in by a
  // call to RecordStabilityMetrics().
  bool HasStabilityMetrics() const;

  // Within stability group, write plugin crash stats.
  void WritePluginStabilityElements(PrefService* pref);

  // Within the stability group, write required attributes.
  void WriteRequiredStabilityAttributes(PrefService* pref);

  // Within the stability group, write attributes that need to be updated asap
  // and can't be delayed until the user decides to restart chromium.
  // Delaying these stats would bias metrics away from happy long lived
  // chromium processes (ones that don't crash, and keep on running).
  void WriteRealtimeStabilityAttributes(PrefService* pref,
                                        base::TimeDelta incremental_uptime,
                                        base::TimeDelta uptime);

  // Writes the list of installed plugins.
  void WritePluginList(const std::vector<content::WebPluginInfo>& plugin_list);

  // Used to interact with the embedder. Weak pointer; must outlive |this|
  // instance.
  metrics::MetricsServiceClient* const client_;

  // Observes network state to provide values for SystemProfile::Network.
  MetricsNetworkObserver network_observer_;

  // The time when the current log was created.
  const base::TimeTicks creation_time_;

  // For including information on which extensions are installed in reports.
  HashedExtensionMetrics extension_metrics_;

  DISALLOW_COPY_AND_ASSIGN(MetricsLog);
};

#endif  // CHROME_BROWSER_METRICS_METRICS_LOG_H_
