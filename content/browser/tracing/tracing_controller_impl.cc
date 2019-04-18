// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/browser/tracing/tracing_controller_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/cpu.h"
#include "base/files/file_tracing.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "base/trace_event/trace_config.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/tracing/common/trace_startup_config.h"
#include "components/tracing/common/trace_to_console.h"
#include "components/tracing/common/tracing_switches.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/tracing/file_tracing_provider_impl.h"
#include "content/browser/tracing/perfetto_file_tracer.h"
#include "content/browser/tracing/tracing_ui.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/tracing_controller.h"
#include "content/public/browser/tracing_delegate.h"
#include "content/public/common/content_client.h"
#include "content/public/common/service_manager_connection.h"
#include "gpu/config/gpu_info.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/network_change_notifier.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/tracing/public/cpp/trace_event_agent.h"
#include "services/tracing/public/cpp/traced_process_impl.h"
#include "services/tracing/public/cpp/tracing_features.h"
#include "services/tracing/public/mojom/constants.mojom.h"
#include "v8/include/v8-version-string.h"

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon_client.h"
#include "content/browser/tracing/cros_tracing_agent.h"
#endif

#if defined(CAST_TRACING_AGENT)
#include "content/browser/tracing/cast_tracing_agent.h"
#endif

#if defined(OS_WIN)
#include "base/win/registry.h"
#include "base/win/windows_version.h"
#endif

#if defined(OS_ANDROID)
#include "base/debug/elf_reader.h"
#include "content/browser/android/tracing_controller_android.h"

// Symbol with virtual address of the start of ELF header of the current binary.
extern char __ehdr_start;
#endif  // defined(OS_ANDROID)

