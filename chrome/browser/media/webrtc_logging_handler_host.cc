// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc_logging_handler_host.h"

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/cpu.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/sys_info.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/media/webrtc_log_list.h"
#include "chrome/browser/media/webrtc_log_uploader.h"
#include "chrome/browser/media/webrtc_rtp_dump_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
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
#include "net/url_request/url_request_context_getter.h"

#if defined(OS_LINUX)
#include "base/linux_util.h"
#endif

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#endif

#if defined(OS_CHROMEOS)
#include "chromeos/system/statistics_provider.h"
#endif

using base::IntToString;
using content::BrowserThread;

namespace {

#if defined(OS_ANDROID)
const size_t kWebRtcLogSize = 1 * 1024 * 1024;  // 1 MB
#else
const size_t kWebRtcLogSize = 6 * 1024 * 1024;  // 6 MB
#endif

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

void FormatMetaDataAsLogMessage(
    const MetaDataMap& meta_data,
    std::string* message) {
  for (MetaDataMap::const_iterator it = meta_data.begin();
       it != meta_data.end(); ++it) {
    *message += it->first + ": " + it->second + '\n';
  }
  // Remove last '\n'.
  message->resize(message->size() - 1);
}

}  // namespace

WebRtcLoggingHandlerHost::WebRtcLoggingHandlerHost(Profile* profile)
    : BrowserMessageFilter(WebRtcLoggingMsgStart),
      profile_(profile),
      logging_state_(CLOSED),
      upload_log_on_render_close_(false) {
  DCHECK(profile_);
}

WebRtcLoggingHandlerHost::~WebRtcLoggingHandlerHost() {}

void WebRtcLoggingHandlerHost::SetMetaData(
    const MetaDataMap& meta_data,
    const GenericDoneCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!callback.is_null());

  std::string error_message;
  if (logging_state_ == CLOSED) {
    meta_data_ = meta_data;
  } else if (logging_state_ == STARTED) {
    meta_data_ = meta_data;
    std::string meta_data_message;
    FormatMetaDataAsLogMessage(meta_data_, &meta_data_message);
    LogToCircularBuffer(meta_data_message);
  } else {
    error_message = "Meta data must be set before stop or upload.";
  }
  bool success = error_message.empty();
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
  logging_state_ = UPLOADING;
  content::BrowserThread::PostTaskAndReplyWithResult(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&WebRtcLoggingHandlerHost::GetLogDirectoryAndEnsureExists,
                 this),
      base::Bind(&WebRtcLoggingHandlerHost::TriggerUpload, this));
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
  circular_buffer_.reset();
  log_buffer_.reset();
  logging_state_ = CLOSED;
  rtp_dump_handler_.reset();
  stop_rtp_dump_callback_.Reset();
  FireGenericDoneCallback(&discard_callback, true, "");
}

void WebRtcLoggingHandlerHost::LogMessage(const std::string& message) {
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(
          &WebRtcLoggingHandlerHost::AddLogMessageFromBrowser,
          this,
          WebRtcLoggingMessageData(base::Time::Now(), message)));
}

void WebRtcLoggingHandlerHost::StartRtpDump(
    RtpDumpType type,
    const GenericDoneCallback& callback,
    const content::RenderProcessHost::WebRtcStopRtpDumpCallback&
        stop_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(stop_rtp_dump_callback_.is_null() ||
         stop_rtp_dump_callback_.Equals(stop_callback));

  stop_rtp_dump_callback_ = stop_callback;

  if (!rtp_dump_handler_) {
    content::BrowserThread::PostTaskAndReplyWithResult(
        content::BrowserThread::FILE,
        FROM_HERE,
        base::Bind(&WebRtcLoggingHandlerHost::GetLogDirectoryAndEnsureExists,
                   this),
        base::Bind(&WebRtcLoggingHandlerHost::CreateRtpDumpHandlerAndStart,
                   this,
                   type,
                   callback));
    return;
  }

  GenericDoneCallback start_callback = callback;
  DoStartRtpDump(type, &start_callback);
}

