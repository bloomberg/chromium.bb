// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiling_host/profiling_process_host.h"

#include "base/allocator/features.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/process/process_iterator.h"
#include "base/strings/string_number_conversions.h"
#include "base/sys_info.h"
#include "base/task_scheduler/post_task.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/tracing/crash_service_uploader.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/profiling/constants.mojom.h"
#include "chrome/common/profiling/profiling_constants.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
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

}  // namespace

namespace profiling {

bool ProfilingProcessHost::has_started_ = false;

namespace {

const size_t kMaxTraceSizeUploadInBytes = 10 * 1024 * 1024;
const char kNoTriggerName[] = "";

// This helper class cleans up initialization boilerplate for the callers who
// need to create ProfilingClients bound to various different things.
class ProfilingClientBinder {
 public:
  // Binds to a non-renderer-child-process' ProfilingClient.
  explicit ProfilingClientBinder(content::BrowserChildProcessHost* host)
      : ProfilingClientBinder() {
    content::BindInterface(host->GetHost(), std::move(request_));
  }

  // Binds to a renderer's ProfilingClient.
  explicit ProfilingClientBinder(content::RenderProcessHost* host)
      : ProfilingClientBinder() {
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
                           const std::string& feedback);

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

  uploader->DoUpload(file_contents, content::TraceUploader::UNCOMPRESSED_UPLOAD,
                     std::move(metadata),
                     content::TraceUploader::UploadProgressCallback(),
                     base::Bind(&OnTraceUploadComplete, base::Owned(uploader)));
}

void ReadTraceForUpload(base::FilePath file_path, std::string trigger_name) {
  std::string file_contents;
  bool success = base::ReadFileToStringWithMaxSize(file_path, &file_contents,
                                                   kMaxTraceSizeUploadInBytes);
  base::DeleteFile(file_path, false);

  if (!success) {
    DLOG(ERROR) << "Cannot read trace file contents.";
    return;
  }

  content::BrowserThread::GetTaskRunnerForThread(content::BrowserThread::UI)
      ->PostTask(FROM_HERE, base::BindOnce(&UploadTraceToCrashServer,
                                           std::move(file_contents),
                                           std::move(trigger_name)));
}

void OnTraceUploadComplete(TraceCrashServiceUploader* uploader,
                           bool success,
                           const std::string& feedback) {
  if (!success) {
    LOG(ERROR) << "Cannot upload trace file: " << feedback;
    return;
  }
}

}  // namespace

ProfilingProcessHost::ProfilingProcessHost()
    : is_registered_(false), background_triggers_(this) {}

ProfilingProcessHost::~ProfilingProcessHost() {
  if (is_registered_)
    Unregister();
}

void ProfilingProcessHost::Register() {
  DCHECK(!is_registered_);
  Add(this);
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CREATED,
                 content::NotificationService::AllBrowserContextsAndSources());
  is_registered_ = true;
}

void ProfilingProcessHost::Unregister() {
  DCHECK(is_registered_);
  Remove(this);
}

void ProfilingProcessHost::BrowserChildProcessLaunchedAndConnected(
    const content::ChildProcessData& data) {
  // In minimal mode, only profile the GPU process.
  if (mode_ == Mode::kMinimal &&
      data.process_type != content::ProcessType::PROCESS_TYPE_GPU) {
    return;
  }

  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::IO)) {
    content::BrowserThread::GetTaskRunnerForThread(content::BrowserThread::IO)
        ->PostTask(
            FROM_HERE,
            base::BindOnce(
                &ProfilingProcessHost::BrowserChildProcessLaunchedAndConnected,
                base::Unretained(this), data));
    return;
  }
  content::BrowserChildProcessHost* host =
      content::BrowserChildProcessHost::FromID(data.id);
  if (!host)
    return;

  // Tell the child process to start profiling.
  ProfilingClientBinder client(host);
  AddClientToProfilingService(client.take(), base::GetProcId(data.handle));
}

void ProfilingProcessHost::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  // Ignore newly launched renderer if only profiling a minimal set of
  // processes.
  if (mode_ == Mode::kMinimal)
    return;

  if (type != content::NOTIFICATION_RENDERER_PROCESS_CREATED)
    return;

  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::IO)) {
    content::BrowserThread::GetTaskRunnerForThread(content::BrowserThread::IO)
        ->PostTask(FROM_HERE, base::BindOnce(&ProfilingProcessHost::Observe,
                                             base::Unretained(this), type,
                                             source, details));
    return;
  }

  // Tell the child process to start profiling.
  content::RenderProcessHost* host =
      content::Source<content::RenderProcessHost>(source).ptr();
  ProfilingClientBinder client(host);
  AddClientToProfilingService(client.take(),
                              base::GetProcId(host->GetHandle()));
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
}

