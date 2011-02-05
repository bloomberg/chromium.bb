// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(OS_MACOSX)
#include <signal.h>
#include <unistd.h>
#endif  // OS_MACOSX

#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/metrics/field_trial.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/metrics/stats_counters.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/threading/platform_thread.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_counters.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/gfx_resource_provider.h"
#include "chrome/common/hi_res_timer_manager.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/main_function_params.h"
#include "chrome/common/net/net_resource_provider.h"
#include "chrome/common/pepper_plugin_registry.h"
#include "chrome/renderer/renderer_main_platform_delegate.h"
#include "chrome/renderer/render_process_impl.h"
#include "chrome/renderer/render_thread.h"
#include "grit/generated_resources.h"
#include "net/base/net_module.h"
#include "ui/base/system_monitor/system_monitor.h"
#include "ui/gfx/gfx_module.h"

#if defined(OS_MACOSX)
#include "base/eintr_wrapper.h"
#include "chrome/app/breakpad_mac.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#endif  // OS_MACOSX

#if defined(USE_LINUX_BREAKPAD)
#include "chrome/app/breakpad_linux.h"
#endif

#if defined(OS_MACOSX)
namespace {

// TODO(viettrungluu): crbug.com/28547: The following signal handling is needed,
// as a stopgap, to avoid leaking due to not releasing Breakpad properly.
// Without this problem, this could all be eliminated. Remove when Breakpad is
// fixed?
// TODO(viettrungluu): Code taken from browser_main.cc (with a bit of editing).
// The code should be properly shared (or this code should be eliminated).
int g_shutdown_pipe_write_fd = -1;

void SIGTERMHandler(int signal) {
  RAW_CHECK(signal == SIGTERM);
  RAW_LOG(INFO, "Handling SIGTERM in renderer.");

  // Reinstall the default handler.  We had one shot at graceful shutdown.
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  action.sa_handler = SIG_DFL;
  CHECK(sigaction(signal, &action, NULL) == 0);

  RAW_CHECK(g_shutdown_pipe_write_fd != -1);
  size_t bytes_written = 0;
  do {
    int rv = HANDLE_EINTR(
        write(g_shutdown_pipe_write_fd,
              reinterpret_cast<const char*>(&signal) + bytes_written,
              sizeof(signal) - bytes_written));
    RAW_CHECK(rv >= 0);
    bytes_written += rv;
  } while (bytes_written < sizeof(signal));

  RAW_LOG(INFO, "Wrote signal to shutdown pipe.");
}

class ShutdownDetector : public base::PlatformThread::Delegate {
 public:
  explicit ShutdownDetector(int shutdown_fd) : shutdown_fd_(shutdown_fd) {
    CHECK(shutdown_fd_ != -1);
  }

  virtual void ThreadMain() {
    int signal;
    size_t bytes_read = 0;
    ssize_t ret;
    do {
      ret = HANDLE_EINTR(
          read(shutdown_fd_,
               reinterpret_cast<char*>(&signal) + bytes_read,
               sizeof(signal) - bytes_read));
      if (ret < 0) {
        NOTREACHED() << "Unexpected error: " << strerror(errno);
        break;
      } else if (ret == 0) {
        NOTREACHED() << "Unexpected closure of shutdown pipe.";
        break;
      }
      bytes_read += ret;
    } while (bytes_read < sizeof(signal));

    if (bytes_read == sizeof(signal))
      VLOG(1) << "Handling shutdown for signal " << signal << ".";
    else
      VLOG(1) << "Handling shutdown for unknown signal.";

    // Clean up Breakpad if necessary.
    if (IsCrashReporterEnabled()) {
      VLOG(1) << "Cleaning up Breakpad.";
      DestructCrashReporter();
    } else {
      VLOG(1) << "Breakpad not enabled; no clean-up needed.";
    }

    // Something went seriously wrong, so get out.
    if (bytes_read != sizeof(signal)) {
      LOG(WARNING) << "Failed to get signal. Quitting ungracefully.";
      _exit(1);
    }

    // Re-raise the signal.
    kill(getpid(), signal);

    // The signal may be handled on another thread. Give that a chance to
    // happen.
    sleep(3);

    // We really should be dead by now.  For whatever reason, we're not. Exit
    // immediately, with the exit status set to the signal number with bit 8
    // set.  On the systems that we care about, this exit status is what is
    // normally used to indicate an exit by this signal's default handler.
    // This mechanism isn't a de jure standard, but even in the worst case, it
    // should at least result in an immediate exit.
    LOG(WARNING) << "Still here, exiting really ungracefully.";
    _exit(signal | (1 << 7));
  }

 private:
  const int shutdown_fd_;

  DISALLOW_COPY_AND_ASSIGN(ShutdownDetector);
};

}  // namespace
#endif  // OS_MACOSX

// This function provides some ways to test crash and assertion handling
// behavior of the renderer.
static void HandleRendererErrorTestParameters(const CommandLine& command_line) {
  // This parameter causes an assertion.
  if (command_line.HasSwitch(switches::kRendererAssertTest)) {
    DCHECK(false);
  }


#if !defined(OFFICIAL_BUILD)
  // This parameter causes an assertion too.
  if (command_line.HasSwitch(switches::kRendererCheckFalseTest)) {
    CHECK(false);
  }
#endif  // !defined(OFFICIAL_BUILD)


  // This parameter causes a null pointer crash (crash reporter trigger).
  if (command_line.HasSwitch(switches::kRendererCrashTest)) {
    int* bad_pointer = NULL;
    *bad_pointer = 0;
  }

  if (command_line.HasSwitch(switches::kRendererStartupDialog)) {
    ChildProcess::WaitForDebugger(L"Renderer");
  }
}