void WebRtcLoggingHandlerHost::StopRtpDump(
    RtpDumpType type,
    const GenericDoneCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!callback.is_null());

  if (!rtp_dump_handler_) {
    GenericDoneCallback stop_callback = callback;
    FireGenericDoneCallback(
        &stop_callback, false, "RTP dump has not been started.");
    return;
  }

  if (!stop_rtp_dump_callback_.is_null()) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(stop_rtp_dump_callback_,
                   type == RTP_DUMP_INCOMING || type == RTP_DUMP_BOTH,
                   type == RTP_DUMP_OUTGOING || type == RTP_DUMP_BOTH));
  }

  rtp_dump_handler_->StopDump(type, callback);
}

void WebRtcLoggingHandlerHost::OnRtpPacket(scoped_ptr<uint8[]> packet_header,
                                           size_t header_length,
                                           size_t packet_length,
                                           bool incoming) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&WebRtcLoggingHandlerHost::DumpRtpPacketOnIOThread,
                 this,
                 base::Passed(&packet_header),
                 header_length,
                 packet_length,
                 incoming));
}

void WebRtcLoggingHandlerHost::DumpRtpPacketOnIOThread(
    scoped_ptr<uint8[]> packet_header,
    size_t header_length,
    size_t packet_length,
    bool incoming) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // |rtp_dump_handler_| could be NULL if we are waiting for the FILE thread to
  // create/ensure the log directory.
  if (rtp_dump_handler_) {
    rtp_dump_handler_->OnRtpPacket(
        packet_header.get(), header_length, packet_length, incoming);
  }
}

void WebRtcLoggingHandlerHost::OnChannelClosing() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (logging_state_ == STARTED || logging_state_ == STOPPED) {
    if (upload_log_on_render_close_) {
      logging_state_ = UPLOADING;
      logging_started_time_ = base::Time();

      content::BrowserThread::PostTaskAndReplyWithResult(
          content::BrowserThread::FILE,
          FROM_HERE,
          base::Bind(&WebRtcLoggingHandlerHost::GetLogDirectoryAndEnsureExists,
                     this),
          base::Bind(&WebRtcLoggingHandlerHost::TriggerUpload, this));
    } else {
      g_browser_process->webrtc_log_uploader()->LoggingStoppedDontUpload();
    }
  }
  content::BrowserMessageFilter::OnChannelClosing();
}

void WebRtcLoggingHandlerHost::OnDestruct() const {
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

bool WebRtcLoggingHandlerHost::OnMessageReceived(const IPC::Message& message) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WebRtcLoggingHandlerHost, message)
    IPC_MESSAGE_HANDLER(WebRtcLoggingMsg_AddLogMessages, OnAddLogMessages)
    IPC_MESSAGE_HANDLER(WebRtcLoggingMsg_LoggingStopped,
                        OnLoggingStoppedInRenderer)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void WebRtcLoggingHandlerHost::AddLogMessageFromBrowser(
    const WebRtcLoggingMessageData& message) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (logging_state_ == STARTED)
    LogToCircularBuffer(message.Format(logging_started_time_));
}

void WebRtcLoggingHandlerHost::OnAddLogMessages(
    const std::vector<WebRtcLoggingMessageData>& messages) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (logging_state_ == STARTED || logging_state_ == STOPPING) {
    for (size_t i = 0; i < messages.size(); ++i) {
      LogToCircularBuffer(messages[i].Format(logging_started_time_));
    }
  }
}

void WebRtcLoggingHandlerHost::OnLoggingStoppedInRenderer() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (logging_state_ != STOPPING) {
    // If an out-of-order response is received, stop_callback_ may be invalid,
    // and must not be invoked.
    DLOG(ERROR) << "OnLoggingStoppedInRenderer invoked in state "
                << logging_state_;
    BadMessageReceived();
    return;
  }
  logging_started_time_ = base::Time();
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
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, base::Bind(
      &WebRtcLoggingHandlerHost::DoStartLogging, this));
}

