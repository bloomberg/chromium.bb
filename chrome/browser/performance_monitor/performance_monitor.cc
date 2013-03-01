// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/performance_monitor.h"

#include <set>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/worker_pool.h"
#include "base/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/performance_monitor/constants.h"
#include "chrome/browser/performance_monitor/performance_monitor_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_iterator.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/chrome_process_util.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/load_notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "net/url_request/url_request.h"

using content::BrowserThread;
using extensions::Extension;

namespace performance_monitor {

namespace {

const uint32 kAccessFlags = base::kProcessAccessDuplicateHandle |
                            base::kProcessAccessQueryInformation |
                            base::kProcessAccessTerminate |
                            base::kProcessAccessWaitForTermination;

bool g_started_initialization = false;

std::string TimeToString(base::Time time) {
  int64 time_int64 = time.ToInternalValue();
  return base::Int64ToString(time_int64);
}

bool StringToTime(std::string time, base::Time* output) {
  int64 time_int64 = 0;
  if (!base::StringToInt64(time, &time_int64))
    return false;
  *output = base::Time::FromInternalValue(time_int64);
  return true;
}

// Try to get the URL for the RenderViewHost if the host does not correspond to
// an incognito profile (we don't store URLs from incognito sessions). Returns
// true if url has been populated, and false otherwise.
bool MaybeGetURLFromRenderView(const content::RenderViewHost* view,
                               std::string* url) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderViewHost(view);

  if (Profile::FromBrowserContext(
          web_contents->GetBrowserContext())->IsOffTheRecord()) {
    return false;
  }

  *url = web_contents->GetURL().spec();
  return true;
}

// Takes ownership of and deletes |database| on the background thread, so as to
// avoid destruction in the middle of an operation.
void DeleteDatabaseOnBackgroundThread(Database* database) {
  delete database;
}

}  // namespace

bool PerformanceMonitor::initialized_ = false;

PerformanceMonitor::PerformanceDataForIOThread::PerformanceDataForIOThread()
    : network_bytes_read(0) {
}

PerformanceMonitor::PerformanceMonitor() : database_(NULL),
                                           metrics_map_(new MetricsMap) {
}

PerformanceMonitor::~PerformanceMonitor() {
  BrowserThread::PostBlockingPoolSequencedTask(
      Database::kDatabaseSequenceToken,
      FROM_HERE,
      base::Bind(&DeleteDatabaseOnBackgroundThread, database_.release()));
}

bool PerformanceMonitor::SetDatabasePath(const base::FilePath& path) {
  if (!database_.get()) {
    database_path_ = path;
    return true;
  }

  // PerformanceMonitor already initialized with another path.
  return false;
}

// static
PerformanceMonitor* PerformanceMonitor::GetInstance() {
  return Singleton<PerformanceMonitor>::get();
}

void PerformanceMonitor::Start() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Avoid responding to multiple calls to Start().
  if (g_started_initialization)
    return;

  g_started_initialization = true;
  util::PostTaskToDatabaseThreadAndReply(
      FROM_HERE,
      base::Bind(&PerformanceMonitor::InitOnBackgroundThread,
                 base::Unretained(this)),
      base::Bind(&PerformanceMonitor::FinishInit,
                 base::Unretained(this)));
}

void PerformanceMonitor::InitOnBackgroundThread() {
  CHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
  database_ = Database::Create(database_path_);

  // Initialize the io thread's performance data to the value in the database;
  // if there isn't a recording in the database, the value stays at 0.
  Metric metric;
  if (database_->GetRecentStatsForActivityAndMetric(METRIC_NETWORK_BYTES_READ,
                                                    &metric)) {
    performance_data_for_io_thread_.network_bytes_read = metric.value;
  }
}

