// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging_win.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/template_util.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "base/win/scoped_handle.h"
#include "chrome/chrome_watcher/chrome_watcher_main_api.h"
#include "components/browser_watcher/endsession_watcher_window_win.h"
#include "components/browser_watcher/exit_code_watcher_win.h"
#include "components/browser_watcher/exit_funnel_win.h"

#ifdef KASKO
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

  // The funnel used to record events for this browser.
  browser_watcher::ExitFunnel exit_funnel_;

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
                               const base::char16* registry_path) :
    browser_exited_(true, false),  // manual reset, initially non-signalled.
    exit_code_watcher_(registry_path),
    end_session_watcher_window_(
        base::Bind(&BrowserMonitor::OnEndSessionMessage,
                   base::Unretained(this))),
    background_thread_("BrowserWatcherThread"),
    run_loop_(run_loop),
    main_thread_(base::MessageLoopProxy::current()) {
}

BrowserMonitor::~BrowserMonitor() {
}

bool BrowserMonitor::StartWatching(
    const base::char16* registry_path,
    base::Process process,
    base::win::ScopedHandle on_initialized_event) {
  if (!exit_code_watcher_.Initialize(process.Pass()))
    return false;

  if (!exit_funnel_.Init(registry_path,
                        exit_code_watcher_.process().Handle())) {
    return false;
  }

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
  DCHECK_EQ(main_thread_, base::MessageLoopProxy::current());

  if (message == WM_QUERYENDSESSION) {
    exit_funnel_.RecordEvent(L"WatcherQueryEndSession");
  } else if (message == WM_ENDSESSION) {
    exit_funnel_.RecordEvent(L"WatcherEndSession");
  }
  if (lparam & ENDSESSION_CLOSEAPP)
    exit_funnel_.RecordEvent(L"ES_CloseApp");
  if (lparam & ENDSESSION_CRITICAL)
    exit_funnel_.RecordEvent(L"ES_Critical");
  if (lparam & ENDSESSION_LOGOFF)
    exit_funnel_.RecordEvent(L"ES_Logoff");
  const LPARAM kKnownBits =
      ENDSESSION_CLOSEAPP | ENDSESSION_CRITICAL | ENDSESSION_LOGOFF;
  if (lparam & ~kKnownBits)
    exit_funnel_.RecordEvent(L"ES_Other");

  // If the browser hasn't exited yet, dally for a bit to try and stretch this
  // process' lifetime to give it some more time to capture the browser exit.
  browser_exited_.TimedWait(base::TimeDelta::FromSeconds(kDelayTimeSeconds));

  run_loop_->Quit();
}

void BrowserMonitor::Watch(base::win::ScopedHandle on_initialized_event) {
  // This needs to run on an IO thread.
  DCHECK_NE(main_thread_, base::MessageLoopProxy::current());

  // Signal our client now that the Kasko reporter is initialized and we have
  // cleared all of the obstacles that might lead to an early exit.
  ::SetEvent(on_initialized_event.Get());
  on_initialized_event.Close();

  exit_code_watcher_.WaitForExit();
  exit_funnel_.RecordEvent(L"BrowserExit");

  // Note that the browser has exited.
  browser_exited_.Signal();

  main_thread_->PostTask(FROM_HERE,
      base::Bind(&BrowserMonitor::BrowserExited, base::Unretained(this)));
}

void BrowserMonitor::BrowserExited() {
  // This runs in the main thread.
  DCHECK_EQ(main_thread_, base::MessageLoopProxy::current());

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

}  // namespace

// The main entry point to the watcher, declared as extern "C" to avoid name
// mangling.
extern "C" int WatcherMain(const base::char16* registry_path,
                           HANDLE process_handle,
                           HANDLE on_initialized_event_handle,
                           const base::char16* browser_data_directory) {
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

#ifdef KASKO
  bool launched_kasko = kasko::api::InitializeReporter(
      GetKaskoEndpoint(process.Pid()).c_str(),
      L"https://clients2.google.com/cr/report",
      base::FilePath(browser_data_directory)
          .Append(L"Crash Reports")
          .value()
          .c_str(),
      base::FilePath(browser_data_directory)
          .Append(kPermanentlyFailedReportsSubdir)
          .value()
          .c_str());
#endif  // KASKO

  // Run a UI message loop on the main thread.
  base::MessageLoop msg_loop(base::MessageLoop::TYPE_UI);
  msg_loop.set_thread_name("WatcherMainThread");

  base::RunLoop run_loop;
  BrowserMonitor monitor(&run_loop, registry_path);
  if (!monitor.StartWatching(registry_path, process.Pass(),
                             on_initialized_event.Pass())) {
    return 1;
  }

  run_loop.Run();

#ifdef KASKO
  if (launched_kasko)
    kasko::api::ShutdownReporter();
#endif  // KASKO

  // Wind logging down.
  logging::LogEventProvider::Uninitialize();

  return 0;
}

static_assert(
    base::is_same<decltype(&WatcherMain), ChromeWatcherMainFunction>::value,
    "WatcherMain() has wrong type");
