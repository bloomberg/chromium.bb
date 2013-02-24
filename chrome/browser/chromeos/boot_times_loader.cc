// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/boot_times_loader.h"

#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/metrics/histogram.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/authentication_notification_details.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_iterator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"

using content::BrowserThread;
using content::NavigationController;
using content::RenderWidgetHost;
using content::RenderWidgetHostView;
using content::WebContents;

namespace {

RenderWidgetHost* GetRenderWidgetHost(NavigationController* tab) {
  WebContents* web_contents = tab->GetWebContents();
  if (web_contents) {
    RenderWidgetHostView* render_widget_host_view =
        web_contents->GetRenderWidgetHostView();
    if (render_widget_host_view)
      return render_widget_host_view->GetRenderWidgetHost();
  }
  return NULL;
}

const std::string GetTabUrl(RenderWidgetHost* rwh) {
  RenderWidgetHostView* rwhv = rwh->GetView();
  for (chrome::BrowserIterator it; !it.done(); it.Next()) {
    Browser* browser = *it;
    for (int i = 0, tab_count = browser->tab_strip_model()->count();
         i < tab_count;
         ++i) {
      WebContents* tab = browser->tab_strip_model()->GetWebContentsAt(i);
      if (tab->GetRenderWidgetHostView() == rwhv) {
        return tab->GetURL().spec();
      }
    }
  }
  return std::string();
}

void PostCallbackIfNotCanceled(
    const CancelableTaskTracker::IsCanceledCallback& is_canceled_cb,
    base::TaskRunner* task_runner,
    const chromeos::BootTimesLoader::GetBootTimesCallback& callback,
    const chromeos::BootTimesLoader::BootTimes& boot_times) {
  if (is_canceled_cb.Run())
    return;
  task_runner->PostTask(FROM_HERE, base::Bind(callback, boot_times));
}

}  // namespace

namespace chromeos {

#define FPL(value) FILE_PATH_LITERAL(value)

// Dir uptime & disk logs are located in.
static const base::FilePath::CharType kLogPath[] = FPL("/tmp");
// Dir log{in,out} logs are located in.
static const base::FilePath::CharType kLoginLogPath[] =
    FPL("/home/chronos/user");
// Prefix for the time measurement files.
static const base::FilePath::CharType kUptimePrefix[] = FPL("uptime-");
// Prefix for the disk usage files.
static const base::FilePath::CharType kDiskPrefix[] = FPL("disk-");
// Name of the time that Chrome's main() is called.
static const base::FilePath::CharType kChromeMain[] = FPL("chrome-main");
// Delay in milliseconds between file read attempts.
static const int64 kReadAttemptDelayMs = 250;
// Delay in milliseconds before writing the login times to disk.
static const int64 kLoginTimeWriteDelayMs = 3000;

// Names of login stats files.
static const base::FilePath::CharType kLoginSuccess[] = FPL("login-success");
static const base::FilePath::CharType kChromeFirstRender[] =
    FPL("chrome-first-render");

// Names of login UMA values.
static const char kUmaLogin[] = "BootTime.Login";
static const char kUmaLoginPrefix[] = "BootTime.";
static const char kUmaLogout[] = "ShutdownTime.Logout";
static const char kUmaLogoutPrefix[] = "ShutdownTime.";

// Name of file collecting login times.
static const base::FilePath::CharType kLoginTimes[] = FPL("login-times");

// Name of file collecting logout times.
static const char kLogoutTimes[] = "logout-times";

static base::LazyInstance<BootTimesLoader> g_boot_times_loader =
    LAZY_INSTANCE_INITIALIZER;

BootTimesLoader::BootTimesLoader()
    : backend_(new Backend()),
      have_registered_(false) {
  login_time_markers_.reserve(30);
  logout_time_markers_.reserve(30);
}

BootTimesLoader::~BootTimesLoader() {}

// static
BootTimesLoader* BootTimesLoader::Get() {
  return g_boot_times_loader.Pointer();
}

CancelableTaskTracker::TaskId BootTimesLoader::GetBootTimes(
      const GetBootTimesCallback& callback,
      CancelableTaskTracker* tracker) {
  if (!BrowserThread::IsMessageLoopValid(BrowserThread::FILE)) {
    // This should only happen if Chrome is shutting down, so we don't do
    // anything.
    return CancelableTaskTracker::kBadTaskId;
  }

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kTestType)) {
    // TODO(davemoore) This avoids boottimes for tests. This needs to be
    // replaced with a mock of BootTimesLoader.
    return CancelableTaskTracker::kBadTaskId;
  }

  CancelableTaskTracker::IsCanceledCallback is_canceled;
  CancelableTaskTracker::TaskId id = tracker->NewTrackedTaskId(&is_canceled);

  GetBootTimesCallback callback_runner =
      base::Bind(&PostCallbackIfNotCanceled,
                 is_canceled, base::MessageLoopProxy::current(), callback);
  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&Backend::GetBootTimesAndRunCallback,
                 backend_, is_canceled, callback_runner));
  return id;
}