void WebRtcLoggingHandlerHost::DoStartLogging() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  log_buffer_.reset(new unsigned char[kWebRtcLogSize]);
  circular_buffer_.reset(
    new PartialCircularBuffer(log_buffer_.get(),
                              kWebRtcLogSize,
                              kWebRtcLogSize / 2,
                              false));

  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE, base::Bind(
      &WebRtcLoggingHandlerHost::LogInitialInfoOnFileThread, this));
}

void WebRtcLoggingHandlerHost::LogInitialInfoOnFileThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  net::NetworkInterfaceList network_list;
  net::GetNetworkList(&network_list,
                      net::EXCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES);

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, base::Bind(
      &WebRtcLoggingHandlerHost::LogInitialInfoOnIOThread, this, network_list));
}

void WebRtcLoggingHandlerHost::LogInitialInfoOnIOThread(
    const net::NetworkInterfaceList& network_list) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Log start time (current time). We don't use base/i18n/time_formatting.h
  // here because we don't want the format of the current locale.
  base::Time::Exploded now = {0};
  base::Time::Now().LocalExplode(&now);
  LogToCircularBuffer(base::StringPrintf(
      "Start %d-%02d-%02d %02d:%02d:%02d", now.year, now.month,
      now.day_of_month, now.hour, now.minute, now.second));

  // Write metadata if received before logging started.
  if (!meta_data_.empty()) {
    std::string info;
    FormatMetaDataAsLogMessage(meta_data_, &info);
    LogToCircularBuffer(info);
  }

  // Chrome version
  chrome::VersionInfo version_info;
  LogToCircularBuffer("Chrome version: " + version_info.Version() + " " +
                      chrome::VersionInfo::GetVersionStringModifier());

  // OS
  LogToCircularBuffer(base::SysInfo::OperatingSystemName() + " " +
                      base::SysInfo::OperatingSystemVersion() + " " +
                      base::SysInfo::OperatingSystemArchitecture());
#if defined(OS_LINUX)
  LogToCircularBuffer("Linux distribution: " + base::GetLinuxDistro());
#endif

  // CPU
  base::CPU cpu;
  LogToCircularBuffer(
      "Cpu: " + IntToString(cpu.family()) + "." + IntToString(cpu.model()) +
      "." + IntToString(cpu.stepping()) + ", x" +
      IntToString(base::SysInfo::NumberOfProcessors()) + ", " +
      IntToString(base::SysInfo::AmountOfPhysicalMemoryMB()) + "MB");
  std::string cpu_brand = cpu.cpu_brand();
  // Workaround for crbug.com/249713.
  // TODO(grunell): Remove workaround when bug is fixed.
  size_t null_pos = cpu_brand.find('\0');
  if (null_pos != std::string::npos)
    cpu_brand.erase(null_pos);
  LogToCircularBuffer("Cpu brand: " + cpu_brand);

  // Computer model
  std::string computer_model = "Not available";
#if defined(OS_MACOSX)
  computer_model = base::mac::GetModelIdentifier();
#elif defined(OS_CHROMEOS)
  chromeos::system::StatisticsProvider::GetInstance()->
      GetMachineStatistic(chromeos::system::kHardwareClassKey, &computer_model);
#endif
  LogToCircularBuffer("Computer model: " + computer_model);

  // GPU
  gpu::GPUInfo gpu_info = content::GpuDataManager::GetInstance()->GetGPUInfo();
  LogToCircularBuffer(
      "Gpu: machine-model-name=" + gpu_info.machine_model_name +
      ", machine-model-version=" + gpu_info.machine_model_version +
      ", vendor-id=" + IntToString(gpu_info.gpu.vendor_id) +
      ", device-id=" + IntToString(gpu_info.gpu.device_id) +
      ", driver-vendor=" + gpu_info.driver_vendor +
      ", driver-version=" + gpu_info.driver_version);
  LogToCircularBuffer(
      "OpenGL: gl-vendor=" + gpu_info.gl_vendor +
      ", gl-renderer=" + gpu_info.gl_renderer +
      ", gl-version=" + gpu_info.gl_version);

  // Network interfaces
  LogToCircularBuffer("Discovered " + IntToString(network_list.size()) +
                      " network interfaces:");
  for (net::NetworkInterfaceList::const_iterator it = network_list.begin();
       it != network_list.end(); ++it) {
    LogToCircularBuffer("Name: " + it->friendly_name + ", Address: " +
                        IPAddressToSensitiveString(it->address));
  }

  NotifyLoggingStarted();
}

