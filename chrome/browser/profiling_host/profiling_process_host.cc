// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiling_host/profiling_process_host.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_helpers.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/histogram_macros.h"
#include "base/sys_info.h"
#include "base/task_scheduler/post_task.h"
#include "base/trace_event/trace_log.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiling_host/chrome_client_connection_manager.h"
#include "chrome/browser/tracing/crash_service_uploader.h"
#include "components/services/heap_profiling/public/cpp/controller.h"
#include "components/services/heap_profiling/public/cpp/settings.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/tracing_controller.h"
#include "content/public/common/service_manager_connection.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"
#include "third_party/zlib/zlib.h"

#if defined(OS_WIN)
#include <io.h>
#endif

namespace {

base::trace_event::TraceConfig GetBackgroundTracingConfig(bool anonymize) {
  // Disable all categories other than memory-infra.
  base::trace_event::TraceConfig trace_config(
      "-*,disabled-by-default-memory-infra",
      base::trace_event::RECORD_UNTIL_FULL);

  // This flag is set by background tracing to filter out undesired events.
  if (anonymize)
    trace_config.EnableArgumentFilter();

  return trace_config;
}

}  // namespace

namespace heap_profiling {

bool ProfilingProcessHost::has_started_ = false;

namespace {

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
                              std::string trigger_name,
                              uint32_t sampling_rate) {
  // Traces has been observed as small as 4k. Seems likely to be a bug. To
  // account for all potentially too-small traces, we set the lower bounds to
  // 512 bytes. The upper bounds is set to 300MB as an extra-high threshold,
  // just in case something goes wrong.
  UMA_HISTOGRAM_CUSTOM_COUNTS("OutOfProcessHeapProfiling.UploadTrace.Size",
                              file_contents.size(), 512, 300 * 1024 * 1024, 50);

  base::Value rules_list(base::Value::Type::LIST);
  base::Value rule(base::Value::Type::DICTIONARY);
  rule.SetKey("rule", base::Value("MEMLOG"));
  rule.SetKey("trigger_name", base::Value(std::move(trigger_name)));
  rule.SetKey("category", base::Value("BENCHMARK_MEMORY_HEAVY"));
  rules_list.GetList().push_back(std::move(rule));

  std::string sampling_mode = base::StringPrintf("SAMPLING_%u", sampling_rate);

  base::Value configs(base::Value::Type::DICTIONARY);
  configs.SetKey("mode", base::Value(sampling_mode));
  configs.SetKey("category", base::Value("MEMLOG"));
  configs.SetKey("configs", std::move(rules_list));

  std::unique_ptr<base::DictionaryValue> metadata =
      std::make_unique<base::DictionaryValue>();
  metadata->SetKey("config", std::move(configs));

  TraceCrashServiceUploader* uploader = new TraceCrashServiceUploader(
      g_browser_process->system_request_context());

  uploader->DoUpload(file_contents, content::TraceUploader::COMPRESSED_UPLOAD,
                     std::move(metadata),
                     content::TraceUploader::UploadProgressCallback(),
                     base::Bind(&OnTraceUploadComplete, base::Owned(uploader)));
}

}  // namespace

ProfilingProcessHost::ProfilingProcessHost() : background_triggers_(this) {}

ProfilingProcessHost::~ProfilingProcessHost() = default;

Mode ProfilingProcessHost::GetMode() {
  if (!client_connection_manager_)
    return Mode::kNone;
  return client_connection_manager_->GetMode();
}

// static
void ProfilingProcessHost::Start(content::ServiceManagerConnection* connection,
                                 Mode mode,
                                 mojom::StackMode stack_mode,
                                 uint32_t sampling_rate,
                                 base::OnceClosure closure) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  CHECK(!has_started_);
  has_started_ = true;
  ProfilingProcessHost* host = GetInstance();

  host->connector_ = connection->GetConnector()->Clone();
  content::BrowserThread::GetTaskRunnerForThread(content::BrowserThread::IO)
      ->PostTask(
          FROM_HERE,
          base::BindOnce(&ProfilingProcessHost::StartServiceOnIOThread,
                         base::Unretained(host), stack_mode, sampling_rate,
                         connection->GetConnector()->Clone(), mode,
                         std::move(closure)));
}

