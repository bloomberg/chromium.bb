// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiling_host/profiling_process_host.h"

#include "base/allocator/features.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/sys_info.h"
#include "base/task_scheduler/post_task.h"
#include "base/trace_event/memory_dump_manager.h"
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

namespace {

const size_t kMaxTraceSizeUploadInBytes = 10 * 1024 * 1024;

void OnTraceUploadComplete(TraceCrashServiceUploader* uploader,
                           bool success,
                           const std::string& feedback);

void UploadTraceToCrashServer(base::FilePath file_path) {
  std::string file_contents;
  if (!base::ReadFileToStringWithMaxSize(file_path, &file_contents,
                                         kMaxTraceSizeUploadInBytes)) {
    DLOG(ERROR) << "Cannot read trace file contents.";
    return;
  }

  TraceCrashServiceUploader* uploader = new TraceCrashServiceUploader(
      g_browser_process->system_request_context());

  uploader->DoUpload(file_contents, content::TraceUploader::UNCOMPRESSED_UPLOAD,
                     nullptr, content::TraceUploader::UploadProgressCallback(),
                     base::Bind(&OnTraceUploadComplete, base::Owned(uploader)));
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

ProfilingProcessHost::ProfilingProcessHost() {
  Add(this);
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CREATED,
                 content::NotificationService::AllBrowserContextsAndSources());
}

ProfilingProcessHost::~ProfilingProcessHost() {
  Remove(this);
}

void ProfilingProcessHost::BrowserChildProcessLaunchedAndConnected(
    const content::ChildProcessData& data) {
  // Ignore newly launched child process if only profiling the browser.
  if (mode_ == Mode::kBrowser)
    return;

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
  profiling::mojom::MemlogClientPtr memlog_client;
  profiling::mojom::MemlogClientRequest request =
      mojo::MakeRequest(&memlog_client);
  BindInterface(host->GetHost(), std::move(request));
  base::ProcessId pid = base::GetProcId(data.handle);
  SendPipeToProfilingService(std::move(memlog_client), pid);
}

void ProfilingProcessHost::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  // Ignore newly launched renderer if only profiling the browser.
  if (mode_ == Mode::kBrowser)
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
  profiling::mojom::MemlogClientPtr memlog_client;
  profiling::mojom::MemlogClientRequest request =
      mojo::MakeRequest(&memlog_client);
  content::BindInterface(host, std::move(request));
  base::ProcessId pid = base::GetProcId(host->GetHandle());

  SendPipeToProfilingService(std::move(memlog_client), pid);
}

bool ProfilingProcessHost::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  DCHECK_NE(GetCurrentMode(), Mode::kNone);
  // TODO: Support dumping all processes for --memlog=all mode.
  // https://crbug.com/758437.
  memlog_->DumpProcessForTracing(
      base::Process::Current().Pid(),
      base::BindOnce(&ProfilingProcessHost::OnDumpProcessForTracingCallback,
                     base::Unretained(this)));
  return true;
}

void ProfilingProcessHost::OnDumpProcessForTracingCallback(
    mojo::ScopedSharedBufferHandle buffer,
    uint32_t size) {
  if (!buffer->is_valid()) {
    DLOG(ERROR) << "Failed to dump process for tracing";
    return;
  }

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

  TRACE_EVENT_API_ADD_TRACE_EVENT(
      TRACE_EVENT_PHASE_MEMORY_DUMP,
      base::trace_event::TraceLog::GetCategoryGroupEnabled(
          base::trace_event::MemoryDumpManager::kTraceCategory),
      "periodic_interval", trace_event_internal::kGlobalScope, 0x0,
      kTraceEventNumArgs, kTraceEventArgNames, kTraceEventArgTypes,
      nullptr /* arg_values */, &wrapper, TRACE_EVENT_FLAG_HAS_ID);
}

void ProfilingProcessHost::SendPipeToProfilingService(
    profiling::mojom::MemlogClientPtr memlog_client,
    base::ProcessId pid) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  mojo::edk::PlatformChannelPair data_channel;

  memlog_->AddSender(
      pid,
      mojo::WrapPlatformFile(data_channel.PassServerHandle().release().handle),
      base::BindOnce(&ProfilingProcessHost::SendPipeToClientProcess,
                     base::Unretained(this), std::move(memlog_client),
                     mojo::WrapPlatformFile(
                         data_channel.PassClientHandle().release().handle)));
}

void ProfilingProcessHost::SendPipeToClientProcess(
    profiling::mojom::MemlogClientPtr memlog_client,
    mojo::ScopedHandle handle) {
  memlog_client->StartProfiling(std::move(handle));
}

