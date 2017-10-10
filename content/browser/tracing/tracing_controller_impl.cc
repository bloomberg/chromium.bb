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
#include "base/memory/ptr_util.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "base/time/time.h"
#include "base/trace_event/trace_config.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/browser/tracing/file_tracing_provider_impl.h"
#include "content/browser/tracing/power_tracing_agent.h"
#include "content/browser/tracing/tracing_ui.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/tracing_controller.h"
#include "content/public/browser/tracing_delegate.h"
#include "content/public/common/content_client.h"
#include "content/public/common/service_manager_connection.h"
#include "gpu/config/gpu_info.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/network_change_notifier.h"
#include "services/resource_coordinator/public/cpp/tracing/chrome_trace_event_agent.h"
#include "services/resource_coordinator/public/interfaces/service_constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "v8/include/v8-version-string.h"

#if (defined(OS_POSIX) && defined(USE_UDEV)) || defined(OS_WIN) || \
    defined(OS_MACOSX)
#define ENABLE_POWER_TRACING
#endif

#if defined(ENABLE_POWER_TRACING)
#include "content/browser/tracing/power_tracing_agent.h"
#endif

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon_client.h"
#include "content/browser/tracing/arc_tracing_agent_impl.h"
#include "content/browser/tracing/cros_tracing_agent.h"
#endif

#if defined(OS_WIN)
#include "content/browser/tracing/etw_tracing_agent_win.h"
#endif

