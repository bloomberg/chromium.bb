// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/boot_times_loader.h"

#include <vector>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/process_util.h"
#include "base/singleton.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/thread.h"
#include "base/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/chromeos/login/authentication_notification_details.h"
#include "chrome/browser/chromeos/network_state_notifier.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_service.h"

namespace chromeos {

// File uptime logs are located in.
static const char kLogPath[] = "/tmp";
// Prefix for the time measurement files.
static const char kUptimePrefix[] = "uptime-";
// Prefix for the disk usage files.
static const char kDiskPrefix[] = "disk-";
// Name of the time that Chrome's main() is called.
static const char kChromeMain[] = "chrome-main";
// Delay in milliseconds between file read attempts.
static const int64 kReadAttemptDelayMs = 250;
// Delay in milliseconds before writing the login times to disk.
static const int64 kLoginTimeWriteDelayMs = 3000;

// Names of login stats files.
static const char kLoginSuccess[] = "login-success";
static const char kChromeFirstRender[] = "chrome-first-render";

// Names of login UMA values.
static const char kUmaAuthenticate[] = "BootTime.Authenticate";
static const char kUmaLogin[] = "BootTime.Login";

// Name of file collecting login times.
static const char kLoginTimes[] = "login-times-sent";

BootTimesLoader::BootTimesLoader()
    : backend_(new Backend()),
      have_registered_(false) {
  login_time_markers_.reserve(30);
}

// static
BootTimesLoader* BootTimesLoader::Get() {
  return Singleton<BootTimesLoader>::get();
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
static bool GetTime(const std::string& log, double* value) {
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
  static const char kBootTimesSent[] = "/tmp/boot-times-sent";
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
  const char* kFirmwareBootTime = "firmware-boot-time";
  const char* kPreStartup = "pre-startup";
  const char* kChromeExec = "chrome-exec";
  const char* kChromeMain = "chrome-main";
  const char* kXStarted = "x-started";
  const char* kLoginPromptReady = "login-prompt-ready";
  std::string uptime_prefix = kUptimePrefix;

  if (request->canceled())
    return;

  // Wait until login_prompt_ready is output by reposting.
  FilePath log_dir(kLogPath);
  FilePath log_file = log_dir.Append(uptime_prefix + kLoginPromptReady);
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

static void RecordStatsDelayed(
    const std::string& name,
    const std::string& uptime,
    const std::string& disk) {
  const FilePath log_path(kLogPath);
  std::string disk_prefix = kDiskPrefix;
  const FilePath uptime_output =
      log_path.Append(FilePath(kUptimePrefix + name));
  const FilePath disk_output = log_path.Append(FilePath(kDiskPrefix + name));

  // Write out the files, ensuring that they don't exist already.
  if (!file_util::PathExists(uptime_output))
    file_util::WriteFile(uptime_output, uptime.data(), uptime.size());
  if (!file_util::PathExists(disk_output))
    file_util::WriteFile(disk_output, disk.data(), disk.size());
}

// static
void BootTimesLoader::WriteLoginTimes(
    const std::vector<TimeMarker> login_times) {
  const int kMinTimeMillis = 1;
  const int kMaxTimeMillis = 30;
  const int kNumBuckets = 100;
  const char kUmaPrefix[] = "BootTime.";
  const FilePath log_path(kLogPath);

  base::Time first = login_times.front().time();
  base::Time last = login_times.back().time();
  base::TimeDelta total = last - first;
  scoped_refptr<base::Histogram>total_hist = base::Histogram::FactoryTimeGet(
      kUmaLogin,
      base::TimeDelta::FromMilliseconds(kMinTimeMillis),
      base::TimeDelta::FromMilliseconds(kMaxTimeMillis),
      kNumBuckets,
      base::Histogram::kUmaTargetedHistogramFlag);
  total_hist->AddTime(total);
  std::string output =
      base::StringPrintf("%s: %.2f", kUmaLogin, total.InSecondsF());
  base::Time prev = first;
  for (unsigned int i = 0; i < login_times.size(); ++i) {
    TimeMarker tm = login_times[i];
    base::TimeDelta since_first = tm.time() - first;
    base::TimeDelta since_prev = tm.time() - prev;
    std::string name;

    if (tm.send_to_uma()) {
      name = kUmaPrefix + tm.name();
      scoped_refptr<base::Histogram>prev_hist = base::Histogram::FactoryTimeGet(
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
      log_path.Append(kLoginTimes), output.data(), output.size());
}

void BootTimesLoader::RecordStats(const std::string& name, const Stats& stats) {
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableFunction(
          RecordStatsDelayed, name, stats.uptime, stats.disk));
}

BootTimesLoader::Stats BootTimesLoader::GetCurrentStats() {
  const FilePath kProcUptime("/proc/uptime");
  const FilePath kDiskStat("/sys/block/sda/stat");
  Stats stats;

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
    // Only log for first tab to render.  Make sure this is only done once.
    // If the network isn't connected we'll get a second LOAD_START once it is
    // and the page is reloaded.
    if (NetworkStateNotifier::Get()->is_connected()) {
      // Post difference between first tab and login success time.
      AddLoginTimeMarker("LoginDone", false);
      RecordCurrentStats(kChromeFirstRender);
      // Post chrome first render stat.
      registrar_.Remove(this, NotificationType::LOAD_START,
                        NotificationService::AllSources());
      // Don't swamp the FILE thread right away.
      BrowserThread::PostDelayedTask(
          BrowserThread::FILE, FROM_HERE,
          NewRunnableFunction(WriteLoginTimes, login_time_markers_),
          kLoginTimeWriteDelayMs);
      have_registered_ = false;
    } else {
      AddLoginTimeMarker("LoginRenderNoNetwork", false);
    }
  }
}

}  // namespace chromeos
