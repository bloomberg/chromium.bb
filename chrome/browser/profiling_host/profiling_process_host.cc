// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiling_host/profiling_process_host.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/allocator/features.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/process/process_iterator.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/sys_info.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tracing/crash_service_uploader.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/profiling/constants.mojom.h"
#include "chrome/common/profiling/memlog_sender_pipe.h"
#include "chrome/common/profiling/profiling_constants.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/tracing_controller.h"
#include "content/public/common/bind_interface_helpers.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace {

// A wrapper classes that allows a string to be exported as JSON in a trace
// event.
class StringWrapper : public base::trace_event::ConvertableToTraceFormat {
 public:
  explicit StringWrapper(std::string string) : json_(std::move(string)) {}

  void AppendAsTraceFormat(std::string* out) const override {
    out->append(json_);
  }

  std::string json_;
};

base::trace_event::TraceConfig GetBackgroundTracingConfig() {
  // Disable all categories other than memory-infra.
  base::trace_event::TraceConfig trace_config(
      "-*,disabled-by-default-memory-infra",
      base::trace_event::RECORD_UNTIL_FULL);

  // This flag is set by background tracing to filter out undesired events.
  trace_config.EnableArgumentFilter();

  // Trigger a background memory dump exactly once by setting a time-delta
  // between dumps of 2**29.
  base::trace_event::TraceConfig::MemoryDumpConfig memory_config;
  memory_config.allowed_dump_modes.insert(
      base::trace_event::MemoryDumpLevelOfDetail::BACKGROUND);
  base::trace_event::TraceConfig::MemoryDumpConfig::Trigger trigger;
  trigger.min_time_between_dumps_ms = 1 << 30;
  trigger.level_of_detail =
      base::trace_event::MemoryDumpLevelOfDetail::BACKGROUND;
  trigger.trigger_type = base::trace_event::MemoryDumpType::PERIODIC_INTERVAL;
  memory_config.triggers.clear();
  memory_config.triggers.push_back(trigger);
  trace_config.ResetMemoryDumpConfig(memory_config);
  return trace_config;
}

}  // namespace

namespace profiling {

const base::Feature kOOPHeapProfilingFeature{"OOPHeapProfiling",
                                             base::FEATURE_DISABLED_BY_DEFAULT};
const char kOOPHeapProfilingFeatureMode[] = "mode";

bool ProfilingProcessHost::has_started_ = false;

namespace {

constexpr char kNoTriggerName[] = "";

// This helper class cleans up initialization boilerplate for the callers who
// need to create ProfilingClients bound to various different things.
class ProfilingClientBinder {
 public:
  // Binds to a non-renderer-child-process' ProfilingClient.
  explicit ProfilingClientBinder(content::BrowserChildProcessHost* host)
      : ProfilingClientBinder() {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
    content::BindInterface(host->GetHost(), std::move(request_));
  }

  // Binds to a renderer's ProfilingClient.
  explicit ProfilingClientBinder(content::RenderProcessHost* host)
      : ProfilingClientBinder() {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    content::BindInterface(host, std::move(request_));
  }

  // Binds to the local connector to get the browser process' ProfilingClient.
  explicit ProfilingClientBinder(service_manager::Connector* connector)
      : ProfilingClientBinder() {
    connector->BindInterface(content::mojom::kBrowserServiceName,
                             std::move(request_));
  }

  mojom::ProfilingClientPtr take() { return std::move(memlog_client_); }

 private:
  ProfilingClientBinder()
      : memlog_client_(), request_(mojo::MakeRequest(&memlog_client_)) {}

