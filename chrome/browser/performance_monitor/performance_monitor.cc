// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/performance_monitor.h"

#include <set>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/process/process_iterator.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/worker_pool.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/performance_monitor/constants.h"
#include "chrome/browser/performance_monitor/performance_monitor_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_iterator.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/load_notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension.h"
#include "net/url_request/url_request.h"

using content::BrowserThread;
using extensions::Extension;
using extensions::UnloadedExtensionInfo;

namespace performance_monitor {

namespace {

#if !defined(OS_ANDROID)
std::string TimeToString(base::Time time) {
  int64 time_int64 = time.ToInternalValue();
  return base::Int64ToString(time_int64);
}
#endif  // !defined(OS_ANDROID)

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

// Takes ownership of and deletes |database| on the background thread, to
// avoid destruction in the middle of an operation.
void DeleteDatabaseOnBackgroundThread(Database* database) {
  delete database;
}

}  // namespace

bool PerformanceMonitor::initialized_ = false;

PerformanceMonitor::PerformanceDataForIOThread::PerformanceDataForIOThread()
    : network_bytes_read(0) {
}

PerformanceMonitor::PerformanceMonitor()
    : gather_interval_in_seconds_(kDefaultGatherIntervalInSeconds),
      database_logging_enabled_(false),
      timer_(FROM_HERE,
             base::TimeDelta::FromSeconds(kSampleIntervalInSeconds),
             this,
             &PerformanceMonitor::DoTimedCollections),
      disable_timer_autostart_for_testing_(false) {
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

void PerformanceMonitor::Initialize() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kPerformanceMonitorGathering)) {
    database_logging_enabled_ = true;

    std::string switch_value = CommandLine::ForCurrentProcess()->
        GetSwitchValueASCII(switches::kPerformanceMonitorGathering);

    if (!switch_value.empty()) {
      int specified_interval = 0;
      if (!base::StringToInt(switch_value, &specified_interval) ||
          specified_interval <= 0) {
        LOG(ERROR) << "Invalid value for switch: '"
                   << switches::kPerformanceMonitorGathering
                   << "'; please use an integer greater than 0.";
      } else {
        gather_interval_in_seconds_ = std::max(specified_interval,
                                               kSampleIntervalInSeconds);
      }
    }
  }

  DCHECK(gather_interval_in_seconds_ >= kSampleIntervalInSeconds);

  next_collection_time_ = base::Time::Now() +
      base::TimeDelta::FromSeconds(gather_interval_in_seconds_);

  util::PostTaskToDatabaseThreadAndReply(
      FROM_HERE,
      base::Bind(&PerformanceMonitor::InitOnBackgroundThread,
                 base::Unretained(this)),
      base::Bind(&PerformanceMonitor::FinishInit,
                 base::Unretained(this)));
}

void PerformanceMonitor::InitOnBackgroundThread() {
  CHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (database_logging_enabled_) {
    if (!database_)
      database_ = Database::Create(database_path_);

    if (!database_) {
      LOG(ERROR) << "Could not initialize database; aborting initialization.";
      database_logging_enabled_ = false;
      return;
    }

    // Initialize the io thread's performance data to the value in the database;
    // if there isn't a recording in the database, the value stays at 0.
    Metric metric;
    if (database_->GetRecentStatsForActivityAndMetric(METRIC_NETWORK_BYTES_READ,
                                                      &metric)) {
      performance_data_for_io_thread_.network_bytes_read = metric.value;
    }
  }
}

void PerformanceMonitor::FinishInit() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Events and notifications are only useful if we're logging to the database.
  if (database_logging_enabled_) {
    RegisterForNotifications();
    CheckForUncleanExits();
    BrowserThread::PostBlockingPoolSequencedTask(
        Database::kDatabaseSequenceToken,
        FROM_HERE,
        base::Bind(&PerformanceMonitor::CheckForVersionUpdateOnBackgroundThread,
                   base::Unretained(this)));
  }

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

void PerformanceMonitor::StartGatherCycle() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Start our periodic gathering of metrics.
  if (!disable_timer_autostart_for_testing_)
    timer_.Reset();
}

