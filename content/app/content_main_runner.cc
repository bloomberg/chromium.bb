// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/app/content_main_runner.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/allocator/allocator_check.h"
#include "base/allocator/allocator_extension.h"
#include "base/allocator/features.h"
#include "base/at_exit.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/debug/stack_trace.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/i18n/icu_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/statistics_recorder.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/memory.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/tracing/common/trace_startup.h"
#include "content/app/mojo/mojo_init.h"
#include "content/common/url_schemes.h"
#include "content/public/app/content_main.h"
#include "content/public/app/content_main_delegate.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_descriptor_keys.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "content/public/common/sandbox_init.h"
#include "gin/v8_initializer.h"
#include "media/base/media.h"
#include "media/media_features.h"
#include "ppapi/features/features.h"
#include "services/service_manager/embedder/switches.h"
#include "services/service_manager/sandbox/sandbox_type.h"
#include "ui/base/ui_base_paths.h"
#include "ui/base/ui_base_switches.h"

#if defined(OS_WIN)
#include <malloc.h>
#include <cstring>

#include "base/trace_event/trace_event_etw_export_win.h"
#include "sandbox/win/src/sandbox_types.h"
#include "ui/display/win/dpi.h"
#elif defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/power_monitor/power_monitor_device_source.h"
#include "content/browser/mach_broker_mac.h"
#endif  // OS_WIN

#if defined(OS_POSIX)
#include <signal.h>

#include "base/file_descriptor_store.h"
#include "base/posix/global_descriptors.h"
#include "content/public/common/content_descriptors.h"

#if !defined(OS_MACOSX)
#include "content/public/common/zygote_fork_delegate_linux.h"
#endif
#if !defined(OS_MACOSX) && !defined(OS_ANDROID)
#include "content/zygote/zygote_main.h"
#endif

#endif  // OS_POSIX

#if !defined(CHROME_MULTIPLE_DLL_BROWSER)
#include "content/public/gpu/content_gpu_client.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/utility/content_utility_client.h"
#endif

#if !defined(CHROME_MULTIPLE_DLL_CHILD)
#include "content/browser/browser_main.h"
#include "content/public/browser/content_browser_client.h"
#endif

#if !defined(CHROME_MULTIPLE_DLL_BROWSER) && !defined(CHROME_MULTIPLE_DLL_CHILD)
#include "content/browser/gpu/gpu_main_thread_factory.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/utility_process_host_impl.h"
#include "content/gpu/in_process_gpu_thread.h"
#include "content/renderer/in_process_renderer_thread.h"
#include "content/utility/in_process_utility_thread.h"
#endif

namespace content {
extern int GpuMain(const content::MainFunctionParams&);
#if BUILDFLAG(ENABLE_PLUGINS)
#if !defined(OS_LINUX)
extern int PluginMain(const content::MainFunctionParams&);
#endif
extern int PpapiPluginMain(const MainFunctionParams&);
extern int PpapiBrokerMain(const MainFunctionParams&);
#endif
extern int RendererMain(const content::MainFunctionParams&);
extern int UtilityMain(const MainFunctionParams&);
}  // namespace content

