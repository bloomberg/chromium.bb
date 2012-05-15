// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/app/content_main_runner.h"

#include "base/allocator/allocator_extension.h"
#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/debug/trace_event.h"
#include "base/file_path.h"
#include "base/i18n/icu_util.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/stats_table.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/stringprintf.h"
#include "base/string_number_conversions.h"
#include "content/browser/browser_main.h"
#include "content/common/set_process_title.h"
#include "content/public/app/content_main_delegate.h"
#include "content/public/app/startup_helper_win.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "content/public/common/sandbox_init.h"
#include "content/public/common/url_constants.h"
#include "crypto/nss_util.h"
#include "ipc/ipc_switches.h"
#include "media/base/media.h"
#include "sandbox/src/sandbox_types.h"
#include "ui/base/ui_base_switches.h"
#include "ui/base/ui_base_paths.h"
#include "ui/base/win/dpi.h"
#include "webkit/glue/webkit_glue.h"

#if defined(USE_TCMALLOC)
#include "third_party/tcmalloc/chromium/src/gperftools/malloc_extension.h"
#endif

#if defined(OS_WIN)
#include <atlbase.h>
#include <atlapp.h>
#include <malloc.h>
#elif defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/mach_ipc_mac.h"
#include "base/system_monitor/system_monitor.h"
#include "content/browser/mach_broker_mac.h"
#include "content/common/sandbox_init_mac.h"
#endif  // OS_WIN

#if defined(OS_POSIX)
#include <signal.h>

#include "base/global_descriptors_posix.h"
#include "content/common/chrome_descriptors.h"

#if !defined(OS_MACOSX)
#include "content/public/common/zygote_fork_delegate_linux.h"
#endif

#endif  // OS_POSIX

#if !defined(OS_MACOSX) && defined(USE_TCMALLOC)
extern "C" {
int tc_set_new_mode(int mode);
}
#endif

extern int GpuMain(const content::MainFunctionParams&);
extern int PluginMain(const content::MainFunctionParams&);
extern int PpapiPluginMain(const content::MainFunctionParams&);
extern int PpapiBrokerMain(const content::MainFunctionParams&);
extern int RendererMain(const content::MainFunctionParams&);
extern int WorkerMain(const content::MainFunctionParams&);
extern int UtilityMain(const content::MainFunctionParams&);
#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
namespace content {
extern int ZygoteMain(const content::MainFunctionParams&,
                      content::ZygoteForkDelegate* forkdelegate);
}  // namespace content
#endif

