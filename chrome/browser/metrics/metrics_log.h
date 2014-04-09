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
#include "chrome/common/metrics/metrics_log_base.h"
#include "chrome/common/metrics/variations/variations_util.h"
#include "chrome/installer/util/google_update_settings.h"
#include "ui/gfx/size.h"

class HashedExtensionMetrics;
class MetricsNetworkObserver;
struct OmniboxLog;
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

namespace tracked_objects {
struct ProcessDataSnapshot;
}

namespace chrome_variations {
struct ActiveGroupId;
}

// This is a small helper struct to pass Google Update metrics in a single
// reference argument to MetricsLog::RecordEnvironment().
struct GoogleUpdateMetrics {
    GoogleUpdateMetrics();
    ~GoogleUpdateMetrics();

    // Defines whether this is a user-level or system-level install.
    bool is_system_install;
    // The time at which Google Update last started an automatic update check.
    base::Time last_started_au;
    // The time at which Google Update last successfully recieved update
    // information from Google servers.
    base::Time last_checked;
    // Details about Google Update's attempts to update itself.
    GoogleUpdateSettings::ProductData google_update_data;
    // Details about Google Update's attempts to update this product.
    GoogleUpdateSettings::ProductData product_data;
};

class MetricsLog : public MetricsLogBase {
 public:
  // Creates a new metrics log of the specified type.
  // client_id is the identifier for this profile on this installation
  // session_id is an integer that's incremented on each application launch
  MetricsLog(const std::string& client_id, int session_id, LogType log_type);
  virtual ~MetricsLog();

  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Get the current version of the application as a string.
  static std::string GetVersionString();

  // Use |extension| in all uploaded appversions in addition to the standard
  // version string.
  static void set_version_extension(const std::string& extension);
  static const std::string& version_extension();

  // Records the current operating environment.  Takes the list of installed
  // plugins, Google Update statistics, and synthetic trial IDs as parameters
  // because those can't be obtained synchronously from the UI thread.
  // A synthetic trial is one that is set up dynamically by code in Chrome. For
  // example, a pref may be mapped to a synthetic trial such that the group
  // is determined by the pref value.
  void RecordEnvironment(
      const std::vector<content::WebPluginInfo>& plugin_list,
      const GoogleUpdateMetrics& google_update_metrics,
      const std::vector<chrome_variations::ActiveGroupId>& synthetic_trials);

  // Loads the environment proto that was saved by the last RecordEnvironment()
  // call from prefs and clears the pref value. Returns true on success or false
  // if there was no saved environment in prefs or it could not be decoded.
  bool LoadSavedEnvironmentFromPrefs();

  // Records the input text, available choices, and selected entry when the
  // user uses the Omnibox to open a URL.
  void RecordOmniboxOpenedURL(const OmniboxLog& log);

  // Records the passed profiled data, which should be a snapshot of the
  // browser's profiled performance during startup for a single process.
  void RecordProfilerData(
      const tracked_objects::ProcessDataSnapshot& process_data,
      int process_type);

  // Writes application stability metrics (as part of the profile log). The
  // system profile portion of the log must have already been filled in by a
  // call to RecordEnvironment() or LoadSavedEnvironmentFromPrefs().
  // NOTE: Has the side-effect of clearing the stability prefs..
  //
  // If this log is of type INITIAL_STABILITY_LOG, records additional info such
  // as number of incomplete shutdowns as well as extra breakpad and debugger
  // stats.
  void RecordStabilityMetrics(base::TimeDelta incremental_uptime,
                              base::TimeDelta uptime);

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
      std::vector<chrome_variations::ActiveGroupId>* field_trial_ids) const;

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

  // Writes info about the Google Update install that is managing this client.
  // This is a no-op if called on a non-Windows platform.
  void WriteGoogleUpdateProto(const GoogleUpdateMetrics& google_update_metrics);

  // Observes network state to provide values for SystemProfile::Network.
  MetricsNetworkObserver network_observer_;

  // The time when the current log was created.
  const base::TimeTicks creation_time_;

  // For including information on which extensions are installed in reports.
  HashedExtensionMetrics extension_metrics_;

  DISALLOW_COPY_AND_ASSIGN(MetricsLog);
};

#endif  // CHROME_BROWSER_METRICS_METRICS_LOG_H_
