// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MONITOR_PERFORMANCE_MONITOR_H_
#define CHROME_BROWSER_PERFORMANCE_MONITOR_PERFORMANCE_MONITOR_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/file_path.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/process.h"
#include "base/process_util.h"
#include "base/timer.h"
#include "chrome/browser/performance_monitor/database.h"
#include "chrome/browser/performance_monitor/event.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_process_host.h"

namespace extensions {
class Extension;
}

namespace performance_monitor {
class Database;

class PerformanceMonitor : public content::NotificationObserver {
 public:
  typedef std::map<base::ProcessHandle,
                   linked_ptr<base::ProcessMetrics> > MetricsMap;

  // Set the path which the PerformanceMonitor should use for the database files
  // constructed. This must be done prior to the initialization of the
  // PerformanceMonitor. Returns true on success, false on failure (failure
  // likely indicates that PerformanceMonitor has already been started at the
  // time of the call).
  bool SetDatabasePath(const FilePath& path);

  // Returns the current PerformanceMonitor instance if one exists; otherwise
  // constructs a new PerformanceMonitor.
  static PerformanceMonitor* GetInstance();

  // Begins the initialization process for the PerformanceMonitor in order to
  // start collecting data.
  void Start();

  // content::NotificationObserver
  // Wait for various notifications; insert events into the database upon
  // occurance.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  Database* database() { return database_.get(); }
  FilePath database_path() { return database_path_; }

 private:
  friend struct DefaultSingletonTraits<PerformanceMonitor>;
  friend class PerformanceMonitorBrowserTest;
  FRIEND_TEST_ALL_PREFIXES(PerformanceMonitorUncleanExitBrowserTest,
                           OneProfileUncleanExit);
  FRIEND_TEST_ALL_PREFIXES(PerformanceMonitorUncleanExitBrowserTest,
                           TwoProfileUncleanExit);

  PerformanceMonitor();
  virtual ~PerformanceMonitor();

  // Perform any additional initialization which must be performed on a
  // background thread (e.g. constructing the database).
  void InitOnBackgroundThread();

  void FinishInit();

  // Register for the appropriate notifications as a NotificationObserver.
  void RegisterForNotifications();

  // Checks for whether the previous profiles closed uncleanly; this method
  // should only be called once per run in order to avoid duplication of events
  // (exceptions made for testing purposes where we construct the environment).
  void CheckForUncleanExits();

  // Find the last active time for the profile and insert the event into the
  // database.
  void AddUncleanExitEventOnBackgroundThread(const std::string& profile_name);

  // Check the previous Chrome version from the Database and determine if
  // it has been updated. If it has, insert an event in the database.
  void CheckForVersionUpdateOnBackgroundThread();

  // Wrapper function for inserting events into the database.
  void AddEvent(scoped_ptr<Event> event);

  void AddEventOnBackgroundThread(scoped_ptr<Event> event);

  // Since Database::AddMetric() is overloaded, base::Bind() does not work and
  // we need a helper function.
  void AddMetricOnBackgroundThread(MetricType type, const std::string& value);

  // Notify any listeners that PerformanceMonitor has finished the initializing.
  void NotifyInitialized();

  // Perform any collections that are done on a timed basis.
  void DoTimedCollections();

  // Update the database record of the last time the active profiles were
  // running; this is used in determining when an unclean exit occurred.
  void UpdateLiveProfiles();
  void UpdateLiveProfilesHelper(
      scoped_ptr<std::set<std::string> > active_profiles, std::string time);

  // Gathers CPU usage and memory usage of all Chrome processes in order to.
  void GatherStatisticsOnBackgroundThread();

  // Gathers the CPU usage of every Chrome process that has been running since
  // the last call to GatherStatistics().
  void GatherCPUUsageOnBackgroundThread();

  // Gathers the memory usage of every process in the current list of processes.
  void GatherMemoryUsageOnBackgroundThread();

  // Updates the ProcessMetrics map with the current list of processes.
  void UpdateMetricsMapOnBackgroundThread();

  // Generate an appropriate ExtensionEvent for an extension-related occurrance
  // and insert it in the database.
  void AddExtensionEvent(EventType type,
                         const extensions::Extension* extension);

  // Generate an appropriate CrashEvent for a renderer crash and insert it in
  // the database.
  void AddCrashEvent(
      const content::RenderProcessHost::RendererClosedDetails& details);

  // The location at which the database files are stored; if empty, the database
  // will default to '<user_data_dir>/performance_monitor_dbs'.
  FilePath database_path_;

  scoped_ptr<Database> database_;

  // A map of currently running ProcessHandles to ProcessMetrics.
  MetricsMap metrics_map_;

  // The timer to signal PerformanceMonitor to perform its timed collections.
  base::RepeatingTimer<PerformanceMonitor> timer_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(PerformanceMonitor);
};

}  // namespace performance_monitor

#endif  // CHROME_BROWSER_PERFORMANCE_MONITOR_PERFORMANCE_MONITOR_H_
