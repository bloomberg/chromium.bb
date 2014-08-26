// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POWER_PROCESS_POWER_COLLECTOR_H_
#define CHROME_BROWSER_POWER_PROCESS_POWER_COLLECTOR_H_

#include <map>

#include "base/memory/linked_ptr.h"
#include "base/process/process_handle.h"
#include "base/process/process_metrics.h"
#include "base/timer/timer.h"
#include "components/power/origin_power_map_factory.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/power_manager_client.h"
#endif

class Profile;

namespace content {
class RenderProcessHost;
}

#if defined(OS_CHROMEOS)
namespace power_manager {
class PowerSupplyProperties;
}
#endif

// Manages regular updates of the profile power consumption.
class ProcessPowerCollector
#if defined(OS_CHROMEOS)
    : public chromeos::PowerManagerClient::Observer
#endif
      {
 public:
  class PerProcessData {
   public:
    PerProcessData(scoped_ptr<base::ProcessMetrics> metrics,
                   const GURL& origin,
                   Profile* profile);
    PerProcessData();
    ~PerProcessData();

    base::ProcessMetrics* metrics() const { return metrics_.get(); }
    Profile* profile() const { return profile_; }
    GURL last_origin() const { return last_origin_; }
    int last_cpu_percent() const { return last_cpu_percent_; }
    bool seen_this_cycle() const { return seen_this_cycle_; }
    void set_last_cpu_percent(double new_cpu) { last_cpu_percent_ = new_cpu; }
    void set_seen_this_cycle(bool seen) { seen_this_cycle_ = seen; }

   private:
    // |metrics_| holds the ProcessMetrics information for the given process.
    scoped_ptr<base::ProcessMetrics> metrics_;

    // |profile| is the profile that is visiting the |last_origin_|.
    // It is not owned by PerProcessData.
    Profile* profile_;

    // |last_origin_| is the last origin visited by the process.
    GURL last_origin_;

    // |last_cpu_percent_| is the proportion of the CPU used since the last
    // query.
    double last_cpu_percent_;

    // |seen_this_cycle| represents if the process still exists in this cycle.
    // If it doesn't, we erase the PerProcessData.
    bool seen_this_cycle_;

    DISALLOW_COPY_AND_ASSIGN(PerProcessData);
  };

  // A map from all process handles to a metric.
  typedef std::map<base::ProcessHandle, linked_ptr<PerProcessData> >
      ProcessMetricsMap;
  // A callback used to define mock CPU usage for testing.
  typedef base::Callback<double(base::ProcessHandle)> CpuUsageCallback;

  // On Chrome OS, can only be initialized after the DBusThreadManager has been
  // initialized.
  ProcessPowerCollector();
  // On Chrome OS, can only be destroyed before DBusThreadManager is.
  virtual ~ProcessPowerCollector();

  void set_cpu_usage_callback_for_testing(const CpuUsageCallback& callback) {
    cpu_usage_callback_ = callback;
  }

  ProcessMetricsMap* metrics_map_for_testing() { return &metrics_map_; }

#if defined(OS_CHROMEOS)
  // PowerManagerClient::Observer implementation:
  virtual void PowerChanged(
      const power_manager::PowerSupplyProperties& prop) OVERRIDE;
#endif

  // Begin periodically updating the power consumption numbers by profile.
  void Initialize();

  // Calls UpdatePowerConsumption() and returns the total CPU percent.
  double UpdatePowerConsumptionForTesting();

 private:
  // Starts the timer for updating the power consumption.
  void StartTimer();

  // Calls SynchronizerProcesses() and RecordCpuUsageByOrigin() to update the
  // |metrics_map_| and attribute power consumption. Invoked by |timer_| and as
  // a helper method for UpdatePowerConsumptionForTesting().
  double UpdatePowerConsumption();

  // Calls UpdatePowerConsumption(). Invoked by |timer_|.
  void HandleUpdateTimeout();

  // Synchronizes the currently active processes to the |metrics_map_| and
  // returns the total amount of cpu usage in the cycle.
  double SynchronizeProcesses();

  // Attributes the power usage to the profiles and origins using the
  // information from SynchronizeProcesses() given a total amount
  // of CPU used in this cycle, |total_cpu_percent|.
  void RecordCpuUsageByOrigin(double total_cpu_percent);

  // Adds the information from a given RenderProcessHost to the |metrics_map_|
  // for a given origin. Called by SynchronizeProcesses().
  void UpdateProcessInMap(const content::RenderProcessHost* render_process,
                          const GURL& origin);

  ProcessMetricsMap metrics_map_;
  base::RepeatingTimer<ProcessPowerCollector> timer_;

  // Callback to use to get CPU usage if set.
  CpuUsageCallback cpu_usage_callback_;

  // The factor to scale the CPU usage by.
  double scale_factor_;

  DISALLOW_COPY_AND_ASSIGN(ProcessPowerCollector);
};

#endif  // CHROME_BROWSER_POWER_PROCESS_POWER_COLLECTOR_H_