namespace content {

namespace {

#if defined(V8_USE_EXTERNAL_STARTUP_DATA) && defined(OS_ANDROID)
#if defined __LP64__
#define kV8SnapshotDataDescriptor kV8Snapshot64DataDescriptor
#else
#define kV8SnapshotDataDescriptor kV8Snapshot32DataDescriptor
#endif
#endif

// This sets up two singletons responsible for managing field trials. The
// |field_trial_list| singleton lives on the stack and must outlive the Run()
// method of the process.
void InitializeFieldTrialAndFeatureList(
    std::unique_ptr<base::FieldTrialList>* field_trial_list) {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  // Initialize statistical testing infrastructure.  We set the entropy
  // provider to nullptr to disallow non-browser processes from creating
  // their own one-time randomized trials; they should be created in the
  // browser process.
  field_trial_list->reset(new base::FieldTrialList(nullptr));

  // Ensure any field trials in browser are reflected into the child
  // process.
#if defined(OS_POSIX)
  // On POSIX systems that use the zygote, we get the trials from a shared
  // memory segment backed by an fd instead of the command line.
  base::FieldTrialList::CreateTrialsFromCommandLine(
      command_line, switches::kFieldTrialHandle, kFieldTrialDescriptor);
#else
  base::FieldTrialList::CreateTrialsFromCommandLine(
      command_line, switches::kFieldTrialHandle, -1);
#endif

  std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
  base::FieldTrialList::CreateFeaturesFromCommandLine(
      command_line, switches::kEnableFeatures, switches::kDisableFeatures,
      feature_list.get());
  base::FeatureList::SetInstance(std::move(feature_list));
}

void LoadV8ContextSnapshotFile() {
#if defined(OS_POSIX) && !defined(OS_MACOSX)
  base::FileDescriptorStore& file_descriptor_store =
      base::FileDescriptorStore::GetInstance();
  base::MemoryMappedFile::Region region;
  base::ScopedFD fd = file_descriptor_store.MaybeTakeFD(
      kV8ContextSnapshotDataDescriptor, &region);
  if (fd.is_valid()) {
    gin::V8Initializer::LoadV8ContextSnapshotFromFD(fd.get(), region.offset,
                                                    region.size);
    return;
  }
#endif  // OS
#if !defined(CHROME_MULTIPLE_DLL_BROWSER)
  gin::V8Initializer::LoadV8ContextSnapshot();
#endif
}

void LoadV8SnapshotFile() {
#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
#if defined(OS_POSIX) && !defined(OS_MACOSX)
  base::FileDescriptorStore& file_descriptor_store =
      base::FileDescriptorStore::GetInstance();
  base::MemoryMappedFile::Region region;
  base::ScopedFD fd =
      file_descriptor_store.MaybeTakeFD(kV8SnapshotDataDescriptor, &region);
  if (fd.is_valid()) {
    gin::V8Initializer::LoadV8SnapshotFromFD(fd.get(), region.offset,
                                             region.size);
    return;
  }
#endif  // OS_POSIX && !OS_MACOSX
#if !defined(CHROME_MULTIPLE_DLL_BROWSER)
  gin::V8Initializer::LoadV8Snapshot();
#endif
#endif  // V8_USE_EXTERNAL_STARTUP_DATA
}

void LoadV8NativesFile() {
#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
#if defined(OS_POSIX) && !defined(OS_MACOSX)
  base::FileDescriptorStore& file_descriptor_store =
      base::FileDescriptorStore::GetInstance();
  base::MemoryMappedFile::Region region;
  base::ScopedFD fd =
      file_descriptor_store.MaybeTakeFD(kV8NativesDataDescriptor, &region);
  if (fd.is_valid()) {
    gin::V8Initializer::LoadV8NativesFromFD(fd.get(), region.offset,
                                            region.size);
    return;
  }
#endif  // OS_POSIX && !OS_MACOSX
#if !defined(CHROME_MULTIPLE_DLL_BROWSER)
  gin::V8Initializer::LoadV8Natives();
#endif
#endif  // V8_USE_EXTERNAL_STARTUP_DATA
}

void InitializeV8IfNeeded(const base::CommandLine& command_line,
                          const std::string& process_type) {
  if (process_type == switches::kGpuProcess)
    return;

  LoadV8SnapshotFile();
  LoadV8NativesFile();
  LoadV8ContextSnapshotFile();
}

}  // namespace

#if !defined(CHROME_MULTIPLE_DLL_CHILD)
base::LazyInstance<ContentBrowserClient>::DestructorAtExit
    g_empty_content_browser_client = LAZY_INSTANCE_INITIALIZER;
#endif  //  !CHROME_MULTIPLE_DLL_CHILD

#if !defined(CHROME_MULTIPLE_DLL_BROWSER)
base::LazyInstance<ContentGpuClient>::DestructorAtExit
    g_empty_content_gpu_client = LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<ContentRendererClient>::DestructorAtExit
    g_empty_content_renderer_client = LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<ContentUtilityClient>::DestructorAtExit
    g_empty_content_utility_client = LAZY_INSTANCE_INITIALIZER;
#endif  // !CHROME_MULTIPLE_DLL_BROWSER

class ContentClientInitializer {
 public:
  static void Set(const std::string& process_type,
                  ContentMainDelegate* delegate) {
    ContentClient* content_client = GetContentClient();
#if !defined(CHROME_MULTIPLE_DLL_CHILD)
    if (process_type.empty()) {
      if (delegate)
        content_client->browser_ = delegate->CreateContentBrowserClient();
      if (!content_client->browser_)
        content_client->browser_ = &g_empty_content_browser_client.Get();
    }
#endif  // !CHROME_MULTIPLE_DLL_CHILD

#if !defined(CHROME_MULTIPLE_DLL_BROWSER)
    base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
    if (process_type == switches::kGpuProcess ||
        cmd->HasSwitch(switches::kSingleProcess) ||
        (process_type.empty() && cmd->HasSwitch(switches::kInProcessGPU))) {
      if (delegate)
        content_client->gpu_ = delegate->CreateContentGpuClient();
      if (!content_client->gpu_)
        content_client->gpu_ = &g_empty_content_gpu_client.Get();
    }

    if (process_type == switches::kRendererProcess ||
        cmd->HasSwitch(switches::kSingleProcess)) {
      if (delegate)
        content_client->renderer_ = delegate->CreateContentRendererClient();
      if (!content_client->renderer_)
        content_client->renderer_ = &g_empty_content_renderer_client.Get();
    }

    if (process_type == switches::kUtilityProcess ||
        cmd->HasSwitch(switches::kSingleProcess)) {
      if (delegate)
        content_client->utility_ = delegate->CreateContentUtilityClient();
      // TODO(scottmg): http://crbug.com/237249 Should be in _child.
      if (!content_client->utility_)
        content_client->utility_ = &g_empty_content_utility_client.Get();
    }
#endif  // !CHROME_MULTIPLE_DLL_BROWSER
  }
};

// We dispatch to a process-type-specific FooMain() based on a command-line
// flag.  This struct is used to build a table of (flag, main function) pairs.
struct MainFunction {
  const char* name;
  int (*function)(const MainFunctionParams&);
};

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID) && \
    !defined(OS_FUCHSIA)