// static
ProfilingProcessHost* ProfilingProcessHost::GetInstance() {
  return base::Singleton<
      ProfilingProcessHost,
      base::LeakySingletonTraits<ProfilingProcessHost>>::get();
}

void ProfilingProcessHost::ConfigureBackgroundProfilingTriggers() {
  // Only enable automatic uploads when the Finch experiment is enabled.
  // Developers can still manually upload via chrome://memory-internals.
  if (IsBackgroundHeapProfilingEnabled())
    background_triggers_.StartTimer();
}

void ProfilingProcessHost::SaveTraceWithHeapDumpToFile(
    base::FilePath dest,
    SaveTraceFinishedCallback done,
    bool stop_immediately_after_heap_dump_for_tests) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  auto finish_trace_callback = base::BindOnce(
      [](base::FilePath dest, SaveTraceFinishedCallback done, bool success,
         std::string trace) {
        if (!success) {
          content::BrowserThread::GetTaskRunnerForThread(
              content::BrowserThread::UI)
              ->PostTask(FROM_HERE, base::BindOnce(std::move(done), false));
          return;
        }
        base::PostTaskWithTraits(
            FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
            base::BindOnce(
                &ProfilingProcessHost::SaveTraceToFileOnBlockingThread,
                base::Unretained(ProfilingProcessHost::GetInstance()),
                std::move(dest), std::move(trace), std::move(done)));
      },
      std::move(dest), std::move(done));
  RequestTraceWithHeapDump(std::move(finish_trace_callback),
                           false /* anonymize */);
}

void ProfilingProcessHost::RequestProcessReport(std::string trigger_name) {
  // https://crbug.com/753218: Add e2e tests for this code path.
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (!connector_) {
    DLOG(ERROR)
        << "Requesting process dump when profiling process hasn't started.";
    return;
  }

  // If taking_trace_for_upload_ is already true, then the setter has no effect,
  // and we should early return.
  if (SetTakingTraceForUpload(true))
    return;

  // It's safe to pass a raw pointer for ProfilingProcessHost because it's a
  // singleton that's never destroyed.
  auto finish_report_callback = base::BindOnce(
      [](ProfilingProcessHost* host, std::string trigger_name,
         uint32_t sampling_rate, bool success, std::string trace) {
        UMA_HISTOGRAM_BOOLEAN("OutOfProcessHeapProfiling.RecordTrace.Success",
                              success);
        host->SetTakingTraceForUpload(false);
        if (success) {
          UploadTraceToCrashServer(std::move(trace), std::move(trigger_name),
                                   sampling_rate);
        }
      },
      base::Unretained(this), std::move(trigger_name),
      controller_->sampling_rate());
  RequestTraceWithHeapDump(std::move(finish_report_callback),
                           true /* anonymize */);
}