void PerformanceMonitor::FinishInit() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  RegisterForNotifications();
  CheckForUncleanExits();
  BrowserThread::PostBlockingPoolSequencedTask(
      Database::kDatabaseSequenceToken,
      FROM_HERE,
      base::Bind(&PerformanceMonitor::CheckForVersionUpdateOnBackgroundThread,
                 base::Unretained(this)));

  int gather_interval_in_seconds = kDefaultGatherIntervalInSeconds;
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kPerformanceMonitorGathering) &&
      !CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kPerformanceMonitorGathering).empty()) {
    int specified_interval = 0;
    if (!base::StringToInt(
            CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
                switches::kPerformanceMonitorGathering),
            &specified_interval) || specified_interval <= 0) {
      LOG(ERROR) << "Invalid value for switch: '"
                 << switches::kPerformanceMonitorGathering
                 << "'; please use an integer greater than 0.";
    } else {
      gather_interval_in_seconds = specified_interval;
    }
  }

  timer_.Start(FROM_HERE,
               base::TimeDelta::FromSeconds(gather_interval_in_seconds),
               this,
               &PerformanceMonitor::DoTimedCollections);

  // Post a task to the background thread to a function which does nothing.
  // This will force any tasks the database is performing to finish prior to
  // the reply being sent, since they use the same thread.
  //
  // Important! Make sure that methods in FinishInit() only rely on posting
  // to the background thread, and do not rely upon a reply from the background
  // thread; this is necessary for this notification to be valid.
  util::PostTaskToDatabaseThreadAndReply(
      FROM_HERE,
      base::Bind(&base::DoNothing),
      base::Bind(&PerformanceMonitor::NotifyInitialized,
                 base::Unretained(this)));
}

void PerformanceMonitor::RegisterForNotifications() {
  // Extensions
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_INSTALLED,
      content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_ENABLED,
      content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
      content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_CRX_INSTALLER_DONE,
      content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNINSTALLED,
      content::NotificationService::AllSources());

  // Crashes
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_HANG,
      content::NotificationService::AllSources());
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
      content::NotificationService::AllSources());

  // Profiles (for unclean exit)
  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_ADDED,
      content::NotificationService::AllSources());

  // Page load times
  registrar_.Add(this, content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
}

// We check if profiles exited cleanly initialization time in case they were
// loaded prior to PerformanceMonitor's initialization. Later profiles will be
// checked through the PROFILE_ADDED notification.
void PerformanceMonitor::CheckForUncleanExits() {
  std::vector<Profile*> profiles =
      g_browser_process->profile_manager()->GetLoadedProfiles();

  for (std::vector<Profile*>::const_iterator iter = profiles.begin();
       iter != profiles.end(); ++iter) {
    if ((*iter)->GetLastSessionExitType() == Profile::EXIT_CRASHED) {
      BrowserThread::PostBlockingPoolSequencedTask(
          Database::kDatabaseSequenceToken,
          FROM_HERE,
          base::Bind(&PerformanceMonitor::AddUncleanExitEventOnBackgroundThread,
                     base::Unretained(this),
                     (*iter)->GetDebugName()));
    }
  }
}

void PerformanceMonitor::AddUncleanExitEventOnBackgroundThread(
  const std::string& profile_name) {
  std::string database_key = kStateProfilePrefix + profile_name;
  std::string last_active_string = database_->GetStateValue(database_key);

  // Check if there was no previous time; this should only happen if the profile
  // was last used prior to PerformanceMonitor's integration. Do nothing in this
  // case, since the event was prior to the beginning of our recording.
  if (last_active_string.empty())
    return;

  base::Time last_active_time;
  CHECK(StringToTime(last_active_string, &last_active_time));

  scoped_ptr<Event> event =
      util::CreateUncleanExitEvent(last_active_time, profile_name);

  database_->AddEvent(*event.get());
}

void PerformanceMonitor::CheckForVersionUpdateOnBackgroundThread() {
  CHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));

  chrome::VersionInfo version;
  DCHECK(version.is_valid());
  std::string current_version = version.Version();

  std::string previous_version = database_->GetStateValue(kStateChromeVersion);

  // We should never have a current_version which is older than the
  // previous_version.
  DCHECK(current_version >= previous_version);

  // If this is the first run, there will not be a stored value for Chrome
  // version; we insert the current version and will insert an event for the
  // next update of Chrome. If the previous version is older than the current
  // version, update the state in the database and insert an event.
  if (current_version > previous_version) {
    database_->AddStateValue(kStateChromeVersion, current_version);
    if (!previous_version.empty()) {
      scoped_ptr<Event> event = util::CreateChromeUpdateEvent(
          base::Time::Now(), previous_version, current_version);
      database_->AddEvent(*event.get());
    }
  }
}

