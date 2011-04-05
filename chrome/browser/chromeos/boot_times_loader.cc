// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/boot_times_loader.h"

#include <vector>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/lazy_instance.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/process_util.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/authentication_notification_details.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/network_state_notifier.h"
#include "chrome/common/chrome_switches.h"
#include "content/browser/browser_thread.h"
#include "content/common/notification_service.h"

namespace chromeos {

#define FPL(value) FILE_PATH_LITERAL(value)

// File uptime logs are located in.
static const FilePath::CharType kLogPath[] = FPL("/tmp");
// Prefix for the time measurement files.
static const FilePath::CharType kUptimePrefix[] = FPL("uptime-");
// Prefix for the disk usage files.
static const FilePath::CharType kDiskPrefix[] = FPL("disk-");
// Name of the time that Chrome's main() is called.
static const FilePath::CharType kChromeMain[] = FPL("chrome-main");
// Delay in milliseconds between file read attempts.
static const int64 kReadAttemptDelayMs = 250;
// Delay in milliseconds before writing the login times to disk.
static const int64 kLoginTimeWriteDelayMs = 3000;

// Names of login stats files.
static const FilePath::CharType kLoginSuccess[] = FPL("login-success");
static const FilePath::CharType kChromeFirstRender[] =
    FPL("chrome-first-render");

// Names of login UMA values.
static const char kUmaAuthenticate[] = "BootTime.Authenticate";
static const char kUmaLogin[] = "BootTime.Login";
static const char kUmaLoginPrefix[] = "BootTime.";
static const char kUmaLogout[] = "ShutdownTime.Logout";
static const char kUmaLogoutPrefix[] = "ShutdownTime.";

// Name of file collecting login times.
static const FilePath::CharType kLoginTimes[] = FPL("login-times-sent");

// Name of file collecting logout times.
static const char kLogoutTimes[] = "logout-times-sent";

static base::LazyInstance<BootTimesLoader> g_boot_times_loader(
    base::LINKER_INITIALIZED);

BootTimesLoader::BootTimesLoader()
    : backend_(new Backend()),
      have_registered_(false) {
  login_time_markers_.reserve(30);
  logout_time_markers_.reserve(30);
}

// static
BootTimesLoader* BootTimesLoader::Get() {
  return g_boot_times_loader.Pointer();
}

BootTimesLoader::Handle BootTimesLoader::GetBootTimes(
    CancelableRequestConsumerBase* consumer,
    BootTimesLoader::GetBootTimesCallback* callback) {
  if (!g_browser_process->file_thread()) {
    // This should only happen if Chrome is shutting down, so we don't do
    // anything.
    return 0;
  }

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kTestType)) {
    // TODO(davemoore) This avoids boottimes for tests. This needs to be
    // replaced with a mock of BootTimesLoader.
    return 0;
  }

  scoped_refptr<CancelableRequest<GetBootTimesCallback> > request(
      new CancelableRequest<GetBootTimesCallback>(callback));
  AddRequest(request, consumer);

  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      NewRunnableMethod(backend_.get(), &Backend::GetBootTimes, request));
  return request->handle();
}

