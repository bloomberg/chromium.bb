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
#include "chrome/browser/metrics/metrics_network_observer.h"
#include "chrome/common/metrics/metrics_log_base.h"
#include "chrome/installer/util/google_update_settings.h"
#include "ui/gfx/size.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/metrics/perf_provider_chromeos.h"
#endif

struct AutocompleteLog;
class MetricsNetworkObserver;
class PrefService;
class PrefRegistrySimple;

namespace base {
class DictionaryValue;
}

namespace tracked_objects {
struct ProcessDataSnapshot;
}

namespace chrome_variations {
struct ActiveGroupId;
}

namespace webkit {
struct WebPluginInfo;
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
  // Creates a new metrics log
  // client_id is the identifier for this profile on this installation
  // session_id is an integer that's incremented on each application launch
  MetricsLog(const std::string& client_id, int session_id);
  virtual ~MetricsLog();

  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Get the amount of uptime in seconds since this function was last called.
  // This updates the cumulative uptime metric for uninstall as a side effect.
  static int64 GetIncrementalUptime(PrefService* pref);

  // Get the current version of the application as a string.
  static std::string GetVersionString();

  // Use |extension| in all uploaded appversions in addition to the standard
  // version string.
  static void set_version_extension(const std::string& extension);
  static const std::string& version_extension();

  // Records the current operating environment.  Takes the list of installed
  // plugins and Google Update statistics as parameters because those can't be
  // obtained synchronously from the UI thread.
  // profile_metrics, if non-null, gives a dictionary of all profile metrics
  // that are to be recorded. Each value in profile_metrics should be a
  // dictionary giving the metrics for the profile.
  void RecordEnvironment(
      const std::vector<webkit::WebPluginInfo>& plugin_list,
      const GoogleUpdateMetrics& google_update_metrics,
      const base::DictionaryValue* profile_metrics);

  // Records the current operating environment.  Takes the list of installed
  // plugins and Google Update statistics as parameters because those can't be
  // obtained synchronously from the UI thread.  This is exposed as a separate
  // method from the |RecordEnvironment()| method above because we record the
  // environment with *each* protobuf upload, but only with the initial XML
  // upload.
  void RecordEnvironmentProto(
      const std::vector<webkit::WebPluginInfo>& plugin_list,
      const GoogleUpdateMetrics& google_update_metrics);

  // Records the input text, available choices, and selected entry when the
  // user uses the Omnibox to open a URL.
  void RecordOmniboxOpenedURL(const AutocompleteLog& log);

  // Records the passed profiled data, which should be a snapshot of the
  // browser's profiled performance during startup for a single process.
  void RecordProfilerData(
      const tracked_objects::ProcessDataSnapshot& process_data,
      int process_type);

  // Record recent delta for critical stability metrics.  We can't wait for a
  // restart to gather these, as that delay biases our observation away from
  // users that run happily for a looooong time.  We send increments with each
  // uma log upload, just as we send histogram data.  Takes the list of
  // installed plugins as a parameter because that can't be obtained
  // synchronously from the UI thread.
  void RecordIncrementalStabilityElements(
      const std::vector<webkit::WebPluginInfo>& plugin_list);

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

 private:
  FRIEND_TEST_ALL_PREFIXES(MetricsLogTest, ChromeOSStabilityData);

  // Writes application stability metrics (as part of the profile log).
  // NOTE: Has the side-effect of clearing those counts.
  void WriteStabilityElement(
      const std::vector<webkit::WebPluginInfo>& plugin_list,
      PrefService* pref);

  // Within stability group, write plugin crash stats.
  void WritePluginStabilityElements(
      const std::vector<webkit::WebPluginInfo>& plugin_list,
      PrefService* pref);

  // Within the stability group, write required attributes.
  void WriteRequiredStabilityAttributes(PrefService* pref);

  // Within the stability group, write attributes that need to be updated asap
  // and can't be delayed until the user decides to restart chromium.
  // Delaying these stats would bias metrics away from happy long lived
  // chromium processes (ones that don't crash, and keep on running).
  void WriteRealtimeStabilityAttributes(PrefService* pref);

  // Writes the list of installed plugins.  If |write_as_xml| is true, writes
  // the XML version.  Otherwise, writes the protobuf version.
  void WritePluginList(
      const std::vector<webkit::WebPluginInfo>& plugin_list,
      bool write_as_xml);

  // Within the profile group, write basic install info including appversion.
  void WriteInstallElement();

  // Writes all profile metrics. This invokes WriteProfileMetrics for each key
  // in all_profiles_metrics that starts with kProfilePrefix.
  void WriteAllProfilesMetrics(
      const base::DictionaryValue& all_profiles_metrics);

  // Writes metrics for the profile identified by key. This writes all
  // key/value pairs in profile_metrics.
  void WriteProfileMetrics(const std::string& key,
                           const base::DictionaryValue& profile_metrics);

  // Writes info about the Google Update install that is managing this client.
  // This is a no-op if called on a non-Windows platform.
  void WriteGoogleUpdateProto(const GoogleUpdateMetrics& google_update_metrics);

  // Observes network state to provide values for SystemProfile::Network.
  MetricsNetworkObserver network_observer_;

#if defined(OS_CHROMEOS)
  metrics::PerfProvider perf_provider_;
#endif

  DISALLOW_COPY_AND_ASSIGN(MetricsLog);
};

#endif  // CHROME_BROWSER_METRICS_METRICS_LOG_H_