namespace content {

namespace {

TracingControllerImpl* g_tracing_controller = nullptr;

std::string GetNetworkTypeString() {
  switch (net::NetworkChangeNotifier::GetConnectionType()) {
    case net::NetworkChangeNotifier::CONNECTION_ETHERNET:
      return "Ethernet";
    case net::NetworkChangeNotifier::CONNECTION_WIFI:
      return "WiFi";
    case net::NetworkChangeNotifier::CONNECTION_2G:
      return "2G";
    case net::NetworkChangeNotifier::CONNECTION_3G:
      return "3G";
    case net::NetworkChangeNotifier::CONNECTION_4G:
      return "4G";
    case net::NetworkChangeNotifier::CONNECTION_NONE:
      return "None";
    case net::NetworkChangeNotifier::CONNECTION_BLUETOOTH:
      return "Bluetooth";
    case net::NetworkChangeNotifier::CONNECTION_UNKNOWN:
    default:
      break;
  }
  return "Unknown";
}

std::string GetClockString() {
  switch (base::TimeTicks::GetClock()) {
    case base::TimeTicks::Clock::FUCHSIA_ZX_CLOCK_MONOTONIC:
      return "FUCHSIA_ZX_CLOCK_MONOTONIC";
    case base::TimeTicks::Clock::LINUX_CLOCK_MONOTONIC:
      return "LINUX_CLOCK_MONOTONIC";
    case base::TimeTicks::Clock::IOS_CF_ABSOLUTE_TIME_MINUS_KERN_BOOTTIME:
      return "IOS_CF_ABSOLUTE_TIME_MINUS_KERN_BOOTTIME";
    case base::TimeTicks::Clock::MAC_MACH_ABSOLUTE_TIME:
      return "MAC_MACH_ABSOLUTE_TIME";
    case base::TimeTicks::Clock::WIN_QPC:
      return "WIN_QPC";
    case base::TimeTicks::Clock::WIN_ROLLOVER_PROTECTED_TIME_GET_TIME:
      return "WIN_ROLLOVER_PROTECTED_TIME_GET_TIME";
  }

  NOTREACHED();
  return std::string();
}

#if defined(OS_WIN)
// The following code detect whether the current session is a remote session.
// See:
// https://docs.microsoft.com/en-us/windows/desktop/TermServ/detecting-the-terminal-services-environment
bool IsCurrentSessionRemote() {
  static const wchar_t kRdpSettingsKeyName[] =
      L"SYSTEM\\CurrentControlSet\\Control\\Terminal Server";
  static const wchar_t kGlassSessionIdValueName[] = L"GlassSessionId";

  if (::GetSystemMetrics(SM_REMOTESESSION))
    return true;

  DWORD glass_session_id = 0;
  DWORD current_session_id = 0;
  base::win::RegKey key(HKEY_LOCAL_MACHINE, kRdpSettingsKeyName, KEY_READ);
  if (!::ProcessIdToSessionId(::GetCurrentProcessId(), &current_session_id) ||
      !key.Valid() ||
      key.ReadValueDW(kGlassSessionIdValueName, &glass_session_id) !=
          ERROR_SUCCESS) {
    return false;
  }

  return current_session_id != glass_session_id;
}
#endif

void OnStoppedStartupTracing(const base::FilePath& trace_file) {
  VLOG(0) << "Completed startup tracing to " << trace_file.value();
  tracing::TraceStartupConfig::GetInstance()->OnTraceToResultFileFinished();
}

}  // namespace

TracingController* TracingController::GetInstance() {
  return TracingControllerImpl::GetInstance();
}

TracingControllerImpl::TracingControllerImpl()
    : delegate_(GetContentClient()->browser()->GetTracingDelegate()) {
  DCHECK(!g_tracing_controller);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Deliberately leaked, like this class.
  base::FileTracing::SetProvider(new FileTracingProviderImpl);
  AddAgents();
  g_tracing_controller = this;

  // TODO(oysteine): Startup tracing using Perfetto
  // is enabled by the Mojo consumer in content/browser
  // for now; this is too late in the browser startup
  // process however.
  if (PerfettoFileTracer::ShouldEnable())
    perfetto_file_tracer_ = std::make_unique<PerfettoFileTracer>();
}

TracingControllerImpl::~TracingControllerImpl() = default;

void TracingControllerImpl::AddAgents() {
  tracing::TracedProcessImpl::GetInstance()->SetTaskRunner(
      base::SequencedTaskRunnerHandle::Get());

#if defined(OS_CHROMEOS)
  agents_.push_back(std::make_unique<CrOSTracingAgent>());
#elif defined(CAST_TRACING_AGENT)
  agents_.push_back(std::make_unique<CastTracingAgent>());
#endif

  // For adding general CPU, network, OS, and other system information to the
  // metadata.
  auto* trace_event_agent = tracing::TraceEventAgent::GetInstance();
  trace_event_agent->AddMetadataGeneratorFunction(base::BindRepeating(
      &TracingControllerImpl::GenerateMetadataDict, base::Unretained(this)));
  if (delegate_) {
    trace_event_agent->AddMetadataGeneratorFunction(
        base::BindRepeating(&TracingDelegate::GenerateMetadataDict,
                            base::Unretained(delegate_.get())));
  }
}

void TracingControllerImpl::ConnectToServiceIfNeeded() {
  if (!coordinator_) {
    ServiceManagerConnection::GetForProcess()->GetConnector()->BindInterface(
        tracing::mojom::kServiceName, &coordinator_);
    coordinator_.set_connection_error_handler(base::BindOnce(
        [](TracingControllerImpl* controller) {
          controller->coordinator_.reset();
        },
        base::Unretained(this)));
  }
}

void TracingControllerImpl::DisconnectFromService() {
  coordinator_ = nullptr;
}

// Can be called on any thread.
std::unique_ptr<base::DictionaryValue>
TracingControllerImpl::GenerateMetadataDict() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto metadata_dict = std::make_unique<base::DictionaryValue>();