void PerformanceMonitor::RegisterForNotifications() {
  DCHECK(database_logging_enabled_);

  // Extensions
  registrar_.Add(this,
                 chrome::NOTIFICATION_EXTENSION_WILL_BE_INSTALLED_DEPRECATED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_ENABLED,
      content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED_DEPRECATED,
      content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_CRX_INSTALLER_DONE,
      content::NotificationService::AllSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_EXTENSION_UNINSTALLED_DEPRECATED,
                 content::NotificationService::AllSources());

  // Crashes
  registrar_.Add(this, content::NOTIFICATION_RENDER_WIDGET_HOST_HANG,
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
  DCHECK(database_logging_enabled_);

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
  DCHECK(database_logging_enabled_);
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
  DCHECK(database_logging_enabled_);

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
  DCHECK(database_logging_enabled_);

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
  DCHECK(database_logging_enabled_);

  database_->AddMetric(metric);
}

void PerformanceMonitor::NotifyInitialized() {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PERFORMANCE_MONITOR_INITIALIZED,
      content::Source<PerformanceMonitor>(this),
      content::NotificationService::NoDetails());

  initialized_ = true;
}

void PerformanceMonitor::DoTimedCollections() {
#if !defined(OS_ANDROID)
  // The profile list is only useful for the logged events.
  if (database_logging_enabled_)
    UpdateLiveProfiles();
#endif

  GatherMetricsMapOnUIThread();
}

void PerformanceMonitor::GatherMetricsMapOnUIThread() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  static int current_update_sequence = 0;
  // Even in the "somewhat" unlikely event this wraps around,
  // it doesn't matter. We just check it for inequality.
  current_update_sequence++;

  // Find all render child processes; has to be done on the UI thread.
  for (content::RenderProcessHost::iterator rph_iter =
           content::RenderProcessHost::AllHostsIterator();
       !rph_iter.IsAtEnd(); rph_iter.Advance()) {
    base::ProcessHandle handle = rph_iter.GetCurrentValue()->GetHandle();
    MarkProcessAsAlive(handle, content::PROCESS_TYPE_RENDERER,
                       current_update_sequence);
  }

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&PerformanceMonitor::GatherMetricsMapOnIOThread,
                 base::Unretained(this),
                 current_update_sequence));
}

void PerformanceMonitor::MarkProcessAsAlive(const base::ProcessHandle& handle,
                                        int process_type,
                                        int current_update_sequence) {

  if (handle == 0) {
    // Process may not be valid yet.
    return;
  }

  MetricsMap::iterator process_metrics_iter = metrics_map_.find(handle);
  if (process_metrics_iter == metrics_map_.end()) {
    // If we're not already watching the process, let's initialize it.
    metrics_map_[handle]
        .Initialize(handle, process_type, current_update_sequence);
  } else {
    // If we are watching the process, touch it to keep it alive.
    ProcessMetricsHistory& process_metrics = process_metrics_iter->second;
    process_metrics.set_last_update_sequence(current_update_sequence);
  }
}

#if !defined(OS_ANDROID)
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
  DCHECK(database_logging_enabled_);

  for (std::set<std::string>::const_iterator iter = active_profiles->begin();
       iter != active_profiles->end(); ++iter) {
    database_->AddStateValue(kStateProfilePrefix + *iter, time);
  }
}
#endif

void PerformanceMonitor::GatherMetricsMapOnIOThread(
    int current_update_sequence) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Find all child processes (does not include renderers), which has to be
  // done on the IO thread.
  for (content::BrowserChildProcessHostIterator iter; !iter.Done(); ++iter) {
    const content::ChildProcessData& child_process_data = iter.GetData();
    base::ProcessHandle handle = child_process_data.handle;
    MarkProcessAsAlive(handle, child_process_data.process_type,
                       current_update_sequence);
  }

  // Add the current (browser) process.
  MarkProcessAsAlive(base::GetCurrentProcessHandle(),
                     content::PROCESS_TYPE_BROWSER, current_update_sequence);

  BrowserThread::PostBlockingPoolSequencedTask(
      Database::kDatabaseSequenceToken,
      FROM_HERE,
      base::Bind(&PerformanceMonitor::StoreMetricsOnBackgroundThread,
                 base::Unretained(this), current_update_sequence,
                 performance_data_for_io_thread_));
}