// On platforms that use the zygote, we have a special subset of
// subprocesses that are launched via the zygote.  This function
// fills in some process-launching bits around ZygoteMain().
// Returns the exit code of the subprocess.
int RunZygote(const MainFunctionParams& main_function_params,
              ContentMainDelegate* delegate) {
  static const MainFunction kMainFunctions[] = {
    { switches::kRendererProcess,    RendererMain },
#if BUILDFLAG(ENABLE_PLUGINS)
    { switches::kPpapiPluginProcess, PpapiPluginMain },
#endif
    { switches::kUtilityProcess,     UtilityMain },
  };

  std::vector<std::unique_ptr<ZygoteForkDelegate>> zygote_fork_delegates;
  if (delegate) {
    delegate->ZygoteStarting(&zygote_fork_delegates);
    media::InitializeMediaLibrary();
  }

  // This function call can return multiple times, once per fork().
  if (!ZygoteMain(main_function_params, std::move(zygote_fork_delegates)))
    return 1;

  if (delegate) delegate->ZygoteForked();

  // Zygote::HandleForkRequest may have reallocated the command
  // line so update it here with the new version.
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);
  ContentClientInitializer::Set(process_type, delegate);

  MainFunctionParams main_params(command_line);
  main_params.zygote_child = true;

  std::unique_ptr<base::FieldTrialList> field_trial_list;
  InitializeFieldTrialAndFeatureList(&field_trial_list);

  service_manager::SandboxType sandbox_type =
      service_manager::SandboxTypeFromCommandLine(command_line);
  if (sandbox_type == service_manager::SANDBOX_TYPE_PROFILING)
    content::DisableLocaltimeOverride();

  for (size_t i = 0; i < arraysize(kMainFunctions); ++i) {
    if (process_type == kMainFunctions[i].name)
      return kMainFunctions[i].function(main_params);
  }

  if (delegate)
    return delegate->RunProcess(process_type, main_params);

  NOTREACHED() << "Unknown zygote process type: " << process_type;
  return 1;
}
#endif  // defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID) && \
//         !defined(OS_FUCHSIA)

static void RegisterMainThreadFactories() {
#if !defined(CHROME_MULTIPLE_DLL_BROWSER) && !defined(CHROME_MULTIPLE_DLL_CHILD)
  UtilityProcessHostImpl::RegisterUtilityMainThreadFactory(
      CreateInProcessUtilityThread);
  RenderProcessHostImpl::RegisterRendererMainThreadFactory(
      CreateInProcessRendererThread);
  content::RegisterGpuMainThreadFactory(CreateInProcessGpuThread);
#else
  base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kSingleProcess)) {
    LOG(FATAL) <<
        "--single-process is not supported in chrome multiple dll browser.";
  }
  if (command_line.HasSwitch(switches::kInProcessGPU)) {
    LOG(FATAL) <<
        "--in-process-gpu is not supported in chrome multiple dll browser.";
  }
