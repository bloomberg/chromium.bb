// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines a set of user experience metrics data recorded by
// the MetricsService.  This is the unit of data that is sent to the server.

#ifndef CHROME_BROWSER_METRICS_METRICS_LOG_H_
#define CHROME_BROWSER_METRICS_METRICS_LOG_H_
#pragma once

#include "base/basictypes.h"
#include "chrome/common/metrics_helpers.h"
#include "chrome/common/page_transition_types.h"

struct AutocompleteLog;
class DictionaryValue;
class GURL;
class PrefService;

namespace webkit {
namespace npapi {
struct WebPluginInfo;
}
}

class MetricsLog : public MetricsLogBase {
 public:
  // Creates a new metrics log
  // client_id is the identifier for this profile on this installation
  // session_id is an integer that's incremented on each application launch
  MetricsLog(const std::string& client_id, int session_id);
  virtual ~MetricsLog();

  static void RegisterPrefs(PrefService* prefs);

  // Records the current operating environment.  Takes the list of installed
  // plugins as a parameter because that can't be obtained synchronously
  // from the UI thread.
  // profile_metrics, if non-null, gives a dictionary of all profile metrics
  // that are to be recorded. Each value in profile_metrics should be a
  // dictionary giving the metrics for the profile.
  void RecordEnvironment(
      const std::vector<webkit::npapi::WebPluginInfo>& plugin_list,
      const DictionaryValue* profile_metrics);

  // Records the input text, available choices, and selected entry when the
  // user uses the Omnibox to open a URL.
  void RecordOmniboxOpenedURL(const AutocompleteLog& log);

  // Record recent delta for critical stability metrics.  We can't wait for a
  // restart to gather these, as that delay biases our observation away from
  // users that run happily for a looooong time.  We send increments with each
  // uma log upload, just as we send histogram data.
  void RecordIncrementalStabilityElements();

  // Get the amount of uptime in seconds since this function was last called.
  // This updates the cumulative uptime metric for uninstall as a side effect.
  static int64 GetIncrementalUptime(PrefService* pref);

  // Get the current version of the application as a string.
  static std::string GetVersionString();

  virtual MetricsLog* AsMetricsLog();

 private:
  FRIEND_TEST_ALL_PREFIXES(MetricsLogTest, ChromeOSStabilityData);

  // Returns the date at which the current metrics client ID was created as
  // a string containing milliseconds since the epoch, or "0" if none was found.
  std::string GetInstallDate() const;


  // Writes application stability metrics (as part of the profile log).
  // NOTE: Has the side-effect of clearing those counts.
  void WriteStabilityElement(PrefService* pref);

  // Within stability group, write plugin crash stats.
  void WritePluginStabilityElements(PrefService* pref);

  // Within the stability group, write required attributes.
  void WriteRequiredStabilityAttributes(PrefService* pref);

  // Within the stability group, write attributes that need to be updated asap
  // and can't be delayed until the user decides to restart chromium.
  // Delaying these stats would bias metrics away from happy long lived
  // chromium processes (ones that don't crash, and keep on running).
  void WriteRealtimeStabilityAttributes(PrefService* pref);

  // Writes the list of installed plugins.
  void WritePluginList(
      const std::vector<webkit::npapi::WebPluginInfo>& plugin_list);

  // Within the profile group, write basic install info including appversion.
  void WriteInstallElement();

  // Writes all profile metrics. This invokes WriteProfileMetrics for each key
  // in all_profiles_metrics that starts with kProfilePrefix.
  void WriteAllProfilesMetrics(const DictionaryValue& all_profiles_metrics);

  // Writes metrics for the profile identified by key. This writes all
  // key/value pairs in profile_metrics.
  void WriteProfileMetrics(const std::string& key,
                           const DictionaryValue& profile_metrics);

  DISALLOW_COPY_AND_ASSIGN(MetricsLog);
};

#endif  // CHROME_BROWSER_METRICS_METRICS_LOG_H_
