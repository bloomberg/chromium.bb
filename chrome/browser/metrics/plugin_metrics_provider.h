// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_PLUGIN_METRICS_PROVIDER_H_
#define CHROME_BROWSER_METRICS_PLUGIN_METRICS_PROVIDER_H_

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "components/metrics/metrics_provider.h"
#include "content/public/browser/browser_child_process_observer.h"

namespace base {
class FilePath;
}

namespace content {
struct WebPluginInfo;
}

class PrefRegistrySimple;
class PrefService;

// PluginMetricsProvider is responsible for adding out plugin information to
// the UMA proto.
class PluginMetricsProvider : public metrics::MetricsProvider,
                              public content::BrowserChildProcessObserver {
 public:
  explicit PluginMetricsProvider(PrefService* local_state);
  virtual ~PluginMetricsProvider();

  // Fetches plugin information data asynchronously and calls |done_callback|
  // when done.
  void GetPluginInformation(const base::Closure& done_callback);

  // metrics::MetricsDataProvider:
  virtual void ProvideSystemProfileMetrics(
      metrics::SystemProfileProto* system_profile_proto) OVERRIDE;
  virtual void ProvideStabilityMetrics(
      metrics::SystemProfileProto* system_profile_proto) OVERRIDE;
  virtual void RecordCurrentState() OVERRIDE;

  // Notifies the provider about an error loading the plugin at |plugin_path|.
  void LogPluginLoadingError(const base::FilePath& plugin_path);

  // Sets this provider's list of plugins, exposed for testing.
  void SetPluginsForTesting(const std::vector<content::WebPluginInfo>& plugins);

  // Returns true if process of type |type| should be counted as a plugin
  // process, and false otherwise.
  static bool IsPluginProcess(int process_type);

  // Registers local state prefs used by this class.
  static void RegisterPrefs(PrefRegistrySimple* registry);

 private:
  struct ChildProcessStats;

  // Receives the plugin list from the PluginService and calls |done_callback|.
  void OnGotPlugins(const base::Closure& done_callback,
                    const std::vector<content::WebPluginInfo>& plugins);

  // Returns reference to ChildProcessStats corresponding to |data|.
  ChildProcessStats& GetChildProcessStats(
      const content::ChildProcessData& data);

  // content::BrowserChildProcessObserver:
  virtual void BrowserChildProcessHostConnected(
      const content::ChildProcessData& data) OVERRIDE;
  virtual void BrowserChildProcessCrashed(
      const content::ChildProcessData& data) OVERRIDE;
  virtual void BrowserChildProcessInstanceCreated(
      const content::ChildProcessData& data) OVERRIDE;

  PrefService* local_state_;

  // The list of plugins which was retrieved on the file thread.
  std::vector<content::WebPluginInfo> plugins_;

  // Buffer of child process notifications for quick access.
  std::map<base::string16, ChildProcessStats> child_process_stats_buffer_;

  base::WeakPtrFactory<PluginMetricsProvider> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PluginMetricsProvider);
};

#endif  // CHROME_BROWSER_METRICS_PLUGIN_METRICS_PROVIDER_H_