  mojom::ProfilingClientPtr memlog_client_;
  mojom::ProfilingClientRequest request_;
};

void OnTraceUploadComplete(TraceCrashServiceUploader* uploader,
                           bool success,
                           const std::string& feedback) {
  if (!success) {
    LOG(ERROR) << "Cannot upload trace file: " << feedback;
    return;
  }

  // The reports is successfully sent. Reports the crash-id to ease debugging.
  LOG(WARNING) << "slow-reports sent: '" << feedback << '"';
}

void UploadTraceToCrashServer(std::string file_contents,
                              std::string trigger_name) {
  base::Value rules_list(base::Value::Type::LIST);
  base::Value rule(base::Value::Type::DICTIONARY);
  rule.SetKey("rule", base::Value("MEMLOG"));
  rule.SetKey("trigger_name", base::Value(std::move(trigger_name)));
  rule.SetKey("category", base::Value("BENCHMARK_MEMORY_HEAVY"));
  rules_list.GetList().push_back(std::move(rule));

  base::Value configs(base::Value::Type::DICTIONARY);
  configs.SetKey("mode", base::Value("REACTIVE_TRACING_MODE"));
  configs.SetKey("category", base::Value("MEMLOG"));
  configs.SetKey("configs", std::move(rules_list));

  std::unique_ptr<base::DictionaryValue> metadata =
      base::MakeUnique<base::DictionaryValue>();
  metadata->SetKey("config", std::move(configs));

  TraceCrashServiceUploader* uploader = new TraceCrashServiceUploader(
      g_browser_process->system_request_context());

  uploader->DoUpload(file_contents, content::TraceUploader::COMPRESSED_UPLOAD,
                     std::move(metadata),
                     content::TraceUploader::UploadProgressCallback(),
                     base::Bind(&OnTraceUploadComplete, base::Owned(uploader)));
}

}  // namespace

ProfilingProcessHost::ProfilingProcessHost()
    : is_registered_(false),
      background_triggers_(this),
      mode_(Mode::kNone),
      profiled_renderer_(nullptr),
      always_sample_for_tests_(false) {}

ProfilingProcessHost::~ProfilingProcessHost() {
  if (is_registered_)
    Unregister();
}

void ProfilingProcessHost::Register() {
  DCHECK(!is_registered_);
  Add(this);
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CREATED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
                 content::NotificationService::AllBrowserContextsAndSources());
  is_registered_ = true;
}

void ProfilingProcessHost::Unregister() {
  DCHECK(is_registered_);
  Remove(this);
}

void ProfilingProcessHost::BrowserChildProcessLaunchedAndConnected(
    const content::ChildProcessData& data) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // Ensure this is only called for all non-renderer browser child processes
  // so as not to collide with logic in ProfilingProcessHost::Observe().
  DCHECK_NE(data.process_type, content::ProcessType::PROCESS_TYPE_RENDERER);

  if (!ShouldProfileProcessType(data.process_type)) {
    return;
  }

  profiling::mojom::ProcessType process_type =
      (data.process_type == content::ProcessType::PROCESS_TYPE_GPU)
          ? profiling::mojom::ProcessType::GPU
          : profiling::mojom::ProcessType::OTHER;

  content::BrowserThread::GetTaskRunnerForThread(content::BrowserThread::IO)
      ->PostTask(
          FROM_HERE,
          base::BindOnce(&ProfilingProcessHost::StartProfilingNonRendererChild,
                         base::Unretained(this), data.id,
                         base::GetProcId(data.handle), process_type));
}