// Extracts the uptime value from files located in /tmp, returning the
// value as a double in value.
static bool GetTime(const base::FilePath::StringType& log, double* value) {
  base::FilePath log_dir(kLogPath);
  base::FilePath log_file = log_dir.Append(log);
  std::string contents;
  *value = 0.0;
  if (file_util::ReadFileToString(log_file, &contents)) {
    size_t space_index = contents.find(' ');
    size_t chars_left =
        space_index != std::string::npos ? space_index : std::string::npos;
    std::string value_string = contents.substr(0, chars_left);
    return base::StringToDouble(value_string, value);
  }
  return false;
}

// Converts double seconds to a TimeDelta object.
static base::TimeDelta SecondsToTimeDelta(double seconds) {
  double ms = seconds * base::Time::kMillisecondsPerSecond;
  return base::TimeDelta::FromMilliseconds(static_cast<int64>(ms));
}

// Reports the collected boot times to UMA if they haven't been
// reported yet and if metrics collection is enabled.
static void SendBootTimesToUMA(const BootTimesLoader::BootTimes& boot_times) {
  // Checks if the times for the most recent boot event have been
  // reported already to avoid sending boot time histogram samples
  // every time the user logs out.
  static const base::FilePath::CharType kBootTimesSent[] =
      FPL("/tmp/boot-times-sent");
  base::FilePath sent(kBootTimesSent);
  if (file_util::PathExists(sent))
    return;

  UMA_HISTOGRAM_TIMES("BootTime.Total",
                      SecondsToTimeDelta(boot_times.total));
  UMA_HISTOGRAM_TIMES("BootTime.Firmware",
                      SecondsToTimeDelta(boot_times.firmware));
  UMA_HISTOGRAM_TIMES("BootTime.Kernel",
                      SecondsToTimeDelta(boot_times.pre_startup));
  UMA_HISTOGRAM_TIMES("BootTime.System",
                      SecondsToTimeDelta(boot_times.system));
  if (boot_times.chrome > 0) {
    UMA_HISTOGRAM_TIMES("BootTime.Chrome",
                        SecondsToTimeDelta(boot_times.chrome));
  }

  // Stores the boot times to a file in /tmp to indicate that the
  // times for the most recent boot event have been reported
  // already. The file will be deleted at system shutdown/reboot.
  std::string boot_times_text = base::StringPrintf("total: %.2f\n"
                                                   "firmware: %.2f\n"
                                                   "kernel: %.2f\n"
                                                   "system: %.2f\n"
                                                   "chrome: %.2f\n",
                                                   boot_times.total,
                                                   boot_times.firmware,
                                                   boot_times.pre_startup,
                                                   boot_times.system,
                                                   boot_times.chrome);
  file_util::WriteFile(sent, boot_times_text.data(), boot_times_text.size());
  DCHECK(file_util::PathExists(sent));
}

