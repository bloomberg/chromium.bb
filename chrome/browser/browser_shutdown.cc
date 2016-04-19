// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_shutdown.h"

#include <stdint.h>

#include <map>
#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/crash_keys.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/switch_utils.h"
#include "components/metrics/metrics_service.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/tracing/tracing_switches.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/tracing_controller.h"

#if defined(OS_WIN)
#include "chrome/browser/browser_util_win.h"
#include "chrome/browser/first_run/upgrade_util_win.h"
#endif

#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
#include "chrome/browser/first_run/upgrade_util.h"
#endif

#if defined(ENABLE_RLZ)
#include "components/rlz/rlz_tracker.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/boot_times_recorder.h"
#endif

#if defined(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/service_process/service_process_control.h"
#endif

using base::Time;
using base::TimeDelta;
using content::BrowserThread;

namespace browser_shutdown {
namespace {

// Whether the browser is trying to quit (e.g., Quit chosen from menu).
bool g_trying_to_quit = false;

Time* g_shutdown_started = nullptr;
ShutdownType g_shutdown_type = NOT_VALID;
int g_shutdown_num_processes;
int g_shutdown_num_processes_slow;

const char kShutdownMsFile[] = "chrome_shutdown_ms.txt";

const char* ToShutdownTypeString(ShutdownType type) {
  switch (type) {
    case NOT_VALID:
      NOTREACHED();
      return "";
    case WINDOW_CLOSE:
      return "close";
    case BROWSER_EXIT:
      return "exit";
    case END_SESSION:
      return "end";
  }
  return "";
}

}  // namespace

void RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kShutdownType, NOT_VALID);
  registry->RegisterIntegerPref(prefs::kShutdownNumProcesses, 0);
  registry->RegisterIntegerPref(prefs::kShutdownNumProcessesSlow, 0);
  registry->RegisterBooleanPref(prefs::kRestartLastSessionOnShutdown, false);
}

ShutdownType GetShutdownType() {
  return g_shutdown_type;
}