void ProfilingProcessHost::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  content::RenderProcessHost* host =
      content::Source<content::RenderProcessHost>(source).ptr();

  // NOTIFICATION_RENDERER_PROCESS_CLOSED corresponds to death of an underlying
  // RenderProcess. NOTIFICATION_RENDERER_PROCESS_TERMINATED corresponds to when
  // the RenderProcessHost's lifetime is ending. Ideally, we'd only listen to
  // the former, but if the RenderProcessHost is destroyed before the
  // RenderProcess, then the former is never sent.
  if (host == profiled_renderer_ &&
      (type == content::NOTIFICATION_RENDERER_PROCESS_TERMINATED ||
       type == content::NOTIFICATION_RENDERER_PROCESS_CLOSED)) {
    // |profiled_renderer_| is only ever set in kRendererSampling mode. This
    // code is a deadstore otherwise so it's safe but having a DCHECK makes the
    // intent clear.
    DCHECK_EQ(mode(), Mode::kRendererSampling);
    profiled_renderer_ = nullptr;
  }
  if (type == content::NOTIFICATION_RENDERER_PROCESS_CREATED &&
      ShouldProfileNewRenderer(host)) {
    // In kRendererSampling mode, store the RPH to ensure only one renderer is
    // ever sampled inside a chrome instance.
    if (mode() == Mode::kRendererSampling) {
      profiled_renderer_ = host;
    }
    // Tell the child process to start profiling.
    ProfilingClientBinder client(host);

    content::BrowserThread::GetTaskRunnerForThread(content::BrowserThread::IO)
        ->PostTask(
            FROM_HERE,
            base::BindOnce(&ProfilingProcessHost::AddClientToProfilingService,
                           base::Unretained(this), client.take(),
                           base::GetProcId(host->GetHandle()),
                           profiling::mojom::ProcessType::RENDERER));
  }
}

bool ProfilingProcessHost::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  profiling_service_->DumpProcessesForTracing(
      base::BindOnce(&ProfilingProcessHost::OnDumpProcessesForTracingCallback,
                     base::Unretained(this), args.dump_guid));
  return true;
}

void ProfilingProcessHost::OnDumpProcessesForTracingCallback(
    uint64_t guid,
    std::vector<profiling::mojom::SharedBufferWithSizePtr> buffers) {
  for (auto& buffer_ptr : buffers) {
    mojo::ScopedSharedBufferHandle& buffer = buffer_ptr->buffer;
    uint32_t size = buffer_ptr->size;

    if (!buffer->is_valid())
      return;

    mojo::ScopedSharedBufferMapping mapping = buffer->Map(size);
    if (!mapping) {
      DLOG(ERROR) << "Failed to map buffer";
      return;
    }

    const char* char_buffer = static_cast<const char*>(mapping.get());
    std::string json(char_buffer, char_buffer + size);

    const int kTraceEventNumArgs = 1;
    const char* const kTraceEventArgNames[] = {"dumps"};
    const unsigned char kTraceEventArgTypes[] = {TRACE_VALUE_TYPE_CONVERTABLE};
    std::unique_ptr<base::trace_event::ConvertableToTraceFormat> wrapper(
        new StringWrapper(std::move(json)));

    // Using the same id merges all of the heap dumps into a single detailed
    // dump node in the UI.
    TRACE_EVENT_API_ADD_TRACE_EVENT_WITH_PROCESS_ID(
        TRACE_EVENT_PHASE_MEMORY_DUMP,
        base::trace_event::TraceLog::GetCategoryGroupEnabled(
            base::trace_event::MemoryDumpManager::kTraceCategory),
        "periodic_interval", trace_event_internal::kGlobalScope, guid,
        buffer_ptr->pid, kTraceEventNumArgs, kTraceEventArgNames,
        kTraceEventArgTypes, nullptr /* arg_values */, &wrapper,
        TRACE_EVENT_FLAG_HAS_ID);
  }
  if (dump_process_for_tracing_callback_) {
    std::move(dump_process_for_tracing_callback_).Run();
    dump_process_for_tracing_callback_.Reset();
  }
}

void ProfilingProcessHost::AddClientToProfilingService(
    profiling::mojom::ProfilingClientPtr client,
    base::ProcessId pid,
    profiling::mojom::ProcessType process_type) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  MemlogSenderPipe::PipePair pipes;

  // Passes the client_for_profiling directly to the profiling process.
  // The client process can not start sending data until the pipe is ready,
  // so talking to the client is done in the AddSender completion callback.
  //
  // This code doesn't actually hang onto the client_for_browser interface
  // poiner beyond sending this message to start since there are no other
  // messages we need to send.
  profiling_service_->AddProfilingClient(
      pid, std::move(client),
      mojo::WrapPlatformFile(pipes.PassSender().release().handle),
      mojo::WrapPlatformFile(pipes.PassReceiver().release().handle),
      process_type);
}