namespace content {

namespace {

TracingControllerImpl* g_controller = nullptr;

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

std::unique_ptr<base::DictionaryValue> GenerateSystemMetadataDict() {
  auto metadata_dict = base::MakeUnique<base::DictionaryValue>();

  metadata_dict->SetString("network-type", GetNetworkTypeString());
  metadata_dict->SetString("product-version", GetContentClient()->GetProduct());
  metadata_dict->SetString("v8-version", V8_VERSION_STRING);
  metadata_dict->SetString("user-agent", GetContentClient()->GetUserAgent());

  // OS
#if defined(OS_CHROMEOS)
  metadata_dict->SetString("os-name", "CrOS");
  int32_t major_version;
  int32_t minor_version;
  int32_t bugfix_version;
  // OperatingSystemVersion only has a POSIX implementation which returns the
  // wrong versions for CrOS.
  base::SysInfo::OperatingSystemVersionNumbers(&major_version, &minor_version,
                                               &bugfix_version);
  metadata_dict->SetString(
      "os-version", base::StringPrintf("%d.%d.%d", major_version, minor_version,
                                       bugfix_version));
#else
  metadata_dict->SetString("os-name", base::SysInfo::OperatingSystemName());
  metadata_dict->SetString("os-version",
                           base::SysInfo::OperatingSystemVersion());
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

  std::string cpu_brand = cpu.cpu_brand();
  // Workaround for crbug.com/249713.
  // TODO(oysteine): Remove workaround when bug is fixed.
  size_t null_pos = cpu_brand.find('\0');
  if (null_pos != std::string::npos)
    cpu_brand.erase(null_pos);
  metadata_dict->SetString("cpu-brand", cpu_brand);

  // GPU
  gpu::GPUInfo gpu_info = content::GpuDataManager::GetInstance()->GetGPUInfo();

#if !defined(OS_ANDROID)
  metadata_dict->SetInteger("gpu-venid", gpu_info.gpu.vendor_id);
  metadata_dict->SetInteger("gpu-devid", gpu_info.gpu.device_id);
#endif

  metadata_dict->SetString("gpu-driver", gpu_info.driver_version);
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
  base::Time::Now().UTCExplode(&ctime);
  std::string time_string = base::StringPrintf(
      "%u-%u-%u %d:%d:%d", ctime.year, ctime.month, ctime.day_of_month,
      ctime.hour, ctime.minute, ctime.second);
  metadata_dict->SetString("trace-capture-datetime", time_string);

  return metadata_dict;
}

}  // namespace

TracingController* TracingController::GetInstance() {
  return TracingControllerImpl::GetInstance();
}

TracingControllerImpl::TracingControllerImpl()
    : delegate_(GetContentClient()->browser()->GetTracingDelegate()) {
  DCHECK(!g_controller);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Deliberately leaked, like this class.
  base::FileTracing::SetProvider(new FileTracingProviderImpl);
  AddAgents();
  g_controller = this;
}

TracingControllerImpl::~TracingControllerImpl() = default;

void TracingControllerImpl::AddAgents() {
  auto* connector =
      content::ServiceManagerConnection::GetForProcess()->GetConnector();
  connector->BindInterface(resource_coordinator::mojom::kServiceName,
                           &coordinator_);

// Register tracing agents.
#if defined(ENABLE_POWER_TRACING)
  agents_.push_back(base::MakeUnique<PowerTracingAgent>(connector));
#endif

#if defined(OS_CHROMEOS)
  agents_.push_back(base::MakeUnique<CrOSTracingAgent>(connector));
  agents_.push_back(base::MakeUnique<ArcTracingAgentImpl>(connector));
#elif defined(OS_WIN)
  agents_.push_back(base::MakeUnique<EtwTracingAgent>(connector));
#endif

  auto chrome_agent =
      base::MakeUnique<tracing::ChromeTraceEventAgent>(connector);
  // For adding general CPU, network, OS, and other system information to the
  // metadata.
  chrome_agent->AddMetadataGeneratorFunction(
      base::BindRepeating(&GenerateSystemMetadataDict));
  // For adding controller information to the metadata.
  chrome_agent->AddMetadataGeneratorFunction(base::BindRepeating(
      &TracingControllerImpl::GenerateMetadataDict, base::Unretained(this)));
  if (delegate_) {
    chrome_agent->AddMetadataGeneratorFunction(
        base::BindRepeating(&TracingDelegate::GenerateMetadataDict,
                            base::Unretained(delegate_.get())));
  }
  agents_.push_back(std::move(chrome_agent));
}

std::unique_ptr<base::DictionaryValue>
TracingControllerImpl::GenerateMetadataDict() const {
  auto metadata_dict = base::MakeUnique<base::DictionaryValue>();
  metadata_dict->SetString("trace-config", trace_config_->ToString());
  return metadata_dict;
}

TracingControllerImpl* TracingControllerImpl::GetInstance() {
  DCHECK(g_controller);
  return g_controller;
}

bool TracingControllerImpl::GetCategories(
    const GetCategoriesDoneCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  coordinator_->GetCategories(base::BindRepeating(
      [](const GetCategoriesDoneCallback& callback, bool success,
         const std::string& categories) {
        const std::vector<std::string> split = base::SplitString(
            categories, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
        std::set<std::string> category_set;
        for (const auto& category : split) {
          category_set.insert(category);
        }
        callback.Run(category_set);
      },
      callback));
  // TODO(chiniforooshan): The actual success value should be sent by the
  // callback asynchronously.
  return true;
}

bool TracingControllerImpl::StartTracing(
    const base::trace_event::TraceConfig& trace_config,
    const StartTracingDoneCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  trace_config_ =
      base::MakeUnique<base::trace_event::TraceConfig>(trace_config);
  coordinator_->StartTracing(
      trace_config.ToString(),
      base::BindRepeating(
          [](const StartTracingDoneCallback& callback, bool success) {
            if (!callback.is_null())
              callback.Run();
          },
          callback));
  // TODO(chiniforooshan): The actual success value should be sent by the
  // callback asynchronously.
  return true;
}

bool TracingControllerImpl::StopTracing(
    const scoped_refptr<TraceDataEndpoint>& trace_data_endpoint) {
  return StopTracing(std::move(trace_data_endpoint), "");
}

bool TracingControllerImpl::StopTracing(
    const scoped_refptr<TraceDataEndpoint>& trace_data_endpoint,
    const std::string& agent_label) {
  if (!IsTracing())
    return false;
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  trace_data_endpoint_ = std::move(trace_data_endpoint);
  is_data_complete_ = false;
  is_metadata_available_ = false;
  mojo::DataPipe data_pipe;
  drainer_.reset(new mojo::common::DataPipeDrainer(
      this, std::move(data_pipe.consumer_handle)));
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
    const GetTraceBufferUsageCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  coordinator_->RequestBufferUsage(base::BindRepeating(
      [](const GetTraceBufferUsageCallback& callback, bool success,
         float percent_full, uint32_t approximate_count) {
        callback.Run(percent_full, approximate_count);
      },
      callback));
  // TODO(chiniforooshan): The actual success value should be sent by the
  // callback asynchronously.
  return true;
}

bool TracingControllerImpl::IsTracing() const {
  return trace_config_ != nullptr;
}

void TracingControllerImpl::RegisterTracingUI(TracingUI* tracing_ui) {
  DCHECK(tracing_uis_.find(tracing_ui) == tracing_uis_.end());
  tracing_uis_.insert(tracing_ui);
}

void TracingControllerImpl::UnregisterTracingUI(TracingUI* tracing_ui) {
  std::set<TracingUI*>::iterator it = tracing_uis_.find(tracing_ui);
  DCHECK(it != tracing_uis_.end());
  tracing_uis_.erase(it);
}

void TracingControllerImpl::OnDataAvailable(const void* data,
                                            size_t num_bytes) {
  if (trace_data_endpoint_) {
    const std::string chunk(static_cast<const char*>(data), num_bytes);
    trace_data_endpoint_->ReceiveTraceChunk(
        base::MakeUnique<std::string>(chunk));
  }
}

void TracingControllerImpl::CompleteFlush() {
  if (trace_data_endpoint_) {
    trace_data_endpoint_->ReceiveTraceFinalContents(
        std::move(filtered_metadata_));
  }
  trace_data_endpoint_ = nullptr;
  trace_config_ = nullptr;
}

void TracingControllerImpl::OnDataComplete() {
  is_data_complete_ = true;
  if (is_metadata_available_)
    CompleteFlush();
}

void TracingControllerImpl::OnMetadataAvailable(
    std::unique_ptr<base::DictionaryValue> metadata) {
  DCHECK(!filtered_metadata_);
  is_metadata_available_ = true;
  MetadataFilterPredicate metadata_filter;
  if (trace_config_->IsArgumentFilterEnabled()) {
    if (delegate_)
      metadata_filter = delegate_->GetMetadataFilterPredicate();
  }
  if (metadata_filter.is_null()) {
    filtered_metadata_ = std::move(metadata);
  } else {
    filtered_metadata_ = base::MakeUnique<base::DictionaryValue>();
    for (base::DictionaryValue::Iterator it(*metadata); !it.IsAtEnd();
         it.Advance()) {
      if (metadata_filter.Run(it.key())) {
        filtered_metadata_->Set(
            it.key(), base::MakeUnique<base::Value>(it.value().Clone()));
      } else {
        filtered_metadata_->SetString(it.key(), "__stripped__");
      }
    }
  }
  if (is_data_complete_)
    CompleteFlush();
}

}  // namespace content