// Extracts the uptime value from files located in /tmp, returning the
// value as a double in value.
static bool GetTime(const FilePath::StringType& log, double* value) {
  FilePath log_dir(kLogPath);
  FilePath log_file = log_dir.Append(log);
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
  static const FilePath::CharType kBootTimesSent[] =
      FPL("/tmp/boot-times-sent");
  FilePath sent(kBootTimesSent);
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

void BootTimesLoader::Backend::GetBootTimes(
    scoped_refptr<GetBootTimesRequest> request) {
  const FilePath::CharType kFirmwareBootTime[] = FPL("firmware-boot-time");
  const FilePath::CharType kPreStartup[] = FPL("pre-startup");
  const FilePath::CharType kChromeExec[] = FPL("chrome-exec");
  const FilePath::CharType kChromeMain[] = FPL("chrome-main");
  const FilePath::CharType kXStarted[] = FPL("x-started");
  const FilePath::CharType kLoginPromptReady[] = FPL("login-prompt-ready");
  const FilePath::StringType uptime_prefix = kUptimePrefix;

  if (request->canceled())
    return;

  // Wait until firmware-boot-time file exists by reposting.
  FilePath log_dir(kLogPath);
  FilePath log_file = log_dir.Append(kFirmwareBootTime);
  if (!file_util::PathExists(log_file)) {
    BrowserThread::PostDelayedTask(
        BrowserThread::FILE,
        FROM_HERE,
        NewRunnableMethod(this, &Backend::GetBootTimes, request),
        kReadAttemptDelayMs);
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

  request->ForwardResult(
      GetBootTimesCallback::TupleType(request->handle(), boot_times));
}

// Appends the given buffer into the file. Returns the number of bytes
// written, or -1 on error.
// TODO(satorux): Move this to file_util.
static int AppendFile(const FilePath& file_path,
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

static void RecordStatsDelayed(const FilePath::StringType& name,
                               const std::string& uptime,
                               const std::string& disk) {
  const FilePath log_path(kLogPath);
  const FilePath uptime_output =
      log_path.Append(FilePath(kUptimePrefix + name));
  const FilePath disk_output = log_path.Append(FilePath(kDiskPrefix + name));

  // Append numbers to the files.
  AppendFile(uptime_output, uptime.data(), uptime.size());
  AppendFile(disk_output, disk.data(), disk.size());
}

// static
void BootTimesLoader::WriteTimes(
    const std::string base_name,
    const std::string uma_name,
    const std::string uma_prefix,
    const std::vector<TimeMarker> login_times) {
  const int kMinTimeMillis = 1;
  const int kMaxTimeMillis = 30000;
  const int kNumBuckets = 100;
  const FilePath log_path(kLogPath);

  base::Time first = login_times.front().time();
  base::Time last = login_times.back().time();
  base::TimeDelta total = last - first;
  base::Histogram* total_hist = base::Histogram::FactoryTimeGet(
      uma_name,
      base::TimeDelta::FromMilliseconds(kMinTimeMillis),
      base::TimeDelta::FromMilliseconds(kMaxTimeMillis),
      kNumBuckets,
      base::Histogram::kUmaTargetedHistogramFlag);
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
      base::Histogram* prev_hist = base::Histogram::FactoryTimeGet(
          name,
          base::TimeDelta::FromMilliseconds(kMinTimeMillis),
          base::TimeDelta::FromMilliseconds(kMaxTimeMillis),
          kNumBuckets,
          base::Histogram::kUmaTargetedHistogramFlag);
      prev_hist->AddTime(since_prev);
    } else {
      name = tm.name();
    }
    output +=
        StringPrintf(
            "\n%.2f +%.2f %s",
            since_first.InSecondsF(),
            since_prev.InSecondsF(),
            name.data());
    prev = tm.time();
  }
  file_util::WriteFile(
      log_path.Append(base_name), output.data(), output.size());
}

void BootTimesLoader::WriteLogoutTimes() {
  WriteTimes(kLogoutTimes,
             kUmaLogout,
             kUmaLogoutPrefix,
             logout_time_markers_);
}

void BootTimesLoader::RecordStats(const std::string& name, const Stats& stats) {
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableFunction(
          RecordStatsDelayed, name, stats.uptime, stats.disk));
}

BootTimesLoader::Stats BootTimesLoader::GetCurrentStats() {
  const FilePath kProcUptime(FPL("/proc/uptime"));
  const FilePath kDiskStat(FPL("/sys/block/sda/stat"));
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
  login_time_markers_.clear();
  AddLoginTimeMarker("LoginStarted", false);
  if (!have_registered_) {
    have_registered_ = true;
    registrar_.Add(this, NotificationType::LOAD_START,
                   NotificationService::AllSources());
    registrar_.Add(this, NotificationType::LOGIN_AUTHENTICATION,
                   NotificationService::AllSources());
  }
}

void BootTimesLoader::AddLoginTimeMarker(
    const std::string& marker_name, bool send_to_uma) {
  login_time_markers_.push_back(TimeMarker(marker_name, send_to_uma));
}

void BootTimesLoader::AddLogoutTimeMarker(
    const std::string& marker_name, bool send_to_uma) {
  logout_time_markers_.push_back(TimeMarker(marker_name, send_to_uma));
}

void BootTimesLoader::Observe(
    NotificationType type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  if (type == NotificationType::LOGIN_AUTHENTICATION) {
    Details<AuthenticationNotificationDetails> auth_details(details);
    if (auth_details->success()) {
      AddLoginTimeMarker("Authenticate", true);
      RecordCurrentStats(kLoginSuccess);
      registrar_.Remove(this, NotificationType::LOGIN_AUTHENTICATION,
                        NotificationService::AllSources());
    }
  } else if (type == NotificationType::LOAD_START) {
    // Make sure it's not some page load initiated by OOBE/login screen.
    if (!UserManager::Get()->user_is_logged_in())
      return;
    // Only log for first tab to render.  Make sure this is only done once.
    // If the network isn't connected we'll get a second LOAD_START once it is
    // and the page is reloaded.
    if (NetworkStateNotifier::GetInstance()->is_connected()) {
      // Post difference between first tab and login success time.
      AddLoginTimeMarker("LoginDone", true);
      RecordCurrentStats(kChromeFirstRender);
      // Post chrome first render stat.
      registrar_.Remove(this, NotificationType::LOAD_START,
                        NotificationService::AllSources());
      // Don't swamp the FILE thread right away.
      BrowserThread::PostDelayedTask(
          BrowserThread::FILE, FROM_HERE,
          // This doesn't compile without std::string(...), as
          // NewRunnableFunction doesn't accept arrays.
          NewRunnableFunction(WriteTimes,
                              std::string(kLoginTimes),
                              std::string(kUmaLogin),
                              std::string(kUmaLoginPrefix),
                              login_time_markers_),
          kLoginTimeWriteDelayMs);
      have_registered_ = false;
    } else {
      AddLoginTimeMarker("LoginRenderNoNetwork", false);
    }
  }
}

}  // namespace chromeos