// mainline routine for running as the Renderer process
int RendererMain(const MainFunctionParams& parameters) {
  TRACE_EVENT_BEGIN("RendererMain", 0, "");

  const CommandLine& parsed_command_line = parameters.command_line_;
  base::mac::ScopedNSAutoreleasePool* pool = parameters.autorelease_pool_;

#if defined(OS_MACOSX)
  // TODO(viettrungluu): Code taken from browser_main.cc.
  int pipefd[2];
  int ret = pipe(pipefd);
  if (ret < 0) {
    PLOG(DFATAL) << "Failed to create pipe";
  } else {
    int shutdown_pipe_read_fd = pipefd[0];
    g_shutdown_pipe_write_fd = pipefd[1];
    const size_t kShutdownDetectorThreadStackSize = 4096;
    if (!base::PlatformThread::CreateNonJoinable(
            kShutdownDetectorThreadStackSize,
            new ShutdownDetector(shutdown_pipe_read_fd))) {
      LOG(DFATAL) << "Failed to create shutdown detector task.";
    }
  }

  // crbug.com/28547: When Breakpad is in use, handle SIGTERM to avoid leaking
  // Mach ports.
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  action.sa_handler = SIGTERMHandler;
  CHECK(sigaction(SIGTERM, &action, NULL) == 0);
#endif  // OS_MACOSX

#if defined(USE_LINUX_BREAKPAD)
  // Needs to be called after we have chrome::DIR_USER_DATA.
  InitCrashReporter();
#endif

  // Configure modules that need access to resources.
  net::NetModule::SetResourceProvider(chrome_common_net::NetResourceProvider);
  gfx::GfxModule::SetResourceProvider(chrome::GfxResourceProvider);

  // This function allows pausing execution using the --renderer-startup-dialog
  // flag allowing us to attach a debugger.
  // Do not move this function down since that would mean we can't easily debug
  // whatever occurs before it.
  HandleRendererErrorTestParameters(parsed_command_line);

  RendererMainPlatformDelegate platform(parameters);

  base::StatsScope<base::StatsCounterTimer>
      startup_timer(chrome::Counters::renderer_main());

#if defined(OS_MACOSX)
  // As long as we use Cocoa in the renderer (for the forseeable future as of
  // now; see http://crbug.com/13890 for info) we need to have a UI loop.
  MessageLoop main_message_loop(MessageLoop::TYPE_UI);
#else
  // The main message loop of the renderer services doesn't have IO or UI tasks,
  // unless in-process-plugins is used.
  MessageLoop main_message_loop(RenderProcessImpl::InProcessPlugins() ?
              MessageLoop::TYPE_UI : MessageLoop::TYPE_DEFAULT);
#endif

  base::PlatformThread::SetName("CrRendererMain");

  ui::SystemMonitor system_monitor;
  HighResolutionTimerManager hi_res_timer_manager;

  platform.PlatformInitialize();

  bool no_sandbox = parsed_command_line.HasSwitch(switches::kNoSandbox);
  platform.InitSandboxTests(no_sandbox);

  // Initialize histogram statistics gathering system.
  // Don't create StatisticsRecorder in the single process mode.
  scoped_ptr<base::StatisticsRecorder> statistics;
  if (!base::StatisticsRecorder::IsActive()) {
    statistics.reset(new base::StatisticsRecorder());
  }

  // Initialize statistical testing infrastructure.
  base::FieldTrialList field_trial;
  // Ensure any field trials in browser are reflected into renderer.
  if (parsed_command_line.HasSwitch(switches::kForceFieldTestNameAndValue)) {
    std::string persistent = parsed_command_line.GetSwitchValueASCII(
        switches::kForceFieldTestNameAndValue);
    bool ret = field_trial.CreateTrialsInChildProcess(persistent);
    DCHECK(ret);
  }

  // Load pepper plugins before engaging the sandbox.
  PepperPluginRegistry::GetInstance();

  {
#if !defined(OS_LINUX)
    // TODO(markus): Check if it is OK to unconditionally move this
    // instruction down.
    RenderProcessImpl render_process;
    render_process.set_main_thread(new RenderThread());
#endif
    bool run_loop = true;
    if (!no_sandbox) {
      run_loop = platform.EnableSandbox();
    } else {
      LOG(ERROR) << "Running without renderer sandbox";
    }
#if defined(OS_LINUX)
    RenderProcessImpl render_process;
    render_process.set_main_thread(new RenderThread());
#endif

    platform.RunSandboxTests();

    startup_timer.Stop();  // End of Startup Time Measurement.

    if (run_loop) {
      if (pool)
        pool->Recycle();
      TRACE_EVENT_BEGIN("RendererMain.START_MSG_LOOP", 0, 0);
      MessageLoop::current()->Run();
      TRACE_EVENT_END("RendererMain.START_MSG_LOOP", 0, 0);
    }
  }
  platform.PlatformUninitialize();
  TRACE_EVENT_END("RendererMain", 0, "");
  return 0;
}