// static
ProfilingProcessHost::Mode ProfilingProcessHost::GetCurrentMode() {
  const base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
#if BUILDFLAG(USE_ALLOCATOR_SHIM)
  if (cmdline->HasSwitch(switches::kMemlog) ||
      base::FeatureList::IsEnabled(kOOPHeapProfilingFeature)) {
    if (cmdline->HasSwitch(switches::kEnableHeapProfiling)) {
      // PartitionAlloc doesn't support chained allocation hooks so we can't
      // run both heap profilers at the same time.
      LOG(ERROR) << "--" << switches::kEnableHeapProfiling
                 << " specified with --" << switches::kMemlog
                 << "which are not compatible. Memlog will be disabled.";
      return Mode::kNone;
    }

    std::string mode;
    // Respect the commandline switch above the field trial.
    if (cmdline->HasSwitch(switches::kMemlog)) {
      mode = cmdline->GetSwitchValueASCII(switches::kMemlog);
    } else {
      mode = base::GetFieldTrialParamValueByFeature(
          kOOPHeapProfilingFeature, kOOPHeapProfilingFeatureMode);
    }

    if (mode == switches::kMemlogModeAll)
      return Mode::kAll;
    if (mode == switches::kMemlogModeMinimal)
      return Mode::kMinimal;
    if (mode == switches::kMemlogModeBrowser)
      return Mode::kBrowser;
    if (mode == switches::kMemlogModeGpu)
      return Mode::kGpu;
    if (mode == switches::kMemlogModeRendererSampling)
      return Mode::kRendererSampling;

    DLOG(ERROR) << "Unsupported value: \"" << mode << "\" passed to --"
                << switches::kMemlog;
  }
  return Mode::kNone;
#else
  LOG_IF(ERROR, cmdline->HasSwitch(switches::kMemlog))
      << "--" << switches::kMemlog
      << " specified but it will have no effect because the use_allocator_shim "
      << "is not available in this build.";
  return Mode::kNone;
#endif
}

// static
ProfilingProcessHost* ProfilingProcessHost::Start(
    content::ServiceManagerConnection* connection,
    Mode mode) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  CHECK(!has_started_);
  has_started_ = true;
  ProfilingProcessHost* host = GetInstance();
  host->SetMode(mode);
  host->Register();
  host->MakeConnector(connection);
  host->LaunchAsService();
  host->ConfigureBackgroundProfilingTriggers();
  host->metrics_timer_.Start(
      FROM_HERE, base::TimeDelta::FromHours(24),
      base::Bind(&ProfilingProcessHost::ReportMetrics, base::Unretained(host)));

  return host;
}

// static
ProfilingProcessHost* ProfilingProcessHost::GetInstance() {
  return base::Singleton<
      ProfilingProcessHost,
      base::LeakySingletonTraits<ProfilingProcessHost>>::get();
}

void ProfilingProcessHost::ConfigureBackgroundProfilingTriggers() {
  background_triggers_.StartTimer();
}

void ProfilingProcessHost::RequestProcessDump(base::ProcessId pid,
                                              base::FilePath dest,
                                              base::OnceClosure done) {
  if (!connector_) {
    DLOG(ERROR)
        << "Requesting process dump when profiling process hasn't started.";
    return;
  }

  base::PostTaskWithTraits(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(&ProfilingProcessHost::GetOutputFileOnBlockingThread,
                     base::Unretained(this), pid, std::move(dest),
                     kNoTriggerName, std::move(done)));
}

