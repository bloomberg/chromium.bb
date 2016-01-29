// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <sddl.h>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/file_version_info.h"
#include "base/files/file_path.h"
#include "base/logging_win.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/template_util.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "base/win/scoped_handle.h"
#include "base/win/win_util.h"

#include "chrome/chrome_watcher/chrome_watcher_main_api.h"
#include "chrome/installer/util/util_constants.h"
#include "components/browser_watcher/endsession_watcher_window_win.h"
#include "components/browser_watcher/exit_code_watcher_win.h"
#include "components/browser_watcher/window_hang_monitor_win.h"
#include "third_party/kasko/kasko_features.h"

#if BUILDFLAG(ENABLE_KASKO)
#include "syzygy/kasko/api/reporter.h"
#endif

namespace {

// Use the same log facility as Chrome for convenience.
// {7FE69228-633E-4f06-80C1-527FEA23E3A7}
const GUID kChromeWatcherTraceProviderName = {
    0x7fe69228, 0x633e, 0x4f06,
        { 0x80, 0xc1, 0x52, 0x7f, 0xea, 0x23, 0xe3, 0xa7 } };

// The amount of time we wait around for a WM_ENDSESSION or a process exit.
const int kDelayTimeSeconds = 30;

// Takes care of monitoring a browser. This class watches for a browser's exit
// code, as well as listening for WM_ENDSESSION messages. Events are recorded in
// an exit funnel, for reporting the next time Chrome runs.
class BrowserMonitor {
 public:
  BrowserMonitor(base::RunLoop* run_loop, const base::char16* registry_path);
  ~BrowserMonitor();

  // Initiates the asynchronous monitoring process, returns true on success.
  // |on_initialized_event| will be signaled immediately before blocking on the
  // exit of |process|.
  bool StartWatching(const base::char16* registry_path,
                     base::Process process,
                     base::win::ScopedHandle on_initialized_event);

 private:
  // Called from EndSessionWatcherWindow on a end session messages.
  void OnEndSessionMessage(UINT message, LPARAM lparam);

  // Blocking function that runs on |background_thread_|. Signals
  // |on_initialized_event| before waiting for the browser process to exit.
  void Watch(base::win::ScopedHandle on_initialized_event);

  // Posted to main thread from Watch when browser exits.
  void BrowserExited();

  browser_watcher::ExitCodeWatcher exit_code_watcher_;
  browser_watcher::EndSessionWatcherWindow end_session_watcher_window_;

  // The thread that runs Watch().
  base::Thread background_thread_;

  // Set when the browser has exited, used to stretch the watcher's lifetime
  // when WM_ENDSESSION occurs before browser exit.
  base::WaitableEvent browser_exited_;

  // The run loop for the main thread and its task runner.
  base::RunLoop* run_loop_;
  scoped_refptr<base::SequencedTaskRunner> main_thread_;