void PerformanceMonitor::AddEvent(scoped_ptr<Event> event) {
  BrowserThread::PostBlockingPoolSequencedTask(
      Database::kDatabaseSequenceToken,
      FROM_HERE,
      base::Bind(&PerformanceMonitor::AddEventOnBackgroundThread,
                 base::Unretained(this),
                 base::Passed(&event)));
}

void PerformanceMonitor::AddEventOnBackgroundThread(scoped_ptr<Event> event) {
  database_->AddEvent(*event.get());
}

void PerformanceMonitor::AddMetricOnBackgroundThread(const Metric& metric) {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
  database_->AddMetric(metric);
}

void PerformanceMonitor::NotifyInitialized() {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PERFORMANCE_MONITOR_INITIALIZED,
      content::Source<PerformanceMonitor>(this),
      content::NotificationService::NoDetails());

  initialized_ = true;
}

void PerformanceMonitor::GatherStatisticsOnBackgroundThread() {
  CHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Because the CPU usage is gathered as an average since the last time the
  // function was called, while the memory usage is gathered as an instantaneous
  // usage, the CPU usage is gathered before the metrics map is wiped.
  GatherCPUUsageOnBackgroundThread();
  UpdateMetricsMapOnBackgroundThread();
  GatherMemoryUsageOnBackgroundThread();
}

void PerformanceMonitor::GatherCPUUsageOnBackgroundThread() {
  if (metrics_map_->size()) {
    double cpu_usage = 0;
    for (MetricsMap::const_iterator iter = metrics_map_->begin();
         iter != metrics_map_->end(); ++iter) {
      cpu_usage += iter->second->GetCPUUsage();
    }

    database_->AddMetric(Metric(METRIC_CPU_USAGE,
                                base::Time::Now(),
                                cpu_usage));
  }
}

void PerformanceMonitor::GatherMemoryUsageOnBackgroundThread() {
  size_t private_memory_sum = 0;
  size_t shared_memory_sum = 0;
  for (MetricsMap::const_iterator iter = metrics_map_->begin();
       iter != metrics_map_->end(); ++iter) {
    size_t private_memory = 0;
    size_t shared_memory = 0;
    if (iter->second->GetMemoryBytes(&private_memory, &shared_memory)) {
      private_memory_sum += private_memory;
      shared_memory_sum += shared_memory;
    } else {
      LOG(WARNING) << "GetMemoryBytes returned NULL (platform-specific error)";
    }
  }

  database_->AddMetric(Metric(METRIC_PRIVATE_MEMORY_USAGE,
                              base::Time::Now(),
                              static_cast<double>(private_memory_sum)));
  database_->AddMetric(Metric(METRIC_SHARED_MEMORY_USAGE,
                              base::Time::Now(),
                              static_cast<double>(shared_memory_sum)));
}

void PerformanceMonitor::UpdateMetricsMapOnBackgroundThread() {
  MetricsMap* new_map = new MetricsMap;

  base::ProcessId browser_pid = base::GetCurrentProcId();
  ChromeProcessList chrome_processes(GetRunningChromeProcesses(browser_pid));
  for (ChromeProcessList::const_iterator pid_iter = chrome_processes.begin();
       pid_iter != chrome_processes.end(); ++pid_iter) {
    base::ProcessHandle handle;
    if (base::OpenProcessHandleWithAccess(*pid_iter, kAccessFlags, &handle)) {
      // If we were already watching this process, transfer it to the new map.
      if (ContainsKey(*metrics_map_, handle)) {
        (*new_map)[handle] = (*metrics_map_)[handle];
        continue;
      }

      // Otherwise, gather information and prime the CPU usage to be gathered.
#if defined (OS_MACOSX)
      linked_ptr<base::ProcessMetrics> process_metrics(
          base::ProcessMetrics::CreateProcessMetrics(
              handle, content::BrowserChildProcessHost::GetPortProvider()));
#else
      linked_ptr<base::ProcessMetrics> process_metrics(
          base::ProcessMetrics::CreateProcessMetrics(handle));
#endif

      process_metrics->GetCPUUsage();

      (*new_map)[handle] = process_metrics;
    }
  }

  metrics_map_.reset(new_map);
}