void ProfilingProcessHost::RequestTraceWithHeapDump(
    TraceFinishedCallback callback,
    bool anonymize) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  if (!connector_) {
    DLOG(ERROR)
        << "Requesting heap dump when profiling process hasn't started.";
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false, std::string()));
    return;
  }

  if (content::TracingController::GetInstance()->IsTracing()) {
    DLOG(ERROR) << "Requesting heap dump when tracing has already started.";
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false, std::string()));
    return;
  }

  auto finished_dump_callback = base::BindOnce(
      [](TraceFinishedCallback callback, bool success, uint64_t dump_guid) {
        // Once the trace has stopped, run |callback| on the UI thread.
        auto finish_sink_callback = base::Bind(
            [](TraceFinishedCallback callback,
               std::unique_ptr<const base::DictionaryValue> metadata,
               base::RefCountedString* in) {
              std::string result;
              result.swap(in->data());
              content::BrowserThread::GetTaskRunnerForThread(
                  content::BrowserThread::UI)
                  ->PostTask(FROM_HERE,
                             base::BindOnce(std::move(callback), true,
                                            std::move(result)));
            },
            base::Passed(std::move(callback)));
        scoped_refptr<content::TracingController::TraceDataEndpoint> sink =
            content::TracingController::CreateStringEndpoint(
                std::move(finish_sink_callback));
        content::TracingController::GetInstance()->StopTracing(sink);
      },
      std::move(callback));

  memory_instrumentation::MemoryInstrumentation::GetInstance()
      ->RequestGlobalDumpAndAppendToTrace(
          base::trace_event::MemoryDumpType::EXPLICITLY_TRIGGERED,
          base::trace_event::MemoryDumpLevelOfDetail::BACKGROUND,
          base::AdaptCallbackForRepeating(std::move(finished_dump_callback)));

  // The only reason this should return false is if tracing is already enabled,
  // which we've already checked.
  bool result = content::TracingController::GetInstance()->StartTracing(
      GetBackgroundTracingConfig(anonymize), base::Closure());
  DCHECK(result);
}

void ProfilingProcessHost::GetProfiledPids(GetProfiledPidsCallback callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  content::BrowserThread::GetTaskRunnerForThread(content::BrowserThread::IO)
      ->PostTask(
          FROM_HERE,
          base::BindOnce(&ProfilingProcessHost::GetProfiledPidsOnIOThread,
                         base::Unretained(this), std::move(callback)));
}

void ProfilingProcessHost::StartManualProfiling(base::ProcessId pid) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  if (has_started_) {
    if (client_connection_manager_) {
      client_connection_manager_->StartProfilingProcess(pid);
    } else {
      // Starting the ProfilingProcessHost requires hopping to the IO thread and
      // back. That means that there's a very short period of time where
      // has_started_ evaluates to true, but client_connection_manager_ has not
      // yet been created. In this case, we hop to the IO thread and back.
      base::OnceClosure callback = base::BindOnce(
          [](ProfilingProcessHost* host, base::ProcessId pid) {
            content::BrowserThread::GetTaskRunnerForThread(
                content::BrowserThread::UI)
                ->PostTask(
                    FROM_HERE,
                    base::BindOnce(&ProfilingProcessHost::StartManualProfiling,
                                   base::Unretained(host), pid));
          },
          base::Unretained(this), pid);
      content::BrowserThread::GetTaskRunnerForThread(content::BrowserThread::IO)
          ->PostTask(FROM_HERE, std::move(callback));
    }
  } else {
    base::Closure callback = base::Bind(
        [](base::ProcessId pid) {
          ProfilingProcessHost::GetInstance()
              ->client_connection_manager_->StartProfilingProcess(pid);
        },
        pid);
    ProfilingProcessHost::Start(
        content::ServiceManagerConnection::GetForProcess(), Mode::kManual,
        GetStackModeForStartup(), GetSamplingRateForStartup(),
        std::move(callback));
  }
}

void ProfilingProcessHost::GetProfiledPidsOnIOThread(
    GetProfiledPidsCallback callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  if (!controller_) {
    content::BrowserThread::GetTaskRunnerForThread(content::BrowserThread::UI)
        ->PostTask(FROM_HERE, base::BindOnce(std::move(callback),
                                             std::vector<base::ProcessId>()));
    return;
  }

  auto post_result_to_ui_thread = base::BindOnce(
      [](GetProfiledPidsCallback callback,
         const std::vector<base::ProcessId>& result) {
        content::BrowserThread::GetTaskRunnerForThread(
            content::BrowserThread::UI)
            ->PostTask(FROM_HERE, base::BindOnce(std::move(callback), result));
      },
      std::move(callback));
  controller_->GetProfiledPids(std::move(post_result_to_ui_thread));
}

