// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc_logging_handler_host.h"

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/cpu.h"
#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/sys_info.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/cros_settings_names.h"
#include "chrome/browser/media/webrtc_log_uploader.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/media/webrtc_logging_messages.h"
#include "chrome/common/partial_circular_buffer.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/render_process_host.h"
#include "gpu/config/gpu_info.h"
#include "net/base/address_family.h"
#include "net/base/net_util.h"
#include "net/url_request/url_request_context_getter.h"

#if defined(OS_LINUX)
#include "base/linux_util.h"
#endif

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#endif

using base::IntToString;
using content::BrowserThread;


#if defined(OS_ANDROID)
const size_t kWebRtcLogSize = 1 * 1024 * 1024;  // 1 MB
#else
const size_t kWebRtcLogSize = 6 * 1024 * 1024;  // 6 MB
#endif

namespace {

// For privacy reasons when logging IP addresses. The returned "sensitive
// string" is for release builds a string with the end stripped away. Last
// octet for IPv4 and last 80 bits (5 groups) for IPv6. String will be
// "1.2.3.x" and "1.2.3::" respectively. For debug builds, the string is
// not stripped.
std::string IPAddressToSensitiveString(const net::IPAddressNumber& address) {
#if defined(NDEBUG)
  std::string sensitive_address;
  switch (net::GetAddressFamily(address)) {
    case net::ADDRESS_FAMILY_IPV4: {
      sensitive_address = net::IPAddressToString(address);
      size_t find_pos = sensitive_address.rfind('.');
      if (find_pos == std::string::npos)
        return std::string();
      sensitive_address.resize(find_pos);
      sensitive_address += ".x";
      break;
    }
    case net::ADDRESS_FAMILY_IPV6: {
      // TODO(grunell): Create a string of format "1:2:3:x:x:x:x:x" to clarify
      // that the end has been stripped out.
      net::IPAddressNumber sensitive_address_number = address;
      sensitive_address_number.resize(net::kIPv6AddressSize - 10);
      sensitive_address_number.resize(net::kIPv6AddressSize, 0);
      sensitive_address = net::IPAddressToString(sensitive_address_number);
      break;
    }
    case net::ADDRESS_FAMILY_UNSPECIFIED: {
      break;
    }
  }
  return sensitive_address;
#else
  return net::IPAddressToString(address);
#endif
}

}  // namespace

WebRtcLoggingHandlerHost::WebRtcLoggingHandlerHost() {}

WebRtcLoggingHandlerHost::~WebRtcLoggingHandlerHost() {}

void WebRtcLoggingHandlerHost::OnChannelClosing() {
  UploadLog();
  content::BrowserMessageFilter::OnChannelClosing();
}

void WebRtcLoggingHandlerHost::OnDestruct() const {
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

bool WebRtcLoggingHandlerHost::OnMessageReceived(const IPC::Message& message,
                                                 bool* message_was_ok) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(WebRtcLoggingHandlerHost, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(WebRtcLoggingMsg_OpenLog, OnOpenLog)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()

  return handled;
}

void WebRtcLoggingHandlerHost::OnOpenLog(const std::string& app_session_id,
                                         const std::string& app_url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  app_session_id_ = app_session_id;
  app_url_ = app_url;
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, base::Bind(
      &WebRtcLoggingHandlerHost::OpenLogIfAllowed, this));
}

void WebRtcLoggingHandlerHost::OpenLogIfAllowed() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // If the user permits metrics reporting / crash uploading with the checkbox
  // in the prefs, we allow uploading automatically. We disable uploading
  // completely for non-official builds. Allowing can be forced with a flag.
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kEnableMetricsReportingForTesting)) {
    bool enabled = false;
#if defined(GOOGLE_CHROME_BUILD)
#if defined(OS_CHROMEOS)
    chromeos::CrosSettings::Get()->GetBoolean(chromeos::kStatsReportingPref,
                                              &enabled);
#elif defined(OS_ANDROID)
    // Android has its own settings for metrics / crash uploading.
    enabled = g_browser_process->local_state()->GetBoolean(
        prefs::kCrashReportingEnabled);
#else
    enabled = g_browser_process->local_state()->GetBoolean(
        prefs::kMetricsReportingEnabled);
#endif  // #if defined(OS_CHROMEOS)
#endif  // defined(GOOGLE_CHROME_BUILD)
    if (!enabled)
      return;
  }

  if (!g_browser_process->webrtc_log_uploader()->ApplyForStartLogging())
    return;

  system_request_context_ = g_browser_process->system_request_context();
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, base::Bind(
      &WebRtcLoggingHandlerHost::DoOpenLog, this));
}