#endif  // !CHROME_MULTIPLE_DLL_BROWSER && !CHROME_MULTIPLE_DLL_CHILD
}

// Run the FooMain() for a given process type.
// If |process_type| is empty, runs BrowserMain().
// Returns the exit code for this process.
int RunNamedProcessTypeMain(
    const std::string& process_type,
    const MainFunctionParams& main_function_params,
    ContentMainDelegate* delegate) {
  static const MainFunction kMainFunctions[] = {
#if !defined(CHROME_MULTIPLE_DLL_CHILD)
    { "",                            BrowserMain },
#endif
#if !defined(CHROME_MULTIPLE_DLL_BROWSER)
#if BUILDFLAG(ENABLE_PLUGINS)
    { switches::kPpapiPluginProcess, PpapiPluginMain },
    { switches::kPpapiBrokerProcess, PpapiBrokerMain },
#endif  // ENABLE_PLUGINS
    { switches::kUtilityProcess,     UtilityMain },
    { switches::kRendererProcess,    RendererMain },
    { switches::kGpuProcess,         GpuMain },
#endif  // !CHROME_MULTIPLE_DLL_BROWSER
  };

  RegisterMainThreadFactories();

  for (size_t i = 0; i < arraysize(kMainFunctions); ++i) {
    if (process_type == kMainFunctions[i].name) {
      if (delegate) {
        int exit_code = delegate->RunProcess(process_type,
            main_function_params);
#if defined(OS_ANDROID)
        // In Android's browser process, the negative exit code doesn't mean the
        // default behavior should be used as the UI message loop is managed by
        // the Java and the browser process's default behavior is always
        // overridden.
        if (process_type.empty())
          return exit_code;
#endif
        if (exit_code >= 0)
          return exit_code;
      }
      return kMainFunctions[i].function(main_function_params);
    }
  }

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID) && \
    !defined(OS_FUCHSIA)
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

class ContentMainRunnerImpl : public ContentMainRunner {
 public:
  ContentMainRunnerImpl()
      : is_initialized_(false),
        is_shutdown_(false),
        completed_basic_startup_(false),
        delegate_(NULL),
        ui_task_(NULL) {
#if defined(OS_WIN)
    memset(&sandbox_info_, 0, sizeof(sandbox_info_));
#endif
  }

  ~ContentMainRunnerImpl() override {
    if (is_initialized_ && !is_shutdown_)
      Shutdown();
  }