// static
ProfilingProcessHost::Mode ProfilingProcessHost::GetCurrentMode() {
#if BUILDFLAG(USE_ALLOCATOR_SHIM)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kMemlog)) {
    std::string mode =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kMemlog);
    if (mode == switches::kMemlogModeAll)
      return Mode::kAll;
    if (mode == switches::kMemlogModeBrowser)
      return Mode::kBrowser;

    DLOG(ERROR) << "Unsupported value: \"" << mode << "\" passed to --"
                << switches::kMemlog;
  }
  return Mode::kNone;
#else
  LOG_IF(ERROR,
         base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kMemlog))
      << "--" << switches::kMemlog
      << " specified but it will have no effect because the use_allocator_shim "
      << "is not available in this build.";
  return Mode::kNone;
#endif
}

// static
ProfilingProcessHost* ProfilingProcessHost::EnsureStarted(
    content::ServiceManagerConnection* connection,
    Mode mode) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  ProfilingProcessHost* host = GetInstance();
  host->SetMode(mode);
  host->MakeConnector(connection);
  host->LaunchAsService();
  return host;
}

// static
ProfilingProcessHost* ProfilingProcessHost::GetInstance() {
  return base::Singleton<
      ProfilingProcessHost,
      base::LeakySingletonTraits<ProfilingProcessHost>>::get();
}

void ProfilingProcessHost::RequestProcessDump(base::ProcessId pid,
                                              const base::FilePath& dest) {
  if (!connector_) {
    DLOG(ERROR)
        << "Requesting process dump when profiling process hasn't started.";
    return;
  }

  const bool kNoUpload = false;
  base::PostTaskWithTraits(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(&ProfilingProcessHost::GetOutputFileOnBlockingThread,
                     base::Unretained(this), pid, dest, kNoUpload));
}

void ProfilingProcessHost::RequestProcessReport(base::ProcessId pid) {
  if (!connector_) {
    DLOG(ERROR)
        << "Requesting process dump when profiling process hasn't started.";
    return;
  }

  base::FilePath output_path;
  if (!CreateTemporaryFile(&output_path)) {
    DLOG(ERROR) << "Cannot create temporary file for memory dump.";
    return;
  }

  const bool kUpload = true;
  base::PostTaskWithTraits(
      FROM_HERE, {base::TaskPriority::BACKGROUND, base::MayBlock()},
      base::BindOnce(&ProfilingProcessHost::GetOutputFileOnBlockingThread,
                     base::Unretained(this), pid, output_path, kUpload));
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
  connector_->BindInterface(mojom::kServiceName, &memlog_);

  // Tell the current process to start profiling.
  profiling::mojom::MemlogClientPtr memlog_client;
  profiling::mojom::MemlogClientRequest request =
      mojo::MakeRequest(&memlog_client);
  connector_->BindInterface(content::mojom::kBrowserServiceName,
                            mojo::MakeRequest(&memlog_client));
  base::ProcessId pid = base::Process::Current().Pid();
  SendPipeToProfilingService(std::move(memlog_client), pid);
}

void ProfilingProcessHost::GetOutputFileOnBlockingThread(
    base::ProcessId pid,
    const base::FilePath& dest,
    bool upload) {
  base::File file(dest,
                  base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::BindOnce(&ProfilingProcessHost::HandleDumpProcessOnIOThread,
                     base::Unretained(this), pid, std::move(dest),
                     std::move(file), upload));
}

void ProfilingProcessHost::HandleDumpProcessOnIOThread(base::ProcessId pid,
                                                       base::FilePath file_path,
                                                       base::File file,
                                                       bool upload) {
  mojo::ScopedHandle handle = mojo::WrapPlatformFile(file.TakePlatformFile());
  memlog_->DumpProcess(
      pid, std::move(handle), GetMetadataJSONForTrace(),
      base::BindOnce(&ProfilingProcessHost::OnProcessDumpComplete,
                     base::Unretained(this), std::move(file_path), upload));
}

void ProfilingProcessHost::OnProcessDumpComplete(base::FilePath file_path,
                                                 bool upload,
                                                 bool success) {
  if (!success) {
    DLOG(ERROR) << "Cannot dump process.";
    // On any errors, the requested trace output file is deleted.
    base::DeleteFile(file_path, false);
    return;
  }

  if (upload) {
    UploadTraceToCrashServer(file_path);

    // Uploaded file is a temporary file and must be deleted.
    base::DeleteFile(file_path, false);
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