void ProfilingProcessHost::RequestProcessReport(base::ProcessId pid,
                                                std::string trigger_name) {
  // https://crbug.com/753218: Add e2e tests for this code path.
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (!connector_) {
    DLOG(ERROR)
        << "Requesting process dump when profiling process hasn't started.";
    return;
  }

  bool result = content::TracingController::GetInstance()->StartTracing(
      GetBackgroundTracingConfig(), base::Closure());
  if (!result)
    return;

  // Once the trace has stopped, upload the log to the crash server.
  auto finish_sink_callback = base::Bind(
      [](std::string trigger_name,
         std::unique_ptr<const base::DictionaryValue> metadata,
         base::RefCountedString* in) {
        std::string result;
        result.swap(in->data());
        content::BrowserThread::GetTaskRunnerForThread(
            content::BrowserThread::UI)
            ->PostTask(FROM_HERE, base::BindOnce(&UploadTraceToCrashServer,
                                                 std::move(result),
                                                 std::move(trigger_name)));
      },
      std::move(trigger_name));

  scoped_refptr<content::TracingController::TraceDataEndpoint> sink =
      content::TracingController::CreateStringEndpoint(
          std::move(finish_sink_callback));
  base::OnceClosure stop_tracing_closure = base::BindOnce(
      base::IgnoreResult<bool (content::TracingController::*)(  // NOLINT
          const scoped_refptr<content::TracingController::TraceDataEndpoint>&)>(
          &content::TracingController::StopTracing),
      base::Unretained(content::TracingController::GetInstance()), sink);

  // Wait 10 seconds, then end the trace.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, std::move(stop_tracing_closure),
      base::TimeDelta::FromSeconds(10));
}

void ProfilingProcessHost::SetDumpProcessForTracingCallback(
    base::OnceClosure callback) {
  DCHECK(!dump_process_for_tracing_callback_);
  dump_process_for_tracing_callback_ = std::move(callback);
}

void ProfilingProcessHost::MakeConnector(
    content::ServiceManagerConnection* connection) {
  connector_ = connection->GetConnector()->Clone();
}

void ProfilingProcessHost::LaunchAsService() {
  // May get called on different threads, we need to be on the IO thread to
  // work.
  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::IO)) {
    content::BrowserThread::GetTaskRunnerForThread(content::BrowserThread::IO)
        ->PostTask(FROM_HERE,
                   base::BindOnce(&ProfilingProcessHost::LaunchAsService,
                                  base::Unretained(this)));
    return;
  }

  // No need to unregister since ProfilingProcessHost is never destroyed.
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "OutOfProcessHeapProfilingDumpProvider",
      content::BrowserThread::GetTaskRunnerForThread(
          content::BrowserThread::IO));

  // Bind to the memlog service. This will start it if it hasn't started
  // already.
  connector_->BindInterface(mojom::kServiceName, &profiling_service_);

  // Start profiling the browser if the mode allows.
  if (ShouldProfileProcessType(content::ProcessType::PROCESS_TYPE_BROWSER)) {
    ProfilingClientBinder client(connector_.get());
    AddClientToProfilingService(client.take(), base::Process::Current().Pid(),
                                profiling::mojom::ProcessType::BROWSER);
  }
}

void ProfilingProcessHost::GetOutputFileOnBlockingThread(
    base::ProcessId pid,
    base::FilePath dest,
    std::string trigger_name,
    base::OnceClosure done) {
  base::ScopedClosureRunner done_runner(std::move(done));
  base::File file(dest,
                  base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::BindOnce(&ProfilingProcessHost::HandleDumpProcessOnIOThread,
                     base::Unretained(this), pid, std::move(dest),
                     std::move(file), std::move(trigger_name),
                     done_runner.Release()));
}

void ProfilingProcessHost::HandleDumpProcessOnIOThread(base::ProcessId pid,
                                                       base::FilePath file_path,
                                                       base::File file,
                                                       std::string trigger_name,
                                                       base::OnceClosure done) {
  mojo::ScopedHandle handle = mojo::WrapPlatformFile(file.TakePlatformFile());
  profiling_service_->DumpProcess(
      pid, std::move(handle), GetMetadataJSONForTrace(),
      base::BindOnce(&ProfilingProcessHost::OnProcessDumpComplete,
                     base::Unretained(this), std::move(file_path),
                     std::move(trigger_name), std::move(done)));
}