void OnShutdownStarting(ShutdownType type) {
  if (g_shutdown_type != NOT_VALID)
    return;
  base::debug::SetCrashKeyValue(crash_keys::kShutdownType,
                                ToShutdownTypeString(type));
#if !defined(OS_CHROMEOS)
  // Start the shutdown tracing. Note that On ChromeOS this has already been
  // called in AttemptUserExit().
  StartShutdownTracing();
#endif

  g_shutdown_type = type;
  // For now, we're only counting the number of renderer processes
  // since we can't safely count the number of plugin processes from this
  // thread, and we'd really like to avoid anything which might add further
  // delays to shutdown time.
  DCHECK(!g_shutdown_started);
  g_shutdown_started = new Time(Time::Now());

  // Call FastShutdown on all of the RenderProcessHosts.  This will be
  // a no-op in some cases, so we still need to go through the normal
  // shutdown path for the ones that didn't exit here.
  g_shutdown_num_processes = 0;
  g_shutdown_num_processes_slow = 0;
  for (content::RenderProcessHost::iterator i(
          content::RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    ++g_shutdown_num_processes;
    if (!i.GetCurrentValue()->FastShutdownIfPossible())
      ++g_shutdown_num_processes_slow;
  }
}

base::FilePath GetShutdownMsPath() {
  base::FilePath shutdown_ms_file;
  PathService::Get(chrome::DIR_USER_DATA, &shutdown_ms_file);
  return shutdown_ms_file.AppendASCII(kShutdownMsFile);
}

#if !defined(OS_ANDROID)
bool ShutdownPreThreadsStop() {
#if defined(OS_CHROMEOS)
  chromeos::BootTimesRecorder::Get()->AddLogoutTimeMarker(
      "BrowserShutdownStarted", false);
#endif
#if defined(ENABLE_PRINT_PREVIEW)
  // Shutdown the IPC channel to the service processes.
  ServiceProcessControl::GetInstance()->Disconnect();
#endif  // ENABLE_PRINT_PREVIEW

  // WARNING: During logoff/shutdown (WM_ENDSESSION) we may not have enough
  // time to get here. If you have something that *must* happen on end session,
  // consider putting it in BrowserProcessImpl::EndSession.
  PrefService* prefs = g_browser_process->local_state();

  // Log the amount of times the user switched profiles during this session.
  ProfileMetrics::LogNumberOfProfileSwitches();

  metrics::MetricsService* metrics = g_browser_process->metrics_service();
  if (metrics)
    metrics->RecordCompletedSessionEnd();

  if (g_shutdown_type > NOT_VALID && g_shutdown_num_processes > 0) {
    // Record the shutdown info so that we can put it into a histogram at next
    // startup.
    prefs->SetInteger(prefs::kShutdownType, g_shutdown_type);
    prefs->SetInteger(prefs::kShutdownNumProcesses, g_shutdown_num_processes);
    prefs->SetInteger(prefs::kShutdownNumProcessesSlow,
                      g_shutdown_num_processes_slow);
  }

  // Check local state for the restart flag so we can restart the session below.
  bool restart_last_session = false;
  if (prefs->HasPrefPath(prefs::kRestartLastSessionOnShutdown)) {
    restart_last_session =
        prefs->GetBoolean(prefs::kRestartLastSessionOnShutdown);
    prefs->ClearPref(prefs::kRestartLastSessionOnShutdown);
  }

  prefs->CommitPendingWrite();

#if defined(ENABLE_RLZ)
  // Cleanup any statics created by RLZ. Must be done before NotificationService
  // is destroyed.
  rlz::RLZTracker::CleanupRlz();
#endif

  return restart_last_session;
}

void ShutdownPostThreadsStop(bool restart_last_session) {
  delete g_browser_process;
  g_browser_process = NULL;

  // crbug.com/95079 - This needs to happen after the browser process object
  // goes away.
  ProfileManager::NukeDeletedProfilesFromDisk();

#if defined(OS_CHROMEOS)
  chromeos::BootTimesRecorder::Get()->AddLogoutTimeMarker("BrowserDeleted",
                                                          true);
#endif

#if defined(OS_WIN)
  if (!browser_util::IsBrowserAlreadyRunning() &&
      g_shutdown_type != END_SESSION) {
    upgrade_util::SwapNewChromeExeIfPresent();
  }
#endif

  if (restart_last_session) {
#if !defined(OS_CHROMEOS)
    // Make sure to relaunch the browser with the original command line plus
    // the Restore Last Session flag. Note that Chrome can be launched (ie.
    // through ShellExecute on Windows) with a switch argument terminator at
    // the end (double dash, as described in b/1366444) plus a URL,
    // which prevents us from appending to the command line directly (issue
    // 46182). We therefore use GetSwitches to copy the command line (it stops
    // at the switch argument terminator).
    base::CommandLine old_cl(*base::CommandLine::ForCurrentProcess());
    std::unique_ptr<base::CommandLine> new_cl(
        new base::CommandLine(old_cl.GetProgram()));
    std::map<std::string, base::CommandLine::StringType> switches =
        old_cl.GetSwitches();
    // Remove the switches that shouldn't persist across restart.
    about_flags::RemoveFlagsSwitches(&switches);
    switches::RemoveSwitchesForAutostart(&switches);
    // Append the old switches to the new command line.
    for (const auto& it : switches) {
      const base::CommandLine::StringType& switch_value = it.second;
      if (!switch_value.empty())
        new_cl->AppendSwitchNative(it.first, it.second);
      else
        new_cl->AppendSwitch(it.first);
    }

#if defined(OS_POSIX) || defined(OS_WIN)
    upgrade_util::RelaunchChromeBrowser(*new_cl.get());
#endif  // defined(OS_WIN)

#else
    NOTIMPLEMENTED();
#endif  // !defined(OS_CHROMEOS)
  }

  if (g_shutdown_type > NOT_VALID && g_shutdown_num_processes > 0) {
    // Measure total shutdown time as late in the process as possible
    // and then write it to a file to be read at startup.
    // We can't use prefs since all services are shutdown at this point.
    TimeDelta shutdown_delta = Time::Now() - *g_shutdown_started;
    std::string shutdown_ms =
        base::Int64ToString(shutdown_delta.InMilliseconds());
    int len = static_cast<int>(shutdown_ms.length()) + 1;
    base::FilePath shutdown_ms_file = GetShutdownMsPath();
    base::WriteFile(shutdown_ms_file, shutdown_ms.c_str(), len);
  }

#if defined(OS_CHROMEOS)
  chrome::NotifyAndTerminate(false);
#endif
}
#endif  // !defined(OS_ANDROID)

void ReadLastShutdownFile(ShutdownType type,
                          int num_procs,
                          int num_procs_slow) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  base::FilePath shutdown_ms_file = GetShutdownMsPath();
  std::string shutdown_ms_str;
  int64_t shutdown_ms = 0;
  if (base::ReadFileToString(shutdown_ms_file, &shutdown_ms_str))
    base::StringToInt64(shutdown_ms_str, &shutdown_ms);
  base::DeleteFile(shutdown_ms_file, false);

  if (type == NOT_VALID || shutdown_ms == 0 || num_procs == 0)
    return;

  const char time_fmt[] = "Shutdown.%s.time";
  const char time_per_fmt[] = "Shutdown.%s.time_per_process";
  std::string time;
  std::string time_per;
  if (type == WINDOW_CLOSE) {
    time = base::StringPrintf(time_fmt, "window_close");
    time_per = base::StringPrintf(time_per_fmt, "window_close");
  } else if (type == BROWSER_EXIT) {
    time = base::StringPrintf(time_fmt, "browser_exit");
    time_per = base::StringPrintf(time_per_fmt, "browser_exit");
  } else if (type == END_SESSION) {
    time = base::StringPrintf(time_fmt, "end_session");
    time_per = base::StringPrintf(time_per_fmt, "end_session");
  } else {
    NOTREACHED();
  }

  if (time.empty())
    return;

  // TODO(erikkay): change these to UMA histograms after a bit more testing.
  UMA_HISTOGRAM_TIMES(time.c_str(), TimeDelta::FromMilliseconds(shutdown_ms));
  UMA_HISTOGRAM_TIMES(time_per.c_str(),
                      TimeDelta::FromMilliseconds(shutdown_ms / num_procs));
  UMA_HISTOGRAM_COUNTS_100("Shutdown.renderers.total", num_procs);
  UMA_HISTOGRAM_COUNTS_100("Shutdown.renderers.slow", num_procs_slow);
}