void ProfilingProcessHost::AddClientToProfilingService(
    profiling::mojom::ProfilingClientPtr client,
    base::ProcessId pid) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  // Writes to the data_channel must be atomic to ensure that the profiling
  // process can demux the messages. We accomplish this by making writes
  // synchronous, and protecting the write() itself with a Lock.
  mojo::edk::PlatformChannelPair data_channel(true /* client_is_blocking */);

  // Passes the client_for_profiling directly to the profiling process.
  // The client process can not start sending data until the pipe is ready,
  // so talking to the client is done in the AddSender completion callback.
  //
  // This code doesn't actually hang onto the client_for_browser interface
  // poiner beyond sending this message to start since there are no other
  // messages we need to send.
  profiling_service_->AddProfilingClient(
      pid, std::move(client),
      mojo::WrapPlatformFile(data_channel.PassClientHandle().release().handle),
      mojo::WrapPlatformFile(data_channel.PassServerHandle().release().handle));
}

// static
ProfilingProcessHost::Mode ProfilingProcessHost::GetCurrentMode() {
  const base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
#if BUILDFLAG(USE_ALLOCATOR_SHIM)
  if (cmdline->HasSwitch(switches::kMemlog)) {
    if (cmdline->HasSwitch(switches::kEnableHeapProfiling)) {
      // PartitionAlloc doesn't support chained allocation hooks so we can't
      // run both heap profilers at the same time.
      LOG(ERROR) << "--" << switches::kEnableHeapProfiling
                 << " specified with --" << switches::kMemlog
                 << "which are not compatible. Memlog will be disabled.";
      return Mode::kNone;
    }

    std::string mode = cmdline->GetSwitchValueASCII(switches::kMemlog);
    if (mode == switches::kMemlogModeAll)
      return Mode::kAll;
    if (mode == switches::kMemlogModeMinimal)
      return Mode::kMinimal;

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

  const bool kNoUpload = false;
  base::PostTaskWithTraits(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(&ProfilingProcessHost::GetOutputFileOnBlockingThread,
                     base::Unretained(this), pid, std::move(dest),
                     kNoTriggerName, kNoUpload, std::move(done)));
}

void ProfilingProcessHost::RequestProcessReport(base::ProcessId pid,
                                                std::string trigger_name) {
  if (!connector_) {
    DLOG(ERROR)
        << "Requesting process dump when profiling process hasn't started.";
    return;
  }

  const bool kUpload = true;
  base::PostTaskWithTraits(
      FROM_HERE, {base::TaskPriority::BACKGROUND, base::MayBlock()},
      base::BindOnce(&ProfilingProcessHost::GetOutputFileOnBlockingThread,
                     base::Unretained(this), pid, base::FilePath(),
                     std::move(trigger_name), kUpload, base::OnceClosure()));
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

  ProfilingClientBinder client(connector_.get());
  AddClientToProfilingService(client.take(), base::Process::Current().Pid());
}

void ProfilingProcessHost::GetOutputFileOnBlockingThread(
    base::ProcessId pid,
    base::FilePath dest,
    std::string trigger_name,
    bool upload,
    base::OnceClosure done) {
  base::ScopedClosureRunner done_runner(std::move(done));
  if (upload) {
    DCHECK(dest.empty());
    if (!CreateTemporaryFile(&dest)) {
      DLOG(ERROR) << "Cannot create temporary file for memory dump.";
      return;
    }
  }

  base::File file(dest,
                  base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::BindOnce(&ProfilingProcessHost::HandleDumpProcessOnIOThread,
                     base::Unretained(this), pid, std::move(dest),
                     std::move(file), std::move(trigger_name), upload,
                     done_runner.Release()));
}

void ProfilingProcessHost::HandleDumpProcessOnIOThread(base::ProcessId pid,
                                                       base::FilePath file_path,
                                                       base::File file,
                                                       std::string trigger_name,
                                                       bool upload,
                                                       base::OnceClosure done) {
  mojo::ScopedHandle handle = mojo::WrapPlatformFile(file.TakePlatformFile());
  profiling_service_->DumpProcess(
      pid, std::move(handle), GetMetadataJSONForTrace(),
      base::BindOnce(&ProfilingProcessHost::OnProcessDumpComplete,
                     base::Unretained(this), std::move(file_path),
                     std::move(trigger_name), upload, std::move(done)));
}

void ProfilingProcessHost::OnProcessDumpComplete(base::FilePath file_path,
                                                 std::string trigger_name,
                                                 bool upload,
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

  if (upload) {
    base::PostTaskWithTraits(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BACKGROUND},
        base::BindOnce(&ReadTraceForUpload, std::move(file_path),
                       std::move(trigger_name)));
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

}  // namespace profiling