  // trace_config_ can be null if the tracing controller finishes flushing
  // traces before the Chrome tracing agent finishes flushing traces. Normally,
  // this does not happen; however, if the service manager is teared down during
  // tracing, e.g. at Chrome shutdown, tracing controller may finish flushing
  // traces without waiting for tracing agents.
  if (trace_config_ && !tracing::TracingUsesPerfettoBackend()) {
    DCHECK(IsTracing());
    metadata_dict->SetString("trace-config", trace_config_->ToString());
  }

  metadata_dict->SetString("network-type", GetNetworkTypeString());
  metadata_dict->SetString("product-version",
                           GetContentClient()->browser()->GetProduct());
  metadata_dict->SetString("v8-version", V8_VERSION_STRING);
  metadata_dict->SetString("user-agent",
                           GetContentClient()->browser()->GetUserAgent());

#if defined(OS_ANDROID)
  // The library name is used for symbolizing heap profiles. This cannot be
  // obtained from process maps since library can be mapped from apk directly.
  // This is not added as part of memory-infra os dumps since it is special case
  // only for chrome library.
  base::Optional<base::StringPiece> soname =
      base::debug::ReadElfLibraryName(&__ehdr_start);
  if (soname)
    metadata_dict->SetString("chrome-library-name", *soname);
#endif  // defined(OS_ANDROID)
  metadata_dict->SetInteger("chrome-bitness", 8 * sizeof(uintptr_t));

  // OS
#if defined(OS_CHROMEOS)
  metadata_dict->SetString("os-name", "CrOS");
#else
  metadata_dict->SetString("os-name", base::SysInfo::OperatingSystemName());
#endif
  metadata_dict->SetString("os-version",
                           base::SysInfo::OperatingSystemVersion());
#if defined(OS_WIN)
  if (base::win::OSInfo::GetArchitecture() ==
      base::win::OSInfo::X64_ARCHITECTURE) {
    if (base::win::OSInfo::GetInstance()->wow64_status() ==
        base::win::OSInfo::WOW64_ENABLED) {
      metadata_dict->SetString("os-wow64", "enabled");
    } else {
      metadata_dict->SetString("os-wow64", "disabled");
    }
  }

  metadata_dict->SetString("os-session",
                           IsCurrentSessionRemote() ? "remote" : "local");
#endif

  metadata_dict->SetString("os-arch",
                           base::SysInfo::OperatingSystemArchitecture());

  // CPU
  base::CPU cpu;
  metadata_dict->SetInteger("cpu-family", cpu.family());
  metadata_dict->SetInteger("cpu-model", cpu.model());
  metadata_dict->SetInteger("cpu-stepping", cpu.stepping());
  metadata_dict->SetInteger("num-cpus", base::SysInfo::NumberOfProcessors());
  metadata_dict->SetInteger("physical-memory",
                            base::SysInfo::AmountOfPhysicalMemoryMB());

  metadata_dict->SetString("cpu-brand", cpu.cpu_brand());

  // GPU
  const gpu::GPUInfo gpu_info =
      content::GpuDataManagerImpl::GetInstance()->GetGPUInfo();
  const gpu::GPUInfo::GPUDevice& active_gpu = gpu_info.active_gpu();

#if !defined(OS_ANDROID)
  metadata_dict->SetInteger("gpu-venid", active_gpu.vendor_id);
  metadata_dict->SetInteger("gpu-devid", active_gpu.device_id);
#endif

  metadata_dict->SetString("gpu-driver", active_gpu.driver_version);
  metadata_dict->SetString("gpu-psver", gpu_info.pixel_shader_version);
  metadata_dict->SetString("gpu-vsver", gpu_info.vertex_shader_version);

#if defined(OS_MACOSX)
  metadata_dict->SetString("gpu-glver", gpu_info.gl_version);
#elif defined(OS_POSIX)
  metadata_dict->SetString("gpu-gl-vendor", gpu_info.gl_vendor);
  metadata_dict->SetString("gpu-gl-renderer", gpu_info.gl_renderer);
#endif