void PerformanceMonitor::StoreMetricsOnBackgroundThread(
    int current_update_sequence,
    const PerformanceDataForIOThread& performance_data_for_io_thread) {
  CHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));

  base::Time time_now = base::Time::Now();

  // The timing can be off by kSampleIntervalInSeconds during any one particular
  // run, but will be correct over time.
  bool end_of_cycle = time_now >= next_collection_time_;
  if (end_of_cycle) {
    next_collection_time_ +=
        base::TimeDelta::FromSeconds(gather_interval_in_seconds_);
  }

  double cpu_usage = 0.0;
  size_t private_memory_sum = 0;
  size_t shared_memory_sum = 0;

  // Update metrics for all watched processes; remove dead entries from the map.
  MetricsMap::iterator iter = metrics_map_.begin();
  while (iter != metrics_map_.end()) {
    ProcessMetricsHistory& process_metrics = iter->second;
    if (process_metrics.last_update_sequence() != current_update_sequence) {
      // Not touched this iteration; let's get rid of it.
      metrics_map_.erase(iter++);
    } else {
      process_metrics.SampleMetrics();

      if (end_of_cycle) {
        // Gather averages of previously sampled metrics.
        cpu_usage += process_metrics.GetAverageCPUUsage();

        size_t private_memory = 0;
        size_t shared_memory = 0;
        process_metrics.GetAverageMemoryBytes(&private_memory, &shared_memory);
        private_memory_sum += private_memory;
        shared_memory_sum += shared_memory;

        process_metrics.EndOfCycle();
      }

      ++iter;
    }
  }

  // Store previously-sampled metrics.
  if (end_of_cycle && database_logging_enabled_) {
    if (!metrics_map_.empty()) {
      database_->AddMetric(Metric(METRIC_CPU_USAGE, time_now, cpu_usage));
      database_->AddMetric(Metric(METRIC_PRIVATE_MEMORY_USAGE,
                                  time_now,
                                  static_cast<double>(private_memory_sum)));
      database_->AddMetric(Metric(METRIC_SHARED_MEMORY_USAGE,
                                  time_now,
                                  static_cast<double>(shared_memory_sum)));
    }

    database_->AddMetric(
        Metric(METRIC_NETWORK_BYTES_READ,
               time_now,
               static_cast<double>(
                   performance_data_for_io_thread.network_bytes_read)));
  }

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&PerformanceMonitor::StartGatherCycle,
                 base::Unretained(this)));
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
  DCHECK(database_logging_enabled_);

  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_WILL_BE_INSTALLED_DEPRECATED: {
      AddExtensionEvent(
          EVENT_EXTENSION_INSTALL,
          content::Details<const extensions::InstalledExtensionInfo>(details)->
              extension);
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_ENABLED: {
      AddExtensionEvent(EVENT_EXTENSION_ENABLE,
                        content::Details<Extension>(details).ptr());
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_UNLOADED_DEPRECATED: {
      const UnloadedExtensionInfo* info =
          content::Details<UnloadedExtensionInfo>(details).ptr();

      // Check if the extension was unloaded because it was disabled.
      if (info->reason == UnloadedExtensionInfo::REASON_DISABLE) {
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
    case chrome::NOTIFICATION_EXTENSION_UNINSTALLED_DEPRECATED: {
      AddExtensionEvent(EVENT_EXTENSION_UNINSTALL,
                        content::Details<Extension>(details).ptr());
      break;
    }
    case content::NOTIFICATION_RENDER_WIDGET_HOST_HANG: {
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

  // A RenderProcessHost may contain multiple render views - for each valid
  // render view, extract the url, and append it to the string, comma-separating
  // the entries.
  std::string url_list;
  scoped_ptr<content::RenderWidgetHostIterator> widgets(
      content::RenderWidgetHost::GetRenderWidgetHosts());
  while (content::RenderWidgetHost* widget = widgets->GetNextHost()) {
    if (widget->GetProcess()->GetID() != host->GetID())
      continue;
    if (!widget->IsRenderView())
      continue;

    content::RenderViewHost* view = content::RenderViewHost::From(widget);
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