void WebRtcLoggingHandlerHost::DoOpenLog() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!shared_memory_);

  shared_memory_.reset(new base::SharedMemory());

  if (!shared_memory_->CreateAndMapAnonymous(kWebRtcLogSize)) {
    DLOG(ERROR) << "Failed to create shared memory.";
    Send(new WebRtcLoggingMsg_OpenLogFailed());
    return;
  }

  if (!shared_memory_->ShareToProcess(PeerHandle(),
                                     &foreign_memory_handle_)) {
    Send(new WebRtcLoggingMsg_OpenLogFailed());
    return;
  }

  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE, base::Bind(
      &WebRtcLoggingHandlerHost::LogMachineInfo, this));
}

void WebRtcLoggingHandlerHost::LogMachineInfo() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  PartialCircularBuffer pcb(shared_memory_->memory(),
                            kWebRtcLogSize,
                            kWebRtcLogSize / 2,
                            false);

  // App session ID
  std::string info = "App session ID: " + app_session_id_ + '\n';
  pcb.Write(info.c_str(), info.length());

  // OS
  info = base::SysInfo::OperatingSystemName() + " " +
         base::SysInfo::OperatingSystemVersion() + " " +
         base::SysInfo::OperatingSystemArchitecture() + '\n';
  pcb.Write(info.c_str(), info.length());
#if defined(OS_LINUX)
  info = "Linux distribution: " + base::GetLinuxDistro() + '\n';
  pcb.Write(info.c_str(), info.length());
#endif

  // CPU
  base::CPU cpu;
  info = "Cpu: " + IntToString(cpu.family()) + "." + IntToString(cpu.model()) +
         "." + IntToString(cpu.stepping()) +
         ", x" + IntToString(base::SysInfo::NumberOfProcessors()) + ", " +
         IntToString(base::SysInfo::AmountOfPhysicalMemoryMB()) + "MB" + '\n';
  pcb.Write(info.c_str(), info.length());
  std::string cpu_brand = cpu.cpu_brand();
  // Workaround for crbug.com/249713.
  // TODO(grunell): Remove workaround when bug is fixed.
  size_t null_pos = cpu_brand.find('\0');
  if (null_pos != std::string::npos)
    cpu_brand.erase(null_pos);
  info = "Cpu brand: " + cpu_brand + '\n';
  pcb.Write(info.c_str(), info.length());

  // Computer model
#if defined(OS_MACOSX)
  info = "Computer model: " + base::mac::GetModelIdentifier() + '\n';
#else
  info = "Computer model: Not available\n";
#endif
  pcb.Write(info.c_str(), info.length());

  // GPU
  gpu::GPUInfo gpu_info = content::GpuDataManager::GetInstance()->GetGPUInfo();
  info = "Gpu: machine-model='" + gpu_info.machine_model +
         "', vendor-id=" + IntToString(gpu_info.gpu.vendor_id) +
         ", device-id=" + IntToString(gpu_info.gpu.device_id) +
         ", driver-vendor='" + gpu_info.driver_vendor +
         "', driver-version=" + gpu_info.driver_version + '\n';
  pcb.Write(info.c_str(), info.length());

  // Network interfaces
  net::NetworkInterfaceList network_list;
  net::GetNetworkList(&network_list);
  info  = "Discovered " + IntToString(network_list.size()) +
          " network interfaces:" + '\n';
  pcb.Write(info.c_str(), info.length());
  for (net::NetworkInterfaceList::iterator it = network_list.begin();
       it != network_list.end(); ++it) {
    info = "Name: " + it->name +
           ", Address: " + IPAddressToSensitiveString(it->address) + '\n';
    pcb.Write(info.c_str(), info.length());
  }

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, base::Bind(
      &WebRtcLoggingHandlerHost::NotifyLogOpened, this));
}

void WebRtcLoggingHandlerHost::NotifyLogOpened() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  Send(new WebRtcLoggingMsg_LogOpened(foreign_memory_handle_, kWebRtcLogSize));
}

void WebRtcLoggingHandlerHost::UploadLog() {
  if (!shared_memory_)
    return;

  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE, base::Bind(
      &WebRtcLogUploader::UploadLog,
      base::Unretained(g_browser_process->webrtc_log_uploader()),
      system_request_context_,
      Passed(&shared_memory_),
      kWebRtcLogSize,
      app_session_id_,
      app_url_));
}
