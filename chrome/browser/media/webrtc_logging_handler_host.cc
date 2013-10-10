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
#include "chrome/browser/media/webrtc_log_uploader.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/media/webrtc_logging_messages.h"
#include "chrome/common/partial_circular_buffer.h"
#include "chrome/common/pref_names.h"
#include "chromeos/settings/cros_settings_names.h"
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

const char kLogNotStoppedOrNoLogOpen[] =
    "Logging not stopped or no log open.";

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

WebRtcLoggingHandlerHost::WebRtcLoggingHandlerHost()
    : logging_state_(CLOSED),
      upload_log_on_render_close_(false) {
}

WebRtcLoggingHandlerHost::~WebRtcLoggingHandlerHost() {}

void WebRtcLoggingHandlerHost::SetMetaData(
    const std::map<std::string, std::string>& meta_data,
    const GenericDoneCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!callback.is_null());

  bool success = false;
  std::string error_message;
  if (logging_state_ == CLOSED) {
    meta_data_ = meta_data;
    success = true;
  } else {
    error_message = "Meta data must be set before starting";
  }
  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                   base::Bind(callback, success,
                                              error_message));
}

void WebRtcLoggingHandlerHost::StartLogging(
    const GenericDoneCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!callback.is_null());

  start_callback_ = callback;
  if (logging_state_ != CLOSED) {
    FireGenericDoneCallback(&start_callback_, false, "A log is already open");
    return;
  }
  logging_state_ = STARTING;
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, base::Bind(
      &WebRtcLoggingHandlerHost::StartLoggingIfAllowed, this));
}

void WebRtcLoggingHandlerHost::StopLogging(
    const GenericDoneCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!callback.is_null());

  stop_callback_ = callback;
  if (logging_state_ != STARTED) {
    FireGenericDoneCallback(&stop_callback_, false, "Logging not started");
    return;
  }
  logging_state_ = STOPPING;
  Send(new WebRtcLoggingMsg_StopLogging());
}

void WebRtcLoggingHandlerHost::UploadLog(const UploadDoneCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!callback.is_null());

  if (logging_state_ != STOPPED) {
    if (!callback.is_null()) {
      content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
          base::Bind(callback, false, "", kLogNotStoppedOrNoLogOpen));
    }
    return;
  }
  upload_callback_ = callback;
  TriggerUploadLog();
}

void WebRtcLoggingHandlerHost::UploadLogDone() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  logging_state_ = CLOSED;
}

void WebRtcLoggingHandlerHost::DiscardLog(const GenericDoneCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!callback.is_null());

  GenericDoneCallback discard_callback = callback;
  if (logging_state_ != STOPPED) {
    FireGenericDoneCallback(&discard_callback, false,
                            kLogNotStoppedOrNoLogOpen);
    return;
  }
  g_browser_process->webrtc_log_uploader()->LoggingStoppedDontUpload();
  shared_memory_.reset(NULL);
  logging_state_ = CLOSED;
  FireGenericDoneCallback(&discard_callback, true, "");
}

void WebRtcLoggingHandlerHost::OnChannelClosing() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (logging_state_ == STARTED || logging_state_ == STOPPED) {
    if (upload_log_on_render_close_) {
      TriggerUploadLog();
    } else {
      g_browser_process->webrtc_log_uploader()->LoggingStoppedDontUpload();
    }
  }
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
    IPC_MESSAGE_HANDLER(WebRtcLoggingMsg_LoggingStopped,
                        OnLoggingStoppedInRenderer)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()

  return handled;
}

void WebRtcLoggingHandlerHost::OnLoggingStoppedInRenderer() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  logging_state_ = STOPPED;
  FireGenericDoneCallback(&stop_callback_, true, "");
}

void WebRtcLoggingHandlerHost::StartLoggingIfAllowed() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!g_browser_process->webrtc_log_uploader()->ApplyForStartLogging()) {
    logging_state_ = CLOSED;
      FireGenericDoneCallback(
          &start_callback_, false, "Cannot start, maybe the maximum number of "
          "simultaneuos logs has been reached.");
    return;
  }
  system_request_context_ = g_browser_process->system_request_context();
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, base::Bind(
      &WebRtcLoggingHandlerHost::DoStartLogging, this));
}

void WebRtcLoggingHandlerHost::DoStartLogging() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!shared_memory_);

  shared_memory_.reset(new base::SharedMemory());

  if (!shared_memory_->CreateAndMapAnonymous(kWebRtcLogSize)) {
    const std::string error_message = "Failed to create shared memory";
    DLOG(ERROR) << error_message;
    logging_state_ = CLOSED;
    FireGenericDoneCallback(&start_callback_, false, error_message);
    return;
  }

  if (!shared_memory_->ShareToProcess(PeerHandle(),
                                     &foreign_memory_handle_)) {
    const std::string error_message =
        "Failed to share memory to render process";
    DLOG(ERROR) << error_message;
    logging_state_ = CLOSED;
    FireGenericDoneCallback(&start_callback_, false, error_message);
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

  // Meta data
  std::string info;
  std::map<std::string, std::string>::iterator it = meta_data_.begin();
  for (; it != meta_data_.end(); ++it) {
    info = it->first + ": " + it->second + '\n';
    pcb.Write(info.c_str(), info.length());
  }

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
      &WebRtcLoggingHandlerHost::NotifyLoggingStarted, this));
}

void WebRtcLoggingHandlerHost::NotifyLoggingStarted() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  Send(new WebRtcLoggingMsg_StartLogging(foreign_memory_handle_,
                                         kWebRtcLogSize));
  logging_state_ = STARTED;
  FireGenericDoneCallback(&start_callback_, true, "");
}

void WebRtcLoggingHandlerHost::TriggerUploadLog() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(logging_state_ == STOPPED);

  logging_state_ = UPLOADING;
  WebRtcLogUploadDoneData upload_done_data;
  upload_done_data.callback = upload_callback_;
  upload_done_data.host = this;
  upload_callback_.Reset();

  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE, base::Bind(
      &WebRtcLogUploader::LoggingStoppedDoUpload,
      base::Unretained(g_browser_process->webrtc_log_uploader()),
      system_request_context_,
      Passed(&shared_memory_),
      kWebRtcLogSize,
      meta_data_,
      upload_done_data));

  meta_data_.clear();
}

void WebRtcLoggingHandlerHost::FireGenericDoneCallback(
    GenericDoneCallback* callback, bool success,
    const std::string& error_message) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!(*callback).is_null());
  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                   base::Bind(*callback, success,
                                              error_message));
  (*callback).Reset();
}