void PerformanceMonitor::UpdateLiveProfiles() {
  std::string time = TimeToString(base::Time::Now());
  scoped_ptr<std::set<std::string> > active_profiles(
      new std::set<std::string>());

  for (chrome::BrowserIterator it; !it.done(); it.Next())
    active_profiles->insert(it->profile()->GetDebugName());

  BrowserThread::PostBlockingPoolSequencedTask(
      Database::kDatabaseSequenceToken,
      FROM_HERE,
      base::Bind(&PerformanceMonitor::UpdateLiveProfilesHelper,
                 base::Unretained(this),
                 base::Passed(&active_profiles),
                 time));
}

void PerformanceMonitor::UpdateLiveProfilesHelper(
    scoped_ptr<std::set<std::string> > active_profiles,
    std::string time) {
  CHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));

  for (std::set<std::string>::const_iterator iter = active_profiles->begin();
       iter != active_profiles->end(); ++iter) {
    database_->AddStateValue(kStateProfilePrefix + *iter, time);
  }
}

void PerformanceMonitor::DoTimedCollections() {
  UpdateLiveProfiles();

  BrowserThread::PostBlockingPoolSequencedTask(
      Database::kDatabaseSequenceToken,
      FROM_HERE,
      base::Bind(&PerformanceMonitor::GatherStatisticsOnBackgroundThread,
                 base::Unretained(this)));

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&PerformanceMonitor::CallInsertIOData,
                 base::Unretained(this)));
}

void PerformanceMonitor::CallInsertIOData() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  BrowserThread::PostBlockingPoolSequencedTask(
      Database::kDatabaseSequenceToken,
      FROM_HERE,
      base::Bind(&PerformanceMonitor::InsertIOData,
                 base::Unretained(this),
                 performance_data_for_io_thread_));
}

void PerformanceMonitor::InsertIOData(
    const PerformanceDataForIOThread& performance_data_for_io_thread) {
  CHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
  database_->AddMetric(
      Metric(METRIC_NETWORK_BYTES_READ,
             base::Time::Now(),
             static_cast<double>(
                 performance_data_for_io_thread.network_bytes_read)));
}

void PerformanceMonitor::BytesReadOnIOThread(const net::URLRequest& request,
                                             const int bytes_read) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (initialized_ && !request.url().SchemeIsFile())
    performance_data_for_io_thread_.network_bytes_read += bytes_read;
}