  metadata_dict->SetString("clock-domain", GetClockString());
  metadata_dict->SetBoolean("highres-ticks",
                            base::TimeTicks::IsHighResolution());

  metadata_dict->SetString(
      "command_line",
      base::CommandLine::ForCurrentProcess()->GetCommandLineString());

  base::Time::Exploded ctime;
  TRACE_TIME_NOW().UTCExplode(&ctime);
  std::string time_string = base::StringPrintf(
      "%u-%u-%u %d:%d:%d", ctime.year, ctime.month, ctime.day_of_month,
      ctime.hour, ctime.minute, ctime.second);
  metadata_dict->SetString("trace-capture-datetime", time_string);

  // TODO(crbug.com/737049): The central controller doesn't know about
  // metadata filters, so we temporarily filter here as the controller is
  // what assembles the full trace data.
  base::trace_event::MetadataFilterPredicate metadata_filter;
  if (trace_config_ && trace_config_->IsArgumentFilterEnabled()) {
    metadata_filter = base::trace_event::TraceLog::GetInstance()
                          ->GetMetadataFilterPredicate();
  }

  if (!metadata_filter.is_null()) {
    for (base::DictionaryValue::Iterator it(*metadata_dict); !it.IsAtEnd();
         it.Advance()) {
      if (!metadata_filter.Run(it.key())) {
        metadata_dict->SetString(it.key(), "__stripped__");
      }
    }
  }

  return metadata_dict;
}

TracingControllerImpl* TracingControllerImpl::GetInstance() {
  DCHECK(g_tracing_controller);
  return g_tracing_controller;
}

bool TracingControllerImpl::GetCategories(GetCategoriesDoneCallback callback) {
  std::set<std::string> category_set;
  tracing::TracedProcessImpl::GetInstance()->GetCategories(&category_set);

  std::move(callback).Run(category_set);
  return true;
}

bool TracingControllerImpl::StartTracing(
    const base::trace_event::TraceConfig& trace_config,
    StartTracingDoneCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // TODO(chiniforooshan): The actual value should be received by callback and
  // this function should return void.
  if (IsTracing()) {
    // Do not allow updating trace config when process filter is not used.
    if (trace_config.process_filter_config().empty() ||
        trace_config_->process_filter_config().empty()) {
      return false;
    }
    // Make sure other parts of trace_config (besides process filter)
    // did not change.
    base::trace_event::TraceConfig old_config_copy(*trace_config_);
    base::trace_event::TraceConfig new_config_copy(trace_config);
    old_config_copy.SetProcessFilterConfig(
        base::trace_event::TraceConfig::ProcessFilterConfig());
    new_config_copy.SetProcessFilterConfig(
        base::trace_event::TraceConfig::ProcessFilterConfig());
    if (old_config_copy.ToString() != new_config_copy.ToString())
      return false;
  }
  trace_config_ =
      std::make_unique<base::trace_event::TraceConfig>(trace_config);

  ConnectToServiceIfNeeded();
  coordinator_->StartTracing(
      trace_config.ToString(),
      base::BindOnce(
          [](StartTracingDoneCallback callback, bool success) {
            if (!callback.is_null())
              std::move(callback).Run();
          },
          std::move(callback)));
  // TODO(chiniforooshan): The actual success value should be sent by the
  // callback asynchronously.
  return true;
}

void TracingControllerImpl::StartStartupTracingIfNeeded() {
  auto* trace_startup_config = tracing::TraceStartupConfig::GetInstance();
  if (trace_startup_config->AttemptAdoptBySessionOwner(
          tracing::TraceStartupConfig::SessionOwner::kTracingController)) {
    StartTracing(trace_startup_config->GetTraceConfig(),
                 StartTracingDoneCallback());
  } else if (base::CommandLine::ForCurrentProcess()->HasSwitch(
                 switches::kTraceToConsole)) {
    StartTracing(tracing::GetConfigForTraceToConsole(),
                 StartTracingDoneCallback());
  }

  if (trace_startup_config->IsTracingStartupForDuration()) {
    TRACE_EVENT0("startup",
                 "TracingControllerImpl::InitStartupTracingForDuration");
    InitStartupTracingForDuration();
  }
}