  int Initialize(const ContentMainParams& params) override {
    ui_task_ = params.ui_task;

    create_discardable_memory_ = params.create_discardable_memory;

#if defined(USE_AURA)
    env_mode_ = params.env_mode;
#endif

#if defined(OS_WIN)
    sandbox_info_ = *params.sandbox_info;
#else  // !OS_WIN

#if defined(OS_MACOSX)
    autorelease_pool_ = params.autorelease_pool;
#endif  // defined(OS_MACOSX)

#if defined(OS_ANDROID)
    // See note at the initialization of ExitManager, below; basically,
    // only Android builds have the ctor/dtor handlers set up to use
    // TRACE_EVENT right away.
    TRACE_EVENT0("startup,benchmark,rail", "ContentMainRunnerImpl::Initialize");
#endif  // OS_ANDROID

    base::GlobalDescriptors* g_fds = base::GlobalDescriptors::GetInstance();
    ALLOW_UNUSED_LOCAL(g_fds);

// On Android, the ipc_fd is passed through the Java service.
#if !defined(OS_ANDROID)
    g_fds->Set(kMojoIPCChannel,
               kMojoIPCChannel + base::GlobalDescriptors::kBaseDescriptor);

    g_fds->Set(
        kFieldTrialDescriptor,
        kFieldTrialDescriptor + base::GlobalDescriptors::kBaseDescriptor);
#endif  // !OS_ANDROID

#if defined(OS_LINUX) || defined(OS_OPENBSD)
    g_fds->Set(kCrashDumpSignal,
               kCrashDumpSignal + base::GlobalDescriptors::kBaseDescriptor);
#endif  // OS_LINUX || OS_OPENBSD

#endif  // !OS_WIN

    is_initialized_ = true;
    delegate_ = params.delegate;

    // The exit manager is in charge of calling the dtors of singleton objects.
    // On Android, AtExitManager is set up when library is loaded.
    // A consequence of this is that you can't use the ctor/dtor-based
    // TRACE_EVENT methods on Linux or iOS builds till after we set this up.
#if !defined(OS_ANDROID)
    if (!ui_task_) {
      // When running browser tests, don't create a second AtExitManager as that
      // interfers with shutdown when objects created before ContentMain is
      // called are destructed when it returns.
      exit_manager_.reset(new base::AtExitManager);
    }
#endif  // !OS_ANDROID

    int exit_code = 0;
    if (delegate_ && delegate_->BasicStartupComplete(&exit_code))
      return exit_code;

    completed_basic_startup_ = true;

    const base::CommandLine& command_line =
        *base::CommandLine::ForCurrentProcess();
    std::string process_type =
        command_line.GetSwitchValueASCII(switches::kProcessType);

#if defined(OS_WIN)
    if (command_line.HasSwitch(switches::kDeviceScaleFactor)) {
      std::string scale_factor_string = command_line.GetSwitchValueASCII(
          switches::kDeviceScaleFactor);
      double scale_factor = 0;
      if (base::StringToDouble(scale_factor_string, &scale_factor))
        display::win::SetDefaultDeviceScaleFactor(scale_factor);
    }
#endif

    if (!GetContentClient())
      SetContentClient(&empty_content_client_);
    ContentClientInitializer::Set(process_type, delegate_);

#if !defined(OS_ANDROID)
    // Enable startup tracing asap to avoid early TRACE_EVENT calls being
    // ignored. For Android, startup tracing is enabled in an even earlier place
    // content/app/android/library_loader_hooks.cc.
    // Zygote process does not have file thread and renderer process on Win10
    // cannot access the file system.
    // TODO(ssid): Check if other processes can enable startup tracing here.
    bool can_access_file_system = (process_type != switches::kZygoteProcess &&
                                   process_type != switches::kRendererProcess);
    tracing::EnableStartupTracingIfNeeded(can_access_file_system);
#endif  // !OS_ANDROID

#if defined(OS_WIN)
    // Enable exporting of events to ETW if requested on the command line.
    if (command_line.HasSwitch(switches::kTraceExportEventsToETW))
      base::trace_event::TraceEventETWExport::EnableETWExport();
#endif  // OS_WIN

#if !defined(OS_ANDROID)
    // Android tracing started at the beginning of the method.
    // Other OSes have to wait till we get here in order for all the memory
    // management setup to be completed.
    TRACE_EVENT0("startup,benchmark,rail", "ContentMainRunnerImpl::Initialize");
#endif  // !OS_ANDROID

#if defined(OS_MACOSX)
    // We need to allocate the IO Ports before the Sandbox is initialized or
    // the first instance of PowerMonitor is created.
    // It's important not to allocate the ports for processes which don't
    // register with the power monitor - see crbug.com/88867.
    if (process_type.empty() ||
        (delegate_ &&
         delegate_->ProcessRegistersWithSystemProcess(process_type))) {
      base::PowerMonitorDeviceSource::AllocateSystemIOPorts();
    }

    if (!process_type.empty() &&
        (!delegate_ || delegate_->ShouldSendMachPort(process_type))) {
      MachBroker::ChildSendTaskPortToParent();
    }
#endif

    // If we are on a platform where the default allocator is overridden (shim
    // layer on windows, tcmalloc on Linux Desktop) smoke-tests that the
    // overriding logic is working correctly. If not causes a hard crash, as its
    // unexpected absence has security implications.
    CHECK(base::allocator::IsAllocatorInitialized());

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

    RegisterPathProvider();
    RegisterContentSchemes(true);

#if defined(OS_ANDROID) && (ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_FILE)
    int icudata_fd = g_fds->MaybeGet(kAndroidICUDataDescriptor);
    if (icudata_fd != -1) {
      auto icudata_region = g_fds->GetRegion(kAndroidICUDataDescriptor);
      CHECK(base::i18n::InitializeICUWithFileDescriptor(icudata_fd,
                                                        icudata_region));
    } else {
      CHECK(base::i18n::InitializeICU());
    }
#else
    CHECK(base::i18n::InitializeICU());
#endif  // OS_ANDROID && (ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_FILE)

    base::StatisticsRecorder::Initialize();

    InitializeV8IfNeeded(command_line, process_type);

#if !defined(OFFICIAL_BUILD)
#if defined(OS_WIN)
    bool should_enable_stack_dump = !process_type.empty();
#else
    bool should_enable_stack_dump = true;
#endif
    // Print stack traces to stderr when crashes occur. This opens up security
    // holes so it should never be enabled for official builds. This needs to
    // happen before crash reporting is initialized (which for chrome happens in
    // the call to PreSandboxStartup() on the delegate below), because otherwise
    // this would interfere with signal handlers used by crash reporting.
    if (should_enable_stack_dump &&
        !command_line.HasSwitch(
            service_manager::switches::kDisableInProcessStackTraces)) {
      base::debug::EnableInProcessStackDumping();
    }
#endif  // !defined(OFFICIAL_BUILD)

    if (delegate_)
      delegate_->PreSandboxStartup();

#if defined(OS_WIN)
    CHECK(InitializeSandbox(
        service_manager::SandboxTypeFromCommandLine(command_line),
        params.sandbox_info));
#elif defined(OS_MACOSX)
    if (process_type == switches::kRendererProcess ||
        process_type == switches::kPpapiPluginProcess ||
        (delegate_ && delegate_->DelaySandboxInitialization(process_type))) {
      // On OS X the renderer sandbox needs to be initialized later in the
      // startup sequence in RendererMainPlatformDelegate::EnableSandbox().
    } else {
      CHECK(InitializeSandbox());
    }
#endif

    if (delegate_)
      delegate_->SandboxInitialized(process_type);

    // Return -1 to indicate no early termination.
    return -1;
  }