namespace {

#if defined(OS_WIN)

static CAppModule _Module;

#elif defined(OS_MACOSX)

// Completes the Mach IPC handshake by sending this process' task port to the
// parent process.  The parent is listening on the Mach port given by
// |GetMachPortName()|.  The task port is used by the parent to get CPU/memory
// stats to display in the task manager.
void SendTaskPortToParentProcess() {
  const mach_msg_timeout_t kTimeoutMs = 100;
  const int32_t kMessageId = 0;
  std::string mach_port_name = MachBroker::GetMachPortName();

  base::MachSendMessage child_message(kMessageId);
  if (!child_message.AddDescriptor(mach_task_self())) {
    LOG(ERROR) << "child AddDescriptor(mach_task_self()) failed.";
    return;
  }

  base::MachPortSender child_sender(mach_port_name.c_str());
  kern_return_t err = child_sender.SendMessage(child_message, kTimeoutMs);
  if (err != KERN_SUCCESS) {
    LOG(ERROR) << StringPrintf("child SendMessage() failed: 0x%x %s", err,
                               mach_error_string(err));
  }
}

#endif  // defined(OS_WIN)

#if defined(OS_POSIX)

// Setup signal-handling state: resanitize most signals, ignore SIGPIPE.
void SetupSignalHandlers() {
  // Sanitise our signal handling state. Signals that were ignored by our
  // parent will also be ignored by us. We also inherit our parent's sigmask.
  sigset_t empty_signal_set;
  CHECK(0 == sigemptyset(&empty_signal_set));
  CHECK(0 == sigprocmask(SIG_SETMASK, &empty_signal_set, NULL));

  struct sigaction sigact;
  memset(&sigact, 0, sizeof(sigact));
  sigact.sa_handler = SIG_DFL;
  static const int signals_to_reset[] =
      {SIGHUP, SIGINT, SIGQUIT, SIGILL, SIGABRT, SIGFPE, SIGSEGV,
       SIGALRM, SIGTERM, SIGCHLD, SIGBUS, SIGTRAP};  // SIGPIPE is set below.
  for (unsigned i = 0; i < arraysize(signals_to_reset); i++) {
    CHECK(0 == sigaction(signals_to_reset[i], &sigact, NULL));
  }

  // Always ignore SIGPIPE.  We check the return value of write().
  CHECK(signal(SIGPIPE, SIG_IGN) != SIG_ERR);
}

#endif  // OS_POSIX

void CommonSubprocessInit(const std::string& process_type) {
#if defined(OS_WIN)
  // HACK: Let Windows know that we have started.  This is needed to suppress
  // the IDC_APPSTARTING cursor from being displayed for a prolonged period
  // while a subprocess is starting.
  PostThreadMessage(GetCurrentThreadId(), WM_NULL, 0, 0);
  MSG msg;
  PeekMessage(&msg, NULL, 0, 0, PM_REMOVE);
#endif
#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
  // Various things break when you're using a locale where the decimal
  // separator isn't a period.  See e.g. bugs 22782 and 39964.  For
  // all processes except the browser process (where we call system
  // APIs that may rely on the correct locale for formatting numbers
  // when presenting them to the user), reset the locale for numeric
  // formatting.
  // Note that this is not correct for plugin processes -- they can
  // surface UI -- but it's likely they get this wrong too so why not.
  setlocale(LC_NUMERIC, "C");
#endif
}

void InitializeStatsTable(base::ProcessId browser_pid,
                          const CommandLine& command_line) {
  // Initialize the Stats Counters table.  With this initialized,
  // the StatsViewer can be utilized to read counters outside of
  // Chrome.  These lines can be commented out to effectively turn
  // counters 'off'.  The table is created and exists for the life
  // of the process.  It is not cleaned up.
  if (command_line.HasSwitch(switches::kEnableStatsTable)) {
    // NOTIMPLEMENTED: we probably need to shut this down correctly to avoid
    // leaking shared memory regions on posix platforms.
    std::string statsfile =
        base::StringPrintf("%s-%u",
                           content::kStatsFilename,
                           static_cast<unsigned int>(browser_pid));
    base::StatsTable* stats_table = new base::StatsTable(statsfile,
        content::kStatsMaxThreads, content::kStatsMaxCounters);
    base::StatsTable::set_current(stats_table);
  }
}

// We dispatch to a process-type-specific FooMain() based on a command-line
// flag.  This struct is used to build a table of (flag, main function) pairs.
struct MainFunction {
  const char* name;
  int (*function)(const content::MainFunctionParams&);
};

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
// On platforms that use the zygote, we have a special subset of
// subprocesses that are launched via the zygote.  This function
// fills in some process-launching bits around ZygoteMain().
// Returns the exit code of the subprocess.
int RunZygote(const content::MainFunctionParams& main_function_params,
              content::ContentMainDelegate* delegate) {
  static const MainFunction kMainFunctions[] = {
    { switches::kRendererProcess,    RendererMain },
    { switches::kWorkerProcess,      WorkerMain },
    { switches::kPpapiPluginProcess, PpapiPluginMain },
    { switches::kUtilityProcess,     UtilityMain },
  };

  scoped_ptr<content::ZygoteForkDelegate> zygote_fork_delegate;
  if (delegate) {
    zygote_fork_delegate.reset(delegate->ZygoteStarting());
    // Each Renderer we spawn will re-attempt initialization of the media
    // libraries, at which point failure will be detected and handled, so
    // we do not need to cope with initialization failures here.
    FilePath media_path;
    if (PathService::Get(content::DIR_MEDIA_LIBS, &media_path))
      media::InitializeMediaLibrary(media_path);
  }

  // This function call can return multiple times, once per fork().
  if (!content::ZygoteMain(main_function_params, zygote_fork_delegate.get()))
    return 1;

  if (delegate) delegate->ZygoteForked();

  // Zygote::HandleForkRequest may have reallocated the command
  // line so update it here with the new version.
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  // If a custom user agent was passed on the command line, we need
  // to (re)set it now, rather than using the default one the zygote
  // initialized.
  if (command_line.HasSwitch(switches::kUserAgent)) {
    webkit_glue::SetUserAgent(
        command_line.GetSwitchValueASCII(switches::kUserAgent), true);
  }

  // The StatsTable must be initialized in each process; we already
  // initialized for the browser process, now we need to initialize
  // within the new processes as well.
  pid_t browser_pid = base::GetParentProcessId(
      base::GetParentProcessId(base::GetCurrentProcId()));
  InitializeStatsTable(browser_pid, command_line);

  content::MainFunctionParams main_params(command_line);

  // Get the new process type from the new command line.
  std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);