base::FilePath TracingControllerImpl::GetStartupTraceFileName() const {
  base::FilePath trace_file;

  trace_file = tracing::TraceStartupConfig::GetInstance()->GetResultFile();
  if (trace_file.empty()) {
#if defined(OS_ANDROID)
    TracingControllerAndroid::GenerateTracingFilePath(&trace_file);
#else
    // Default to saving the startup trace into the current dir.
    trace_file = base::FilePath().AppendASCII("chrometrace.log");
#endif
  }

  return trace_file;
}

void TracingControllerImpl::InitStartupTracingForDuration() {
  DCHECK(tracing::TraceStartupConfig::GetInstance()
             ->IsTracingStartupForDuration());

  startup_trace_file_ = GetStartupTraceFileName();

  startup_trace_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromSeconds(
          tracing::TraceStartupConfig::GetInstance()->GetStartupDuration()),
      this, &TracingControllerImpl::EndStartupTracing);
}

void TracingControllerImpl::EndStartupTracing() {
  // Do nothing if startup tracing is already stopped.
  if (!tracing::TraceStartupConfig::GetInstance()->IsEnabled())
    return;

  StopTracing(CreateFileEndpoint(
      startup_trace_file_,
      base::BindRepeating(OnStoppedStartupTracing, startup_trace_file_)));
}

void TracingControllerImpl::FinalizeStartupTracingIfNeeded() {
  // There are two cases:
  // 1. Startup duration is not reached.
  // 2. Or if the trace should be saved to file for --trace-config-file flag.
  base::Optional<base::FilePath> startup_trace_file;
  if (startup_trace_timer_.IsRunning()) {
    startup_trace_timer_.Stop();
    if (startup_trace_file_ != base::FilePath().AppendASCII("none")) {
      startup_trace_file = startup_trace_file_;
    }
  } else if (tracing::TraceStartupConfig::GetInstance()
                 ->ShouldTraceToResultFile()) {
    startup_trace_file = GetStartupTraceFileName();
  }
  if (!startup_trace_file)
    return;
  // Perfetto currently doesn't support tracing during shutdown as the trace
  // buffer is lost when the service is shut down, so we wait until the trace is
  // complete. See also crbug.com/944107.
  // TODO(eseckler): Avoid the nestedRunLoop here somehow.
  base::RunLoop run_loop;
  StopTracing(CreateFileEndpoint(
      startup_trace_file.value(),
      base::BindRepeating(
          [](base::FilePath trace_file, base::OnceClosure quit_closure) {
            OnStoppedStartupTracing(trace_file);
            std::move(quit_closure).Run();
          },
          startup_trace_file.value(), run_loop.QuitClosure())));
  run_loop.Run();
}

bool TracingControllerImpl::StopTracing(
    const scoped_refptr<TraceDataEndpoint>& trace_data_endpoint) {
  return StopTracing(std::move(trace_data_endpoint), "");
}

bool TracingControllerImpl::StopTracing(
    const scoped_refptr<TraceDataEndpoint>& trace_data_endpoint,
    const std::string& agent_label) {
  if (!IsTracing() || drainer_ || !coordinator_)
    return false;
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

#if defined(OS_ANDROID)
  base::trace_event::TraceLog::GetInstance()->AddClockSyncMetadataEvent();
#endif

  tracing::TraceStartupConfig::GetInstance()->SetDisabled();
  trace_data_endpoint_ = std::move(trace_data_endpoint);
  is_data_complete_ = false;
  is_metadata_available_ = false;
  mojo::DataPipe data_pipe;
  drainer_.reset(
      new mojo::DataPipeDrainer(this, std::move(data_pipe.consumer_handle)));
  if (agent_label.empty()) {
    // Stop and flush all agents.
    coordinator_->StopAndFlush(
        std::move(data_pipe.producer_handle),
        base::BindRepeating(&TracingControllerImpl::OnMetadataAvailable,
                            base::Unretained(this)));
  } else {
    // Stop all and flush a particular agent.
    coordinator_->StopAndFlushAgent(
        std::move(data_pipe.producer_handle), agent_label,
        base::BindRepeating(&TracingControllerImpl::OnMetadataAvailable,
                            base::Unretained(this)));
  }
  // TODO(chiniforooshan): Is the return value used anywhere?
  return true;
}