void ProfilingProcessHost::SetKeepSmallAllocations(
    bool keep_small_allocations) {
  // May get called on different threads, we need to be on the IO thread to
  // work.
  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::IO)) {
    content::BrowserThread::GetTaskRunnerForThread(content::BrowserThread::IO)
        ->PostTask(
            FROM_HERE,
            base::BindOnce(&ProfilingProcessHost::SetKeepSmallAllocations,
                           base::Unretained(this), keep_small_allocations));
    return;
  }

  controller_->SetKeepSmallAllocations(keep_small_allocations);
}

void ProfilingProcessHost::StartServiceOnIOThread(
    mojom::StackMode stack_mode,
    uint32_t sampling_rate,
    std::unique_ptr<service_manager::Connector> connector,
    Mode mode,
    base::OnceClosure closure) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  controller_.reset(
      new Controller(std::move(connector), stack_mode, sampling_rate));
  base::WeakPtr<Controller> controller_weak_ptr = controller_->GetWeakPtr();

  content::BrowserThread::GetTaskRunnerForThread(content::BrowserThread::UI)
      ->PostTask(FROM_HERE,
                 base::BindOnce(
                     &ProfilingProcessHost::FinishPostServiceSetupOnUIThread,
                     base::Unretained(this), mode, std::move(closure),
                     controller_weak_ptr));
}

void ProfilingProcessHost::FinishPostServiceSetupOnUIThread(
    Mode mode,
    base::OnceClosure closure,
    base::WeakPtr<Controller> controller_weak_ptr) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  client_connection_manager_.reset(
      new ChromeClientConnectionManager(controller_weak_ptr, mode));
  client_connection_manager_->Start();
  ConfigureBackgroundProfilingTriggers();
  metrics_timer_.Start(
      FROM_HERE, base::TimeDelta::FromHours(24),
      base::Bind(&ProfilingProcessHost::ReportMetrics, base::Unretained(this)));
  if (closure)
    std::move(closure).Run();
}

void ProfilingProcessHost::SaveTraceToFileOnBlockingThread(
    base::FilePath dest,
    std::string trace,
    SaveTraceFinishedCallback done) {
  base::File file(dest,
                  base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);

  // Pass ownership of the underlying fd/HANDLE to zlib.
  base::PlatformFile platform_file = file.TakePlatformFile();
#if defined(OS_WIN)
  // The underlying handle |platform_file| is also closed when |fd| is closed.
  int fd = _open_osfhandle(reinterpret_cast<intptr_t>(platform_file), 0);
#else
  int fd = platform_file;
#endif
  gzFile gz_file = gzdopen(fd, "w");
  if (!gz_file) {
    DLOG(ERROR) << "Cannot compress trace file";
    content::BrowserThread::GetTaskRunnerForThread(content::BrowserThread::UI)
        ->PostTask(FROM_HERE, base::BindOnce(std::move(done), false));
    return;
  }

  size_t written_bytes = gzwrite(gz_file, trace.c_str(), trace.size());
  gzclose(gz_file);

  content::BrowserThread::GetTaskRunnerForThread(content::BrowserThread::UI)
      ->PostTask(FROM_HERE, base::BindOnce(std::move(done),
                                           written_bytes == trace.size()));
}

void ProfilingProcessHost::ReportMetrics() {
  UMA_HISTOGRAM_ENUMERATION("OutOfProcessHeapProfiling.ProfilingMode",
                            GetMode(), Mode::kCount);
}

bool ProfilingProcessHost::TakingTraceForUpload() {
  base::AutoLock lock(taking_trace_for_upload_lock_);
  return taking_trace_for_upload_;
}

bool ProfilingProcessHost::SetTakingTraceForUpload(bool new_state) {
  base::AutoLock lock(taking_trace_for_upload_lock_);
  bool ret = taking_trace_for_upload_;
  taking_trace_for_upload_ = new_state;
  return ret;
}

}  // namespace heap_profiling