void WebRtcLoggingHandlerHost::NotifyLoggingStarted() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  Send(new WebRtcLoggingMsg_StartLogging());
  logging_started_time_ = base::Time::Now();
  logging_state_ = STARTED;
  FireGenericDoneCallback(&start_callback_, true, "");
}

void WebRtcLoggingHandlerHost::LogToCircularBuffer(const std::string& message) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(circular_buffer_.get());
  circular_buffer_->Write(message.c_str(), message.length());
  const char eol = '\n';
  circular_buffer_->Write(&eol, 1);
}

base::FilePath WebRtcLoggingHandlerHost::GetLogDirectoryAndEnsureExists() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  base::FilePath log_dir_path =
      WebRtcLogList::GetWebRtcLogDirectoryForProfile(profile_->GetPath());
  base::File::Error error;
  if (!base::CreateDirectoryAndGetError(log_dir_path, &error)) {
    DLOG(ERROR) << "Could not create WebRTC log directory, error: " << error;
    return base::FilePath();
  }
  return log_dir_path;
}

void WebRtcLoggingHandlerHost::TriggerUpload(
    const base::FilePath& log_directory) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK_EQ(logging_state_, UPLOADING);

  if (rtp_dump_handler_) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(stop_rtp_dump_callback_, true, true));

    rtp_dump_handler_->StopOngoingDumps(
        base::Bind(&WebRtcLoggingHandlerHost::DoUploadLogAndRtpDumps,
                   this,
                   log_directory));
    return;
  }

  DoUploadLogAndRtpDumps(log_directory);
}

void WebRtcLoggingHandlerHost::DoUploadLogAndRtpDumps(
    const base::FilePath& log_directory) {
  WebRtcLogUploadDoneData upload_done_data;
  upload_done_data.log_path = log_directory;

  if (rtp_dump_handler_) {
    WebRtcRtpDumpHandler::ReleasedDumps rtp_dumps(
        rtp_dump_handler_->ReleaseDumps());
    upload_done_data.incoming_rtp_dump = rtp_dumps.incoming_dump_path;
    upload_done_data.outgoing_rtp_dump = rtp_dumps.outgoing_dump_path;

    rtp_dump_handler_.reset();
    stop_rtp_dump_callback_.Reset();
  }

  upload_done_data.callback = upload_callback_;
  upload_done_data.host = this;
  upload_callback_.Reset();

  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE, base::Bind(
      &WebRtcLogUploader::LoggingStoppedDoUpload,
      base::Unretained(g_browser_process->webrtc_log_uploader()),
      Passed(&log_buffer_),
      kWebRtcLogSize,
      meta_data_,
      upload_done_data));

  meta_data_.clear();
  circular_buffer_.reset();
}

void WebRtcLoggingHandlerHost::FireGenericDoneCallback(
    GenericDoneCallback* callback,
    bool success,
    const std::string& error_message) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!(*callback).is_null());
  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(*callback, success, error_message));
  (*callback).Reset();
}

void WebRtcLoggingHandlerHost::CreateRtpDumpHandlerAndStart(
    RtpDumpType type,
    GenericDoneCallback callback,
    const base::FilePath& dump_dir) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // |rtp_dump_handler_| may be non-NULL if StartRtpDump is called again before
  // GetLogDirectoryAndEnsureExists returns on the FILE thread for a previous
  // StartRtpDump.
  if (!rtp_dump_handler_)
    rtp_dump_handler_.reset(new WebRtcRtpDumpHandler(dump_dir));

  DoStartRtpDump(type, &callback);
}

void WebRtcLoggingHandlerHost::DoStartRtpDump(RtpDumpType type,
                                              GenericDoneCallback* callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(rtp_dump_handler_);

  std::string error;

  bool result = rtp_dump_handler_->StartDump(type, &error);
  FireGenericDoneCallback(callback, result, error);
}