void PerformanceMonitor::Observe(int type,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_INSTALLED: {
      AddExtensionEvent(EVENT_EXTENSION_INSTALL,
                        content::Details<Extension>(details).ptr());
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_ENABLED: {
      AddExtensionEvent(EVENT_EXTENSION_ENABLE,
                        content::Details<Extension>(details).ptr());
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_UNLOADED: {
      const extensions::UnloadedExtensionInfo* info =
          content::Details<extensions::UnloadedExtensionInfo>(details).ptr();

      // Check if the extension was unloaded because it was disabled.
      if (info->reason == extension_misc::UNLOAD_REASON_DISABLE) {
        AddExtensionEvent(EVENT_EXTENSION_DISABLE,
                          info->extension);
      }
      break;
    }
    case chrome::NOTIFICATION_CRX_INSTALLER_DONE: {
      const extensions::CrxInstaller* installer =
          content::Source<extensions::CrxInstaller>(source).ptr();
      const extensions::Extension* extension =
          content::Details<Extension>(details).ptr();

      // Check if the reason for the install was due to a successful
      // extension update. |extension| is NULL in case of install failure.
      if (extension &&
          installer->install_cause() == extension_misc::INSTALL_CAUSE_UPDATE) {
        AddExtensionEvent(EVENT_EXTENSION_UPDATE, extension);
      }
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_UNINSTALLED: {
      AddExtensionEvent(EVENT_EXTENSION_UNINSTALL,
                        content::Details<Extension>(details).ptr());
      break;
    }
    case content::NOTIFICATION_RENDERER_PROCESS_HANG: {
      std::string url;
      content::RenderWidgetHost* widget =
          content::Source<content::RenderWidgetHost>(source).ptr();
      if (widget->IsRenderView()) {
        content::RenderViewHost* view = content::RenderViewHost::From(widget);
        MaybeGetURLFromRenderView(view, &url);
      }
      AddEvent(util::CreateRendererFailureEvent(base::Time::Now(),
                                                EVENT_RENDERER_HANG,
                                                url));
      break;
    }
    case content::NOTIFICATION_RENDERER_PROCESS_CLOSED: {
      AddRendererClosedEvent(
          content::Source<content::RenderProcessHost>(source).ptr(),
          *content::Details<content::RenderProcessHost::RendererClosedDetails>(
              details).ptr());
      break;
    }
    case chrome::NOTIFICATION_PROFILE_ADDED: {
      Profile* profile = content::Source<Profile>(source).ptr();
      if (profile->GetLastSessionExitType() == Profile::EXIT_CRASHED) {
        BrowserThread::PostBlockingPoolSequencedTask(
            Database::kDatabaseSequenceToken,
            FROM_HERE,
            base::Bind(
                &PerformanceMonitor::AddUncleanExitEventOnBackgroundThread,
                base::Unretained(this),
                profile->GetDebugName()));
      }
      break;
    }
    case content::NOTIFICATION_LOAD_STOP: {
      const content::LoadNotificationDetails* load_details =
          content::Details<content::LoadNotificationDetails>(details).ptr();
      if (!load_details)
        break;
      BrowserThread::PostBlockingPoolSequencedTask(
          Database::kDatabaseSequenceToken,
          FROM_HERE,
          base::Bind(
              &PerformanceMonitor::AddMetricOnBackgroundThread,
              base::Unretained(this),
              Metric(METRIC_PAGE_LOAD_TIME,
                     base::Time::Now(),
                     static_cast<double>(
                         load_details->load_time.ToInternalValue()))));
      break;
    }
    default: {
      NOTREACHED();
      break;
    }
  }
}

void PerformanceMonitor::AddExtensionEvent(EventType type,
                                              const Extension* extension) {
  DCHECK(type == EVENT_EXTENSION_INSTALL ||
         type == EVENT_EXTENSION_UNINSTALL ||
         type == EVENT_EXTENSION_UPDATE ||
         type == EVENT_EXTENSION_ENABLE ||
         type == EVENT_EXTENSION_DISABLE);
  AddEvent(util::CreateExtensionEvent(type,
                                      base::Time::Now(),
                                      extension->id(),
                                      extension->name(),
                                      extension->url().spec(),
                                      extension->location(),
                                      extension->VersionString(),
                                      extension->description()));
}

void PerformanceMonitor::AddRendererClosedEvent(
    content::RenderProcessHost* host,
    const content::RenderProcessHost::RendererClosedDetails& details) {
  // We only care if this is an invalid termination.
  if (details.status == base::TERMINATION_STATUS_NORMAL_TERMINATION ||
      details.status == base::TERMINATION_STATUS_STILL_RUNNING)
    return;

  // Determine the type of crash.
  EventType type =
      details.status == base::TERMINATION_STATUS_PROCESS_WAS_KILLED ?
      EVENT_RENDERER_KILLED : EVENT_RENDERER_CRASH;

  content::RenderProcessHost::RenderWidgetHostsIterator iter =
      host->GetRenderWidgetHostsIterator();

  // A RenderProcessHost may contain multiple render views - for each valid
  // render view, extract the url, and append it to the string, comma-separating
  // the entries.
  std::string url_list;
  for (; !iter.IsAtEnd(); iter.Advance()) {
    const content::RenderWidgetHost* widget = iter.GetCurrentValue();
    DCHECK(widget);
    if (!widget || !widget->IsRenderView())
      continue;

    content::RenderViewHost* view =
        content::RenderViewHost::From(
            const_cast<content::RenderWidgetHost*>(widget));

    std::string url;
    if (!MaybeGetURLFromRenderView(view, &url))
      continue;

    if (!url_list.empty())
      url_list += ", ";

    url_list += url;
  }

  AddEvent(util::CreateRendererFailureEvent(base::Time::Now(), type, url_list));
}

}  // namespace performance_monitor