  DISALLOW_COPY_AND_ASSIGN(BrowserMonitor);
};

BrowserMonitor::BrowserMonitor(base::RunLoop* run_loop,
                               const base::char16* registry_path)
    : exit_code_watcher_(registry_path),
      end_session_watcher_window_(
          base::Bind(&BrowserMonitor::OnEndSessionMessage,
                     base::Unretained(this))),
      background_thread_("BrowserWatcherThread"),
      browser_exited_(true, false),  // manual reset, initially non-signalled.
      run_loop_(run_loop),
      main_thread_(base::ThreadTaskRunnerHandle::Get()) {
}

BrowserMonitor::~BrowserMonitor() {
}

bool BrowserMonitor::StartWatching(
    const base::char16* registry_path,
    base::Process process,
    base::win::ScopedHandle on_initialized_event) {
  if (!exit_code_watcher_.Initialize(process.Pass()))
    return false;

  if (!background_thread_.StartWithOptions(
        base::Thread::Options(base::MessageLoop::TYPE_IO, 0))) {
    return false;
  }

  if (!background_thread_.task_runner()->PostTask(
          FROM_HERE, base::Bind(&BrowserMonitor::Watch, base::Unretained(this),
                                base::Passed(on_initialized_event.Pass())))) {
    background_thread_.Stop();
    return false;
  }

  return true;
}

void BrowserMonitor::OnEndSessionMessage(UINT message, LPARAM lparam) {
  DCHECK_EQ(main_thread_, base::ThreadTaskRunnerHandle::Get());

  // If the browser hasn't exited yet, dally for a bit to try and stretch this
  // process' lifetime to give it some more time to capture the browser exit.
  browser_exited_.TimedWait(base::TimeDelta::FromSeconds(kDelayTimeSeconds));

  run_loop_->Quit();
}

void BrowserMonitor::Watch(base::win::ScopedHandle on_initialized_event) {
  // This needs to run on an IO thread.
  DCHECK_NE(main_thread_, base::ThreadTaskRunnerHandle::Get());

  // Signal our client now that the Kasko reporter is initialized and we have
  // cleared all of the obstacles that might lead to an early exit.
  ::SetEvent(on_initialized_event.Get());
  on_initialized_event.Close();

  exit_code_watcher_.WaitForExit();

  // Note that the browser has exited.
  browser_exited_.Signal();

  main_thread_->PostTask(FROM_HERE,
      base::Bind(&BrowserMonitor::BrowserExited, base::Unretained(this)));
}

void BrowserMonitor::BrowserExited() {
  // This runs in the main thread.
  DCHECK_EQ(main_thread_, base::ThreadTaskRunnerHandle::Get());

  // Our background thread has served it's purpose.
  background_thread_.Stop();

  const int exit_code = exit_code_watcher_.exit_code();
  if (exit_code >= 0 && exit_code <= 28) {
    // The browser exited with a well-known exit code, quit this process
    // immediately.
    run_loop_->Quit();
  } else {
    // The browser exited abnormally, wait around for a little bit to see
    // whether this instance will get a logoff message.
    main_thread_->PostDelayedTask(
        FROM_HERE,
        run_loop_->QuitClosure(),
        base::TimeDelta::FromSeconds(kDelayTimeSeconds));
  }
}

void OnWindowEvent(
    const base::string16& registry_path,
    base::Process process,
    const base::Callback<void(const base::Process&)>& on_hung_callback,
    browser_watcher::WindowHangMonitor::WindowEvent window_event) {
  if (window_event == browser_watcher::WindowHangMonitor::WINDOW_HUNG &&
      !on_hung_callback.is_null()) {
    on_hung_callback.Run(process);
  }
}

#if BUILDFLAG(ENABLE_KASKO)
// Helper function for determining the crash server to use. Defaults to the
// standard crash server, but can be overridden via an environment variable.
// Enables easy integration testing.
void GetKaskoCrashServerUrl(base::string16* crash_server) {
  const char kKaskoCrashServerUrl[] = "KASKO_CRASH_SERVER_URL";
  static const wchar_t kDefaultKaskoCrashServerUrl[] =
      L"https://clients2.google.com/cr/report";

  auto env = base::Environment::Create();
  std::string env_var;
  if (env->GetVar(kKaskoCrashServerUrl, &env_var)) {
    base::UTF8ToWide(env_var.c_str(), env_var.size(), crash_server);
  } else {
    *crash_server = kDefaultKaskoCrashServerUrl;
  }
}

// Helper function for determining the crash reports directory to use. Defaults
// to the browser data directory, but can be overridden via an environment
// variable. Enables easy integration testing.
void GetKaskoCrashReportsBaseDir(const base::char16* browser_data_directory,
                                 base::FilePath* base_dir) {
  const char kKaskoCrashReportBaseDir[] = "KASKO_CRASH_REPORTS_BASE_DIR";
  auto env = base::Environment::Create();
  std::string env_var;
  if (env->GetVar(kKaskoCrashReportBaseDir, &env_var)) {
    base::string16 wide_env_var;
    base::UTF8ToWide(env_var.c_str(), env_var.size(), &wide_env_var);
    *base_dir = base::FilePath(wide_env_var);
  } else {
    *base_dir = base::FilePath(browser_data_directory);
  }
}

void DumpHungBrowserProcess(DWORD main_thread_id,
                            const base::string16& channel,
                            const base::Process& process) {
  // TODO(erikwright): Rather than recreating these crash keys here, it would be
  // ideal to read them directly from the browser process.

  // This is looking up the version of chrome_watcher.dll, which is equivalent
  // for our purposes to chrome.dll.
  scoped_ptr<FileVersionInfo> version_info(
      CREATE_FILE_VERSION_INFO_FOR_CURRENT_MODULE());
  using CrashKeyStrings = std::pair<base::string16, base::string16>;
  std::vector<CrashKeyStrings> crash_key_strings;
  if (version_info.get()) {
    crash_key_strings.push_back(
        CrashKeyStrings(L"prod", version_info->product_short_name()));
    base::string16 version = version_info->product_version();
    if (!version_info->is_official_build())
      version.append(base::ASCIIToUTF16("-devel"));
    crash_key_strings.push_back(CrashKeyStrings(L"ver", version));
  } else {
    // No version info found. Make up the values.
    crash_key_strings.push_back(CrashKeyStrings(L"prod", L"Chrome"));
    crash_key_strings.push_back(CrashKeyStrings(L"ver", L"0.0.0.0-devel"));
  }
  crash_key_strings.push_back(CrashKeyStrings(L"channel", channel));
  crash_key_strings.push_back(CrashKeyStrings(L"plat", L"Win32"));
  crash_key_strings.push_back(CrashKeyStrings(L"ptype", L"browser"));
  crash_key_strings.push_back(
      CrashKeyStrings(L"pid", base::IntToString16(process.Pid())));
  crash_key_strings.push_back(CrashKeyStrings(L"hung-process", L"1"));

  std::vector<const base::char16*> key_buffers;
  std::vector<const base::char16*> value_buffers;
  for (auto& strings : crash_key_strings) {
    key_buffers.push_back(strings.first.c_str());
    value_buffers.push_back(strings.second.c_str());
  }
  key_buffers.push_back(nullptr);
  value_buffers.push_back(nullptr);

  // Synthesize an exception for the main thread.
  CONTEXT thread_context = {};
  EXCEPTION_RECORD exception_record = {};
  exception_record.ExceptionCode = EXCEPTION_ARRAY_BOUNDS_EXCEEDED;
  EXCEPTION_POINTERS exception_pointers = {&exception_record, &thread_context};

  // TODO(erikwright): Make the dump-type channel-dependent.
  kasko::api::SendReportForProcess(
      process.Handle(), main_thread_id, &exception_pointers,
      kasko::api::LARGER_DUMP_TYPE, key_buffers.data(), value_buffers.data());
}

void LoggedDeregisterEventSource(HANDLE event_source_handle) {
  if (!::DeregisterEventSource(event_source_handle))
    DPLOG(ERROR) << "DeregisterEventSource";
}

void LoggedLocalFree(PSID sid) {
  if (::LocalFree(sid) != nullptr)
    DPLOG(ERROR) << "LocalFree";
}

void OnCrashReportUpload(void* context,
                         const base::char16* report_id,
                         const base::char16* minidump_path,
                         const base::char16* const* keys,
                         const base::char16* const* values) {
  // Open the event source.
  HANDLE event_source_handle = ::RegisterEventSource(NULL, L"Chrome");
  if (!event_source_handle) {
    PLOG(ERROR) << "RegisterEventSource";
    return;
  }
  // Ensure cleanup on scope exit.
  base::ScopedClosureRunner deregister_event_source(
      base::Bind(&LoggedDeregisterEventSource, event_source_handle));

  // Get the user's SID for the log record.
  base::string16 sid_string;
  PSID sid = nullptr;
  if (base::win::GetUserSidString(&sid_string)) {
    if (!sid_string.empty()) {
      if (!::ConvertStringSidToSid(sid_string.c_str(), &sid))
        DPLOG(ERROR) << "ConvertStringSidToSid";
      DCHECK(sid);
    }
  }
  // Ensure cleanup on scope exit.
  base::ScopedClosureRunner free_sid(
      base::Bind(&LoggedLocalFree, base::Unretained(sid)));

  // Generate the message.
  // Note that the format of this message must match the consumer in
  // chrome/browser/crash_upload_list_win.cc.
  base::string16 message =
      L"Crash uploaded. Id=" + base::string16(report_id) + L".";

  // Matches Omaha.
  const int kCrashUploadEventId = 2;

  // Report the event.
  const base::char16* strings[] = {message.c_str()};
  if (!::ReportEvent(event_source_handle, EVENTLOG_INFORMATION_TYPE,
                     0,  // category
                     kCrashUploadEventId, sid,
                     1,  // count
                     0, strings, nullptr)) {
    DPLOG(ERROR);
  }

  // TODO(erikwright): Copy minidump to some "last dump" location?
}

#endif  // BUILDFLAG(ENABLE_KASKO)

}  // namespace