void ProfilingProcessHost::OnProcessDumpComplete(base::FilePath file_path,
                                                 std::string trigger_name,
                                                 base::OnceClosure done,
                                                 bool success) {
  base::ScopedClosureRunner done_runner(std::move(done));
  if (!success) {
    DLOG(ERROR) << "Cannot dump process.";
    // On any errors, the requested trace output file is deleted.
    base::PostTaskWithTraits(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BACKGROUND},
        base::BindOnce(base::IgnoreResult(&base::DeleteFile), file_path,
                       false));
    return;
  }
}

void ProfilingProcessHost::SetMode(Mode mode) {
  mode_ = mode;
}

std::unique_ptr<base::DictionaryValue>
ProfilingProcessHost::GetMetadataJSONForTrace() {
  std::unique_ptr<base::DictionaryValue> metadata_dict(
      new base::DictionaryValue);
  metadata_dict->SetKey(
      "product-version",
      base::Value(version_info::GetProductNameAndVersionForUserAgent()));
  metadata_dict->SetKey("user-agent", base::Value(GetUserAgent()));
  metadata_dict->SetKey("os-name",
                        base::Value(base::SysInfo::OperatingSystemName()));
  metadata_dict->SetKey(
      "command_line",
      base::Value(
          base::CommandLine::ForCurrentProcess()->GetCommandLineString()));
  metadata_dict->SetKey(
      "os-arch", base::Value(base::SysInfo::OperatingSystemArchitecture()));
  return metadata_dict;
}

void ProfilingProcessHost::ReportMetrics() {
  UMA_HISTOGRAM_ENUMERATION("OutOfProcessHeapProfiling.ProfilingMode", mode(),
                            Mode::kCount);
}

bool ProfilingProcessHost::ShouldProfileProcessType(int process_type) {
  switch (mode()) {
    case Mode::kAll:
      return true;

    case Mode::kMinimal:
      return (process_type == content::ProcessType::PROCESS_TYPE_GPU ||
              process_type == content::ProcessType::PROCESS_TYPE_BROWSER);

    case Mode::kGpu:
      return process_type == content::ProcessType::PROCESS_TYPE_GPU;

    case Mode::kBrowser:
      return process_type == content::ProcessType::PROCESS_TYPE_BROWSER;

    case Mode::kRendererSampling:
      // This seems odd because a renderer does get profiled. However, since
      // the general rule for the whole type is to not profile in this mode,
      // returning false is appropriate. kRendererSampling has special case
      // logic elsewhere to enable rendering specifically chosed renderers.
      return false;

    case Mode::kNone:
      return false;

    case Mode::kCount:
      // Fall through to hit NOTREACHED() below.
      {}
  }

  NOTREACHED();
  return false;
}

bool ProfilingProcessHost::ShouldProfileNewRenderer(
    content::RenderProcessHost* renderer) const {
  // Never profile incognito processes.
  if (Profile::FromBrowserContext(renderer->GetBrowserContext())
          ->GetProfileType() == Profile::INCOGNITO_PROFILE) {
    return false;
  }

  if (mode() == Mode::kAll) {
    return true;
  } else if (mode() == Mode::kRendererSampling && !profiled_renderer_) {
    if (always_sample_for_tests_) {
      return true;
    }

    // Sample renderers with a 1/3 probability.
    return (base::RandUint64() % 100000) < 33333;
  }

  return false;
}

void ProfilingProcessHost::StartProfilingNonRendererChild(
    int child_process_id,
    base::ProcessId proc_id,
    profiling::mojom::ProcessType process_type) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  content::BrowserChildProcessHost* host =
      content::BrowserChildProcessHost::FromID(child_process_id);
  if (!host)
    return;

  // Tell the child process to start profiling.
  ProfilingClientBinder client(host);
  AddClientToProfilingService(client.take(), proc_id, process_type);
}

void ProfilingProcessHost::SetRendererSamplingAlwaysProfileForTest() {
  always_sample_for_tests_ = true;
}

}  // namespace profiling