void BootTimesLoader::Backend::GetBootTimesAndRunCallback(
    const CancelableTaskTracker::IsCanceledCallback& is_canceled_cb,
    const GetBootTimesCallback& callback) {
  if (is_canceled_cb.Run())
    return;

  const base::FilePath::CharType kFirmwareBootTime[] =
      FPL("firmware-boot-time");
  const base::FilePath::CharType kPreStartup[] = FPL("pre-startup");
  const base::FilePath::CharType kChromeExec[] = FPL("chrome-exec");
  const base::FilePath::CharType kChromeMain[] = FPL("chrome-main");
  const base::FilePath::CharType kXStarted[] = FPL("x-started");
  const base::FilePath::CharType kLoginPromptReady[] =
      FPL("login-prompt-ready");
  const base::FilePath::StringType uptime_prefix = kUptimePrefix;

  // Wait until firmware-boot-time file exists by reposting.
  base::FilePath log_dir(kLogPath);
  base::FilePath log_file = log_dir.Append(kFirmwareBootTime);
  if (!file_util::PathExists(log_file)) {
    BrowserThread::PostDelayedTask(
        BrowserThread::FILE,
        FROM_HERE,
        base::Bind(&Backend::GetBootTimesAndRunCallback,
                   this, is_canceled_cb, callback),
        base::TimeDelta::FromMilliseconds(kReadAttemptDelayMs));
    return;
  }

  BootTimes boot_times;

  GetTime(kFirmwareBootTime, &boot_times.firmware);
  GetTime(uptime_prefix + kPreStartup, &boot_times.pre_startup);
  GetTime(uptime_prefix + kXStarted, &boot_times.x_started);
  GetTime(uptime_prefix + kChromeExec, &boot_times.chrome_exec);
  GetTime(uptime_prefix + kChromeMain, &boot_times.chrome_main);
  GetTime(uptime_prefix + kLoginPromptReady, &boot_times.login_prompt_ready);

  boot_times.total = boot_times.firmware + boot_times.login_prompt_ready;
  if (boot_times.chrome_exec > 0) {
    boot_times.system = boot_times.chrome_exec - boot_times.pre_startup;
    boot_times.chrome = boot_times.login_prompt_ready - boot_times.chrome_exec;
  } else {
    boot_times.system = boot_times.login_prompt_ready - boot_times.pre_startup;
  }

  SendBootTimesToUMA(boot_times);

  callback.Run(boot_times);
}

// Appends the given buffer into the file. Returns the number of bytes
// written, or -1 on error.
// TODO(satorux): Move this to file_util.
static int AppendFile(const base::FilePath& file_path,
                      const char* data,
                      int size) {
  FILE* file = file_util::OpenFile(file_path, "a");
  if (!file) {
    return -1;
  }
  const int num_bytes_written = fwrite(data, 1, size, file);
  file_util::CloseFile(file);
  return num_bytes_written;
}

static void RecordStatsDelayed(const base::FilePath::StringType& name,
                               const std::string& uptime,
                               const std::string& disk) {
  const base::FilePath log_path(kLogPath);
  const base::FilePath uptime_output =
      log_path.Append(base::FilePath(kUptimePrefix + name));
  const base::FilePath disk_output =
      log_path.Append(base::FilePath(kDiskPrefix + name));

  // Append numbers to the files.
  AppendFile(uptime_output, uptime.data(), uptime.size());
  AppendFile(disk_output, disk.data(), disk.size());
}