// The main entry point to the watcher, declared as extern "C" to avoid name
// mangling.
extern "C" int WatcherMain(const base::char16* registry_path,
                           HANDLE process_handle,
                           DWORD main_thread_id,
                           HANDLE on_initialized_event_handle,
                           const base::char16* browser_data_directory,
                           const base::char16* channel_name) {
  base::Process process(process_handle);
  base::win::ScopedHandle on_initialized_event(on_initialized_event_handle);

  // The exit manager is in charge of calling the dtors of singletons.
  base::AtExitManager exit_manager;
  // Initialize the commandline singleton from the environment.
  base::CommandLine::Init(0, nullptr);

  logging::LogEventProvider::Initialize(kChromeWatcherTraceProviderName);

  // Arrange to be shut down as late as possible, as we want to outlive
  // chrome.exe in order to report its exit status.
  ::SetProcessShutdownParameters(0x100, SHUTDOWN_NORETRY);

  base::Callback<void(const base::Process&)> on_hung_callback;

#if BUILDFLAG(ENABLE_KASKO)
  base::string16 crash_server;
  GetKaskoCrashServerUrl(&crash_server);

  base::FilePath crash_reports_base_dir;
  GetKaskoCrashReportsBaseDir(browser_data_directory, &crash_reports_base_dir);
  bool launched_kasko = kasko::api::InitializeReporter(
      GetKaskoEndpoint(process.Pid()).c_str(),
      crash_server.c_str(),
      crash_reports_base_dir
          .Append(L"Crash Reports")
          .value()
          .c_str(),
      crash_reports_base_dir
          .Append(kPermanentlyFailedReportsSubdir)
          .value()
          .c_str(),
      &OnCrashReportUpload, nullptr);
#if BUILDFLAG(ENABLE_KASKO_HANG_REPORTS)
  // Only activate hang reports for the canary channel. For testing purposes,
  // Chrome instances with no channels will also report hangs.
  if (launched_kasko &&
      (base::StringPiece16(channel_name) == L"" ||
       base::StringPiece16(channel_name) == installer::kChromeChannelCanary)) {
    on_hung_callback =
        base::Bind(&DumpHungBrowserProcess, main_thread_id, channel_name);
  }
#endif  // BUILDFLAG(ENABLE_KASKO_HANG_REPORTS)
#endif  // BUILDFLAG(ENABLE_KASKO)

  // Run a UI message loop on the main thread.
  base::MessageLoop msg_loop(base::MessageLoop::TYPE_UI);
  msg_loop.set_thread_name("WatcherMainThread");

  base::RunLoop run_loop;
  BrowserMonitor monitor(&run_loop, registry_path);
  if (!monitor.StartWatching(registry_path, process.Duplicate(),
                             on_initialized_event.Pass())) {
    return 1;
  }

  {
    // Scoped to force |hang_monitor| destruction before Kasko is shut down.
    browser_watcher::WindowHangMonitor hang_monitor(
        base::TimeDelta::FromSeconds(60), base::TimeDelta::FromSeconds(20),
        base::Bind(&OnWindowEvent, registry_path,
                   base::Passed(process.Duplicate()), on_hung_callback));
    hang_monitor.Initialize(process.Duplicate());

    run_loop.Run();
  }

#if BUILDFLAG(ENABLE_KASKO)
  if (launched_kasko)
    kasko::api::ShutdownReporter();
#endif  // BUILDFLAG(ENABLE_KASKO)

  // Wind logging down.
  logging::LogEventProvider::Uninitialize();

  return 0;
}

static_assert(
    base::is_same<decltype(&WatcherMain), ChromeWatcherMainFunction>::value,
    "WatcherMain() has wrong type");