  for (size_t i = 0; i < arraysize(kMainFunctions); ++i) {
    if (process_type == kMainFunctions[i].name)
      return kMainFunctions[i].function(main_params);
  }

  if (delegate)
    return delegate->RunProcess(process_type, main_params);

  NOTREACHED() << "Unknown zygote process type: " << process_type;
  return 1;
}
#endif  // defined(OS_POSIX) && !defined(OS_MACOSX)

// Run the FooMain() for a given process type.
// If |process_type| is empty, runs BrowserMain().
// Returns the exit code for this process.
int RunNamedProcessTypeMain(
    const std::string& process_type,
    const content::MainFunctionParams& main_function_params,
    content::ContentMainDelegate* delegate) {
  static const MainFunction kMainFunctions[] = {
    { "",                            BrowserMain },
    { switches::kRendererProcess,    RendererMain },
    { switches::kPluginProcess,      PluginMain },
    { switches::kWorkerProcess,      WorkerMain },
    { switches::kPpapiPluginProcess, PpapiPluginMain },
    { switches::kPpapiBrokerProcess, PpapiBrokerMain },
    { switches::kUtilityProcess,     UtilityMain },
    { switches::kGpuProcess,         GpuMain },
  };

  for (size_t i = 0; i < arraysize(kMainFunctions); ++i) {
    if (process_type == kMainFunctions[i].name) {
      if (delegate) {
        int exit_code = delegate->RunProcess(process_type,
            main_function_params);
        if (exit_code >= 0)
          return exit_code;
      }
      return kMainFunctions[i].function(main_function_params);
    }
  }

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
  // Zygote startup is special -- see RunZygote comments above
  // for why we don't use ZygoteMain directly.
  if (process_type == switches::kZygoteProcess)
    return RunZygote(main_function_params, delegate);
#endif

  // If it's a process we don't know about, the embedder should know.
  if (delegate)
    return delegate->RunProcess(process_type, main_function_params);

  NOTREACHED() << "Unknown process type: " << process_type;
  return 1;
}

class ContentMainRunnerImpl : public content::ContentMainRunner {
 public:
  ContentMainRunnerImpl()
      : is_initialized_(false),
        is_shutdown_(false),
        completed_basic_startup_(false) {
  }