  int Run() override {
    DCHECK(is_initialized_);
    DCHECK(!is_shutdown_);
    const base::CommandLine& command_line =
        *base::CommandLine::ForCurrentProcess();
    std::string process_type =
        command_line.GetSwitchValueASCII(switches::kProcessType);

    // Run this logic on all child processes. Zygotes will run this at a later
    // point in time when the command line has been updated.
    std::unique_ptr<base::FieldTrialList> field_trial_list;
    if (!process_type.empty() && process_type != switches::kZygoteProcess)
      InitializeFieldTrialAndFeatureList(&field_trial_list);

    MainFunctionParams main_params(command_line);
    main_params.ui_task = ui_task_;
#if defined(OS_WIN)
    main_params.sandbox_info = &sandbox_info_;
#elif defined(OS_MACOSX)
    main_params.autorelease_pool = autorelease_pool_;
#endif
#if defined(USE_AURA)
    main_params.env_mode = env_mode_;
#endif
    main_params.create_discardable_memory = create_discardable_memory_;

    return RunNamedProcessTypeMain(process_type, main_params, delegate_);
  }

  void Shutdown() override {
    DCHECK(is_initialized_);
    DCHECK(!is_shutdown_);

    if (completed_basic_startup_ && delegate_) {
      const base::CommandLine& command_line =
          *base::CommandLine::ForCurrentProcess();
      std::string process_type =
          command_line.GetSwitchValueASCII(switches::kProcessType);

      delegate_->ProcessExiting(process_type);
    }

#if defined(OS_WIN)
#ifdef _CRTDBG_MAP_ALLOC
    _CrtDumpMemoryLeaks();
#endif  // _CRTDBG_MAP_ALLOC
#endif  // OS_WIN

    exit_manager_.reset(NULL);

    delegate_ = NULL;
    is_shutdown_ = true;
  }

 private:
  // True if the runner has been initialized.
  bool is_initialized_;

  // True if the runner has been shut down.
  bool is_shutdown_;

  // True if basic startup was completed.
  bool completed_basic_startup_;

  // Used if the embedder doesn't set one.
  ContentClient empty_content_client_;

  // The delegate will outlive this object.
  ContentMainDelegate* delegate_;

  std::unique_ptr<base::AtExitManager> exit_manager_;
#if defined(OS_WIN)
  sandbox::SandboxInterfaceInfo sandbox_info_;
#elif defined(OS_MACOSX)
  base::mac::ScopedNSAutoreleasePool* autorelease_pool_ = nullptr;
#endif

  base::Closure* ui_task_;

#if defined(USE_AURA)
  aura::Env::Mode env_mode_ = aura::Env::Mode::LOCAL;
#endif

  bool create_discardable_memory_ = true;

  DISALLOW_COPY_AND_ASSIGN(ContentMainRunnerImpl);
};

// static
ContentMainRunner* ContentMainRunner::Create() {
  return new ContentMainRunnerImpl();
}

}  // namespace content