// static
void BootTimesLoader::WriteTimes(
    const std::string base_name,
    const std::string uma_name,
    const std::string uma_prefix,
    std::vector<TimeMarker> login_times) {
  const int kMinTimeMillis = 1;
  const int kMaxTimeMillis = 30000;
  const int kNumBuckets = 100;
  const base::FilePath log_path(kLoginLogPath);

  // Need to sort by time since the entries may have been pushed onto the
  // vector (on the UI thread) in a different order from which they were
  // created (potentially on other threads).
  std::sort(login_times.begin(), login_times.end());

  base::Time first = login_times.front().time();
  base::Time last = login_times.back().time();
  base::TimeDelta total = last - first;
  base::HistogramBase* total_hist = base::Histogram::FactoryTimeGet(
      uma_name,
      base::TimeDelta::FromMilliseconds(kMinTimeMillis),
      base::TimeDelta::FromMilliseconds(kMaxTimeMillis),
      kNumBuckets,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  total_hist->AddTime(total);
  std::string output =
      base::StringPrintf("%s: %.2f", uma_name.c_str(), total.InSecondsF());
  base::Time prev = first;
  for (unsigned int i = 0; i < login_times.size(); ++i) {
    TimeMarker tm = login_times[i];
    base::TimeDelta since_first = tm.time() - first;
    base::TimeDelta since_prev = tm.time() - prev;
    std::string name;

    if (tm.send_to_uma()) {
      name = uma_prefix + tm.name();
      base::HistogramBase* prev_hist = base::Histogram::FactoryTimeGet(
          name,
          base::TimeDelta::FromMilliseconds(kMinTimeMillis),
          base::TimeDelta::FromMilliseconds(kMaxTimeMillis),
          kNumBuckets,
          base::HistogramBase::kUmaTargetedHistogramFlag);
      prev_hist->AddTime(since_prev);
    } else {
      name = tm.name();
    }
    output +=
        StringPrintf(
            "\n%.2f +%.4f %s",
            since_first.InSecondsF(),
            since_prev.InSecondsF(),
            name.data());
    prev = tm.time();
  }
  output += '\n';

  file_util::WriteFile(
      log_path.Append(base_name), output.data(), output.size());
}

void BootTimesLoader::LoginDone() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  AddLoginTimeMarker("LoginDone", false);
  RecordCurrentStats(kChromeFirstRender);
  registrar_.Remove(this, content::NOTIFICATION_LOAD_START,
                    content::NotificationService::AllSources());
  registrar_.Remove(this, content::NOTIFICATION_LOAD_STOP,
                    content::NotificationService::AllSources());
  registrar_.Remove(this, content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                    content::NotificationService::AllSources());
  registrar_.Remove(
      this,
      content::NOTIFICATION_RENDER_WIDGET_HOST_DID_UPDATE_BACKING_STORE,
      content::NotificationService::AllSources());
  // Don't swamp the FILE thread right away.
  BrowserThread::PostDelayedTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&WriteTimes, kLoginTimes, kUmaLogin, kUmaLoginPrefix,
                 login_time_markers_),
      base::TimeDelta::FromMilliseconds(kLoginTimeWriteDelayMs));
}

void BootTimesLoader::WriteLogoutTimes() {
  // Either we're on the browser thread, or (more likely) Chrome is in the
  // process of shutting down and we're on the main thread but the message loop
  // has already been terminated.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
         !BrowserThread::IsMessageLoopValid(BrowserThread::UI));

  WriteTimes(kLogoutTimes,
             kUmaLogout,
             kUmaLogoutPrefix,
             logout_time_markers_);
}

void BootTimesLoader::RecordStats(const std::string& name, const Stats& stats) {
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&RecordStatsDelayed, name, stats.uptime, stats.disk));
}

BootTimesLoader::Stats BootTimesLoader::GetCurrentStats() {
  const base::FilePath kProcUptime(FPL("/proc/uptime"));
  const base::FilePath kDiskStat(FPL("/sys/block/sda/stat"));
  Stats stats;
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  file_util::ReadFileToString(kProcUptime, &stats.uptime);
  file_util::ReadFileToString(kDiskStat, &stats.disk);
  return stats;
}

void BootTimesLoader::RecordCurrentStats(const std::string& name) {
  RecordStats(name, GetCurrentStats());
}

void BootTimesLoader::SaveChromeMainStats() {
  chrome_main_stats_ = GetCurrentStats();
}

void BootTimesLoader::RecordChromeMainStats() {
  RecordStats(kChromeMain, chrome_main_stats_);
}