bool TracingControllerImpl::GetTraceBufferUsage(
    GetTraceBufferUsageCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ConnectToServiceIfNeeded();
  coordinator_->RequestBufferUsage(base::BindOnce(
      [](GetTraceBufferUsageCallback callback, bool success, float percent_full,
         uint32_t approximate_count) {
        std::move(callback).Run(percent_full, approximate_count);
      },
      std::move(callback)));
  // TODO(chiniforooshan): The actual success value should be sent by the
  // callback asynchronously.
  return true;
}

bool TracingControllerImpl::IsTracing() {
  return trace_config_ != nullptr;
}

void TracingControllerImpl::RegisterTracingUI(TracingUI* tracing_ui) {
  DCHECK(tracing_uis_.find(tracing_ui) == tracing_uis_.end());
  tracing_uis_.insert(tracing_ui);
}

void TracingControllerImpl::UnregisterTracingUI(TracingUI* tracing_ui) {
  auto it = tracing_uis_.find(tracing_ui);
  DCHECK(it != tracing_uis_.end());
  tracing_uis_.erase(it);
}

void TracingControllerImpl::OnDataAvailable(const void* data,
                                            size_t num_bytes) {
  if (trace_data_endpoint_) {
    const std::string chunk(static_cast<const char*>(data), num_bytes);
    trace_data_endpoint_->ReceiveTraceChunk(
        std::make_unique<std::string>(chunk));
  }
}

void TracingControllerImpl::CompleteFlush() {
  if (trace_data_endpoint_) {
    trace_data_endpoint_->ReceiveTraceFinalContents(
        std::move(filtered_metadata_));
  }
  filtered_metadata_.reset(nullptr);
  trace_data_endpoint_ = nullptr;
  trace_config_ = nullptr;
  drainer_ = nullptr;
}

void TracingControllerImpl::OnDataComplete() {
  is_data_complete_ = true;
  if (is_metadata_available_)
    CompleteFlush();
}

void TracingControllerImpl::OnMetadataAvailable(base::Value metadata) {
  DCHECK(!filtered_metadata_);
  is_metadata_available_ = true;
  base::trace_event::MetadataFilterPredicate metadata_filter;
  if (trace_config_->IsArgumentFilterEnabled()) {
    metadata_filter = base::trace_event::TraceLog::GetInstance()
                          ->GetMetadataFilterPredicate();
  }
  if (metadata_filter.is_null()) {
    filtered_metadata_ = base::DictionaryValue::From(
        base::Value::ToUniquePtrValue(std::move(metadata)));
  } else {
    filtered_metadata_ = std::make_unique<base::DictionaryValue>();
    for (auto it : metadata.DictItems()) {
      if (metadata_filter.Run(it.first)) {
        filtered_metadata_->SetKey(it.first, std::move(it.second));
      } else {
        filtered_metadata_->SetKey(it.first, base::Value("__stripped__"));
      }
    }
  }
  if (is_data_complete_)
    CompleteFlush();
}

void TracingControllerImpl::SetTracingDelegateForTesting(
    std::unique_ptr<TracingDelegate> delegate) {
  if (!delegate) {
    delegate_.reset(GetContentClient()->browser()->GetTracingDelegate());
  } else {
    delegate_ = std::move(delegate);
  }
}

}  // namespace content