void ReadLastShutdownInfo() {
  PrefService* prefs = g_browser_process->local_state();
  ShutdownType type =
      static_cast<ShutdownType>(prefs->GetInteger(prefs::kShutdownType));
  int num_procs = prefs->GetInteger(prefs::kShutdownNumProcesses);
  int num_procs_slow = prefs->GetInteger(prefs::kShutdownNumProcessesSlow);
  // clear the prefs immediately so we don't pick them up on a future run
  prefs->SetInteger(prefs::kShutdownType, NOT_VALID);
  prefs->SetInteger(prefs::kShutdownNumProcesses, 0);
  prefs->SetInteger(prefs::kShutdownNumProcessesSlow, 0);

  // Read and delete the file on the file thread.
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&ReadLastShutdownFile, type, num_procs, num_procs_slow));
}

void SetTryingToQuit(bool quitting) {
  g_trying_to_quit = quitting;
}

bool IsTryingToQuit() {
  return g_trying_to_quit;
}

void StartShutdownTracing() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kTraceShutdown)) {
    base::trace_event::TraceConfig trace_config(
        command_line.GetSwitchValueASCII(switches::kTraceShutdown), "");
    content::TracingController::GetInstance()->StartTracing(
        trace_config,
        content::TracingController::StartTracingDoneCallback());
  }
  TRACE_EVENT0("shutdown", "StartShutdownTracing");
}

}  // namespace browser_shutdown