void BootTimesLoader::RecordLoginAttempted() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  login_time_markers_.clear();
  AddLoginTimeMarker("LoginStarted", false);
  if (!have_registered_) {
    have_registered_ = true;
    registrar_.Add(this, chrome::NOTIFICATION_LOGIN_AUTHENTICATION,
                   content::NotificationService::AllSources());
    registrar_.Add(this, content::NOTIFICATION_LOAD_START,
                   content::NotificationService::AllSources());
    registrar_.Add(this, content::NOTIFICATION_LOAD_STOP,
                   content::NotificationService::AllSources());
    registrar_.Add(this, content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                   content::NotificationService::AllSources());
    registrar_.Add(
        this,
        content::NOTIFICATION_RENDER_WIDGET_HOST_DID_UPDATE_BACKING_STORE,
        content::NotificationService::AllSources());
  }
}

void BootTimesLoader::AddLoginTimeMarker(
    const std::string& marker_name, bool send_to_uma) {
  AddMarker(&login_time_markers_, TimeMarker(marker_name, send_to_uma));
}

void BootTimesLoader::AddLogoutTimeMarker(
    const std::string& marker_name, bool send_to_uma) {
  AddMarker(&logout_time_markers_, TimeMarker(marker_name, send_to_uma));
}

// static
void BootTimesLoader::AddMarker(std::vector<TimeMarker>* vector,
                                TimeMarker marker)
{
  // The marker vectors can only be safely manipulated on the main thread.
  // If we're late in the process of shutting down (eg. as can be the case at
  // logout), then we have to assume we're on the main thread already.
  if (BrowserThread::CurrentlyOn(BrowserThread::UI) ||
      !BrowserThread::IsMessageLoopValid(BrowserThread::UI)) {
    vector->push_back(marker);
  } else {
    // Add the marker on the UI thread.
    // Note that it's safe to use an unretained pointer to the vector because
    // BootTimesLoader's lifetime exceeds that of the UI thread message loop.
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&BootTimesLoader::AddMarker,
                   base::Unretained(vector),
                   marker));
  }
}

void BootTimesLoader::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_LOGIN_AUTHENTICATION: {
      content::Details<AuthenticationNotificationDetails> auth_details(details);
      if (auth_details->success()) {
        AddLoginTimeMarker("Authenticate", true);
        RecordCurrentStats(kLoginSuccess);
        registrar_.Remove(this, chrome::NOTIFICATION_LOGIN_AUTHENTICATION,
                          content::NotificationService::AllSources());
      }
      break;
    }
    case content::NOTIFICATION_LOAD_START: {
      NavigationController* tab =
          content::Source<NavigationController>(source).ptr();
      RenderWidgetHost* rwh = GetRenderWidgetHost(tab);
      DCHECK(rwh);
      AddLoginTimeMarker("TabLoad-Start: " + GetTabUrl(rwh), false);
      render_widget_hosts_loading_.insert(rwh);
      break;
    }
    case content::NOTIFICATION_LOAD_STOP: {
      NavigationController* tab =
          content::Source<NavigationController>(source).ptr();
      RenderWidgetHost* rwh = GetRenderWidgetHost(tab);
      if (render_widget_hosts_loading_.find(rwh) !=
          render_widget_hosts_loading_.end()) {
        AddLoginTimeMarker("TabLoad-End: " + GetTabUrl(rwh), false);
      }
      break;
    }
    case content::NOTIFICATION_RENDER_WIDGET_HOST_DID_UPDATE_BACKING_STORE: {
      RenderWidgetHost* rwh = content::Source<RenderWidgetHost>(source).ptr();
      if (render_widget_hosts_loading_.find(rwh) !=
          render_widget_hosts_loading_.end()) {
        AddLoginTimeMarker("TabPaint: " + GetTabUrl(rwh), false);
        LoginDone();
      }
      break;
    }
    case content::NOTIFICATION_WEB_CONTENTS_DESTROYED: {
      WebContents* web_contents = content::Source<WebContents>(source).ptr();
      RenderWidgetHost* render_widget_host =
          GetRenderWidgetHost(&web_contents->GetController());
      render_widget_hosts_loading_.erase(render_widget_host);
      break;
    }
    default:
      break;
  }
}

}  // namespace chromeos