  ~ContentMainRunnerImpl() {
    if (is_initialized_ && !is_shutdown_)
      Shutdown();
  }

#if defined(USE_TCMALLOC)
static void GetStatsThunk(char* buffer, int buffer_length) {
  MallocExtension::instance()->GetStats(buffer, buffer_length);
}

static void ReleaseFreeMemoryThunk() {
  MallocExtension::instance()->ReleaseFreeMemory();
}
#endif


#if defined(OS_WIN)
  virtual int Initialize(HINSTANCE instance,
                         sandbox::SandboxInterfaceInfo* sandbox_info,
                         content::ContentMainDelegate* delegate) OVERRIDE {
    // argc/argv are ignored on Windows; see command_line.h for details.
    int argc = 0;
    char** argv = NULL;

    content::RegisterInvalidParamHandler();
    _Module.Init(NULL, static_cast<HINSTANCE>(instance));

    if (sandbox_info)
      sandbox_info_ = *sandbox_info;
    else
      memset(&sandbox_info_, 0, sizeof(sandbox_info_));

#else  // !OS_WIN
  virtual int Initialize(int argc,
                         const char** argv,
                         content::ContentMainDelegate* delegate) OVERRIDE {

    // NOTE(willchan): One might ask why this call is done here rather than in
    // process_util_linux.cc with the definition of
    // EnableTerminationOnOutOfMemory().  That's because base shouldn't have a
    // dependency on TCMalloc.  Really, we ought to have our allocator shim code
    // implement this EnableTerminationOnOutOfMemory() function.  Whateverz.
    // This works for now.
#if !defined(OS_MACOSX) && defined(USE_TCMALLOC)
    // For tcmalloc, we need to tell it to behave like new.
    tc_set_new_mode(1);

    // On windows, we've already set these thunks up in _heap_init()
    base::allocator::SetGetStatsFunction(GetStatsThunk);
    base::allocator::SetReleaseFreeMemoryFunction(ReleaseFreeMemoryThunk);
#endif

    // On Android,
    // - setlocale() is not supported.
    // - We do not override the signal handlers so that we can get
    //   stack trace when crashing.
    // - The ipc_fd is passed through the Java service.
    // Thus, these are all disabled.
#if !defined(OS_ANDROID)
    // Set C library locale to make sure CommandLine can parse argument values
    // in correct encoding.
    setlocale(LC_ALL, "");

    SetupSignalHandlers();

    base::GlobalDescriptors* g_fds = base::GlobalDescriptors::GetInstance();
    g_fds->Set(kPrimaryIPCChannel,
               kPrimaryIPCChannel + base::GlobalDescriptors::kBaseDescriptor);
#endif

#if defined(OS_LINUX) || defined(OS_OPENBSD)
    g_fds->Set(kCrashDumpSignal,
               kCrashDumpSignal + base::GlobalDescriptors::kBaseDescriptor);
#endif

#endif  // !OS_WIN

    is_initialized_ = true;
    delegate_ = delegate;

    base::EnableTerminationOnHeapCorruption();
    base::EnableTerminationOnOutOfMemory();

    // On Android, AtExitManager is set up when library is loaded.
#if !defined(OS_ANDROID)
    // The exit manager is in charge of calling the dtors of singleton objects.
    exit_manager_.reset(new base::AtExitManager);
#endif

#if defined(OS_MACOSX)
    // We need this pool for all the objects created before we get to the
    // event loop, but we don't want to leave them hanging around until the
    // app quits. Each "main" needs to flush this pool right before it goes into
    // its main event loop to get rid of the cruft.
    autorelease_pool_.reset(new base::mac::ScopedNSAutoreleasePool());
#endif

    // On Android, the command line is initialized when library is loaded.
    // (But *is* initialized here for content shell bringup)
#if !defined(OS_ANDROID) || defined(ANDROID_UPSTREAM_BRINGUP)
    CommandLine::Init(argc, argv);
#endif

    int exit_code;
    if (delegate && delegate->BasicStartupComplete(&exit_code))
      return exit_code;
    DCHECK(!delegate || content::GetContentClient()) <<
        "BasicStartupComplete didn't set the content client";

    completed_basic_startup_ = true;

    const CommandLine& command_line = *CommandLine::ForCurrentProcess();
    std::string process_type =
          command_line.GetSwitchValueASCII(switches::kProcessType);

    // Enable startup tracing asap to avoid early TRACE_EVENT calls being
    // ignored.
    if (command_line.HasSwitch(switches::kTraceStartup)) {
      base::debug::TraceLog::GetInstance()->SetEnabled(
          command_line.GetSwitchValueASCII(switches::kTraceStartup));
    }

#if defined(OS_MACOSX)
    // We need to allocate the IO Ports before the Sandbox is initialized or
    // the first instance of SystemMonitor is created.
    // It's important not to allocate the ports for processes which don't
    // register with the system monitor - see crbug.com/88867.
    if (process_type.empty() ||
        process_type == switches::kPluginProcess ||
        process_type == switches::kRendererProcess ||
        process_type == switches::kUtilityProcess ||
        process_type == switches::kWorkerProcess ||
        (delegate &&
         delegate->ProcessRegistersWithSystemProcess(process_type))) {
      base::SystemMonitor::AllocateSystemIOPorts();
    }

    if (!process_type.empty() &&
        (!delegate || delegate->ShouldSendMachPort(process_type))) {
      SendTaskPortToParentProcess();
    }
#elif defined(OS_WIN)
#if defined(ENABLE_HIDPI)
    ui::EnableHighDPISupport();
#endif
    content::SetupCRT(command_line);
#endif

#if defined(OS_POSIX)
    if (!process_type.empty()) {
      // When you hit Ctrl-C in a terminal running the browser
      // process, a SIGINT is delivered to the entire process group.
      // When debugging the browser process via gdb, gdb catches the
      // SIGINT for the browser process (and dumps you back to the gdb
      // console) but doesn't for the child processes, killing them.
      // The fix is to have child processes ignore SIGINT; they'll die
      // on their own when the browser process goes away.
      //
      // Note that we *can't* rely on BeingDebugged to catch this case because
      // we are the child process, which is not being debugged.
      // TODO(evanm): move this to some shared subprocess-init function.
      if (!base::debug::BeingDebugged())
        signal(SIGINT, SIG_IGN);
    }
#endif

#if defined(USE_NSS)
    crypto::EarlySetupForNSSInit();
#endif

    ui::RegisterPathProvider();
    content::RegisterPathProvider();
    content::RegisterContentSchemes(true);

    CHECK(icu_util::Initialize());

    base::ProcessId browser_pid = base::GetCurrentProcId();
    if (command_line.HasSwitch(switches::kProcessChannelID)) {
#if defined(OS_WIN) || defined(OS_MACOSX)
      std::string channel_name =
          command_line.GetSwitchValueASCII(switches::kProcessChannelID);

      int browser_pid_int;
      base::StringToInt(channel_name, &browser_pid_int);
      browser_pid = static_cast<base::ProcessId>(browser_pid_int);
      DCHECK_NE(browser_pid_int, 0);
#elif defined(OS_POSIX)
      // On linux, we're in the zygote here; so we need the parent process' id.
      browser_pid = base::GetParentProcessId(base::GetCurrentProcId());
#endif
    }

    InitializeStatsTable(browser_pid, command_line);

    if (delegate)
      delegate->PreSandboxStartup();

    // Set any custom user agent passed on the command line now so the string
    // doesn't change between calls to webkit_glue::GetUserAgent(), otherwise it
    // defaults to the user agent set during SetContentClient().
    if (command_line.HasSwitch(switches::kUserAgent)) {
      webkit_glue::SetUserAgent(
          command_line.GetSwitchValueASCII(switches::kUserAgent), true);
    }

    if (!process_type.empty())
      CommonSubprocessInit(process_type);

#if defined(OS_WIN)
    CHECK(content::InitializeSandbox(sandbox_info));
#elif defined(OS_MACOSX)
    if (process_type == switches::kRendererProcess ||
        process_type == switches::kPpapiPluginProcess ||
        (delegate && delegate->DelaySandboxInitialization(process_type))) {
      // On OS X the renderer sandbox needs to be initialized later in the
      // startup sequence in RendererMainPlatformDelegate::EnableSandbox().
    } else {
      CHECK(content::InitializeSandbox());
    }
#endif

    if (delegate)
      delegate->SandboxInitialized(process_type);

#if defined(OS_POSIX)
    SetProcessTitleFromCommandLine(argv);
#endif

    // Return -1 to indicate no early termination.
    return -1;
  }

  virtual int Run() OVERRIDE {
    DCHECK(is_initialized_);
    DCHECK(!is_shutdown_);
    const CommandLine& command_line = *CommandLine::ForCurrentProcess();
    std::string process_type =
          command_line.GetSwitchValueASCII(switches::kProcessType);

    content::MainFunctionParams main_params(command_line);
#if defined(OS_WIN)
    main_params.sandbox_info = &sandbox_info_;
#elif defined(OS_MACOSX)
    main_params.autorelease_pool = autorelease_pool_.get();
#endif

    return RunNamedProcessTypeMain(process_type, main_params, delegate_);
  }

  virtual void Shutdown() OVERRIDE {
    DCHECK(is_initialized_);
    DCHECK(!is_shutdown_);

    if (completed_basic_startup_ && delegate_) {
      const CommandLine& command_line = *CommandLine::ForCurrentProcess();
      std::string process_type =
          command_line.GetSwitchValueASCII(switches::kProcessType);

      delegate_->ProcessExiting(process_type);
    }

#if defined(OS_WIN)
#ifdef _CRTDBG_MAP_ALLOC
    _CrtDumpMemoryLeaks();
#endif  // _CRTDBG_MAP_ALLOC

    _Module.Term();
#endif  // OS_WIN

#if defined(OS_MACOSX)
    autorelease_pool_.reset(NULL);
#endif

    exit_manager_.reset(NULL);

    delegate_ = NULL;
    is_shutdown_ = true;
  }

 protected:
  // True if the runner has been initialized.
  bool is_initialized_;

  // True if the runner has been shut down.
  bool is_shutdown_;

  // True if basic startup was completed.
  bool completed_basic_startup_;

  // The delegate will outlive this object.
  content::ContentMainDelegate* delegate_;

  scoped_ptr<base::AtExitManager> exit_manager_;
#if defined(OS_WIN)
  sandbox::SandboxInterfaceInfo sandbox_info_;
#elif defined(OS_MACOSX)
  scoped_ptr<base::mac::ScopedNSAutoreleasePool> autorelease_pool_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ContentMainRunnerImpl);
};

}  // namespace

namespace content {

// static
ContentMainRunner* ContentMainRunner::Create() {
  return new ContentMainRunnerImpl();
}

}  // namespace content
