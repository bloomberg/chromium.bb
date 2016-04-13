// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc_logging_handler_host.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/cpu.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/sys_info.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/bad_message.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/media/webrtc_log_list.h"
#include "chrome/browser/media/webrtc_log_uploader.h"
#include "chrome/browser/media/webrtc_rtp_dump_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/media/webrtc_logging_messages.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/render_process_host.h"
#include "gpu/config/gpu_info.h"
#include "net/base/ip_address.h"
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

// Key used to attach the handler to the RenderProcessHost.
const char WebRtcLoggingHandlerHost::kWebRtcLoggingHandlerHostKey[] =
    "kWebRtcLoggingHandlerHostKey";

namespace {

const char kLogNotStoppedOrNoLogOpen[] =
    "Logging not stopped or no log open.";

// For privacy reasons when logging IP addresses. The returned "sensitive
// string" is for release builds a string with the end stripped away. Last
// octet for IPv4 and last 80 bits (5 groups) for IPv6. String will be
// "1.2.3.x" and "1.2.3::" respectively. For debug builds, the string is
// not stripped.
std::string IPAddressToSensitiveString(const net::IPAddress& address) {
#if defined(NDEBUG)
  std::string sensitive_address;
  switch (address.size()) {
    case net::IPAddress::kIPv4AddressSize: {
      sensitive_address = address.ToString();
      size_t find_pos = sensitive_address.rfind('.');
      if (find_pos == std::string::npos)
        return std::string();
      sensitive_address.resize(find_pos);
      sensitive_address += ".x";
      break;
    }
    case net::IPAddress::kIPv6AddressSize: {
      // TODO(grunell): Create a string of format "1:2:3:x:x:x:x:x" to clarify
      // that the end has been stripped out.
      std::vector<uint8_t> bytes = address.bytes();
      std::fill(bytes.begin() + 6, bytes.end(), 0);
      net::IPAddress stripped_address(bytes);
      sensitive_address = stripped_address.ToString();
      break;
    }
    default: { break; }
  }
  return sensitive_address;
#else
  return address.ToString();
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

WebRtcLogBuffer::WebRtcLogBuffer()
    : buffer_(),
      circular_(&buffer_[0], sizeof(buffer_), sizeof(buffer_) / 2, false),
      read_only_(false) {
}

WebRtcLogBuffer::~WebRtcLogBuffer() {
  DCHECK(read_only_ || thread_checker_.CalledOnValidThread());
}

void WebRtcLogBuffer::Log(const std::string& message) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!read_only_);
  circular_.Write(message.c_str(), message.length());
  const char eol = '\n';
  circular_.Write(&eol, 1);
}

PartialCircularBuffer WebRtcLogBuffer::Read() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(read_only_);
  return PartialCircularBuffer(&buffer_[0], sizeof(buffer_));
}

void WebRtcLogBuffer::SetComplete() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!read_only_) << "Already set? (programmer error)";
  read_only_ = true;
  // Detach from the current thread so that we can check reads on a different
  // thread.  This is to make sure that Read()s still happen on one thread only.
  thread_checker_.DetachFromThread();
}

WebRtcLoggingHandlerHost::WebRtcLoggingHandlerHost(
    int render_process_id,
    Profile* profile,
    WebRtcLogUploader* log_uploader)
    : BrowserMessageFilter(WebRtcLoggingMsgStart),
      profile_(profile),
      logging_state_(CLOSED),
      upload_log_on_render_close_(false),
      log_uploader_(log_uploader),
      render_process_id_(render_process_id) {
  DCHECK(profile_);
  DCHECK(log_uploader_);
}

WebRtcLoggingHandlerHost::~WebRtcLoggingHandlerHost() {
  // If we hit this, then we might be leaking a log reference count (see
  // ApplyForStartLogging).
  DCHECK_EQ(CLOSED, logging_state_);
}

void WebRtcLoggingHandlerHost::SetMetaData(
    std::unique_ptr<MetaDataMap> meta_data,
    const GenericDoneCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!callback.is_null());

  std::string error_message;
  if (logging_state_ == CLOSED) {
    if (!meta_data_.get())
      meta_data_ = std::move(meta_data);
  } else if (logging_state_ == STARTED) {
    std::string meta_data_message;
    FormatMetaDataAsLogMessage(*meta_data.get(), &meta_data_message);
    LogToCircularBuffer(meta_data_message);
  } else {
    error_message = "Meta data must be set before stop or upload.";
  }

  if (error_message.empty() && meta_data.get()) {
    // Keep the meta data around for uploading separately from the log.
    for (const auto& it : *meta_data.get())
      (*meta_data_.get())[it.first] = it.second;
  }

  FireGenericDoneCallback(callback, error_message.empty(), error_message);
}

void WebRtcLoggingHandlerHost::StartLogging(
    const GenericDoneCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!callback.is_null());

  if (logging_state_ != CLOSED) {
    FireGenericDoneCallback(callback, false, "A log is already open.");
    return;
  }

  if (!log_uploader_->ApplyForStartLogging()) {
    FireGenericDoneCallback(callback, false,
        "Cannot start, maybe the maximum number of "
        "simultaneuos logs has been reached.");
    return;
  }

  logging_state_ = STARTING;

  DCHECK(!log_buffer_.get());
  log_buffer_.reset(new WebRtcLogBuffer());
  if (!meta_data_.get())
    meta_data_.reset(new MetaDataMap());

  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE, base::Bind(
      &WebRtcLoggingHandlerHost::LogInitialInfoOnFileThread, this, callback));
}

void WebRtcLoggingHandlerHost::StopLogging(
    const GenericDoneCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!callback.is_null());

  if (logging_state_ != STARTED) {
    FireGenericDoneCallback(callback, false, "Logging not started.");
    return;
  }

  stop_callback_ = callback;
  logging_state_ = STOPPING;

  Send(new WebRtcLoggingMsg_StopLogging());

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &WebRtcLoggingHandlerHost::DisableBrowserProcessLoggingOnUIThread,
          this));
}

void WebRtcLoggingHandlerHost::UploadLog(const UploadDoneCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!callback.is_null());

  if (logging_state_ != STOPPED) {
    if (!callback.is_null()) {
      content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
          base::Bind(callback, false, "", kLogNotStoppedOrNoLogOpen));
    }
    return;
  }

  content::BrowserThread::PostTaskAndReplyWithResult(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&WebRtcLoggingHandlerHost::GetLogDirectoryAndEnsureExists,
                 this),
      base::Bind(&WebRtcLoggingHandlerHost::TriggerUpload, this, callback));
}

void WebRtcLoggingHandlerHost::UploadStoredLog(
    const std::string& log_id,
    const UploadDoneCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!callback.is_null());

  content::BrowserThread::PostTask(content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&WebRtcLoggingHandlerHost::UploadStoredLogOnFileThread,
                 this, log_id, callback));
}

void WebRtcLoggingHandlerHost::UploadStoredLogOnFileThread(
    const std::string& log_id,
    const UploadDoneCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  WebRtcLogUploadDoneData upload_data;
  upload_data.log_path = GetLogDirectoryAndEnsureExists();
  upload_data.callback = callback;
  upload_data.host = this;
  upload_data.local_log_id = log_id;

  log_uploader_->UploadStoredLog(upload_data);
}

void WebRtcLoggingHandlerHost::UploadLogDone() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  logging_state_ = CLOSED;
}

void WebRtcLoggingHandlerHost::DiscardLog(const GenericDoneCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!callback.is_null());

  if (logging_state_ != STOPPED) {
    FireGenericDoneCallback(callback, false, kLogNotStoppedOrNoLogOpen);
    return;
  }
  log_uploader_->LoggingStoppedDontUpload();
  log_buffer_.reset();
  meta_data_.reset();
  logging_state_ = CLOSED;
  rtp_dump_handler_.reset();
  stop_rtp_dump_callback_.Reset();
  FireGenericDoneCallback(callback, true, "");
}

// Stores the log locally using a hash of log_id + security origin.
void WebRtcLoggingHandlerHost::StoreLog(
    const std::string& log_id,
    const GenericDoneCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!callback.is_null());

  if (logging_state_ != STOPPED) {
    FireGenericDoneCallback(callback, false, kLogNotStoppedOrNoLogOpen);
    return;
  }

  if (rtp_dump_handler_) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(stop_rtp_dump_callback_, true, true));

    rtp_dump_handler_->StopOngoingDumps(
        base::Bind(&WebRtcLoggingHandlerHost::StoreLogContinue,
                   this, log_id, callback));
    return;
  }

  StoreLogContinue(log_id, callback);
}

void WebRtcLoggingHandlerHost::StoreLogContinue(
    const std::string& log_id,
    const GenericDoneCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!callback.is_null());

  std::unique_ptr<WebRtcLogPaths> log_paths(new WebRtcLogPaths());
  ReleaseRtpDumps(log_paths.get());

  content::BrowserThread::PostTaskAndReplyWithResult(
      content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&WebRtcLoggingHandlerHost::GetLogDirectoryAndEnsureExists,
                 this),
      base::Bind(&WebRtcLoggingHandlerHost::StoreLogInDirectory, this, log_id,
                 base::Passed(&log_paths), callback));
}

void WebRtcLoggingHandlerHost::LogMessage(const std::string& message) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (logging_state_ == STARTED) {
    LogToCircularBuffer(WebRtcLoggingMessageData::Format(
        message, base::Time::Now(), logging_started_time_));
  }
}

void WebRtcLoggingHandlerHost::StartRtpDump(
    RtpDumpType type,
    const GenericDoneCallback& callback,
    const content::RenderProcessHost::WebRtcStopRtpDumpCallback&
        stop_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
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

  DoStartRtpDump(type, callback);
}

void WebRtcLoggingHandlerHost::StopRtpDump(
    RtpDumpType type,
    const GenericDoneCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!callback.is_null());

  if (!rtp_dump_handler_) {
    FireGenericDoneCallback(callback, false, "RTP dump has not been started.");
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

void WebRtcLoggingHandlerHost::OnRtpPacket(
    std::unique_ptr<uint8_t[]> packet_header,
    size_t header_length,
    size_t packet_length,
    bool incoming) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

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
    std::unique_ptr<uint8_t[]> packet_header,
    size_t header_length,
    size_t packet_length,
    bool incoming) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // |rtp_dump_handler_| could be NULL if we are waiting for the FILE thread to
  // create/ensure the log directory.
  if (rtp_dump_handler_) {
    rtp_dump_handler_->OnRtpPacket(
        packet_header.get(), header_length, packet_length, incoming);
  }
}

void WebRtcLoggingHandlerHost::OnChannelClosing() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (logging_state_ == STARTED || logging_state_ == STOPPED) {
    if (upload_log_on_render_close_) {
      logging_started_time_ = base::Time();

      content::BrowserThread::PostTaskAndReplyWithResult(
          content::BrowserThread::FILE,
          FROM_HERE,
          base::Bind(&WebRtcLoggingHandlerHost::GetLogDirectoryAndEnsureExists,
                     this),
          base::Bind(&WebRtcLoggingHandlerHost::TriggerUpload, this,
                     UploadDoneCallback()));
    } else {
      log_uploader_->LoggingStoppedDontUpload();
      logging_state_ = CLOSED;
    }
  }
  content::BrowserMessageFilter::OnChannelClosing();
}

void WebRtcLoggingHandlerHost::OnDestruct() const {
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

bool WebRtcLoggingHandlerHost::OnMessageReceived(const IPC::Message& message) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WebRtcLoggingHandlerHost, message)
    IPC_MESSAGE_HANDLER(WebRtcLoggingMsg_AddLogMessages, OnAddLogMessages)
    IPC_MESSAGE_HANDLER(WebRtcLoggingMsg_LoggingStopped,
                        OnLoggingStoppedInRenderer)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void WebRtcLoggingHandlerHost::OnAddLogMessages(
    const std::vector<WebRtcLoggingMessageData>& messages) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (logging_state_ == STARTED || logging_state_ == STOPPING) {
    for (size_t i = 0; i < messages.size(); ++i) {
      LogToCircularBuffer(messages[i].Format(logging_started_time_));
    }
  }
}

void WebRtcLoggingHandlerHost::OnLoggingStoppedInRenderer() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (logging_state_ != STOPPING) {
    // If an out-of-order response is received, stop_callback_ may be invalid,
    // and must not be invoked.
    DLOG(ERROR) << "OnLoggingStoppedInRenderer invoked in state "
                << logging_state_;
    bad_message::ReceivedBadMessage(
        this, bad_message::WRLHH_LOGGING_STOPPED_BAD_STATE);
    return;
  }
  logging_started_time_ = base::Time();
  logging_state_ = STOPPED;
  FireGenericDoneCallback(stop_callback_, true, "");
  stop_callback_.Reset();
}

void WebRtcLoggingHandlerHost::LogInitialInfoOnFileThread(
    const GenericDoneCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  net::NetworkInterfaceList network_list;
  net::GetNetworkList(&network_list,
                      net::EXCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES);

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, base::Bind(
      &WebRtcLoggingHandlerHost::LogInitialInfoOnIOThread, this, network_list,
      callback));
}

void WebRtcLoggingHandlerHost::LogInitialInfoOnIOThread(
    const net::NetworkInterfaceList& network_list,
    const GenericDoneCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (logging_state_ != STARTING) {
    FireGenericDoneCallback(callback, false, "Logging cancelled.");
    return;
  }

  // Tell the renderer and the browser to enable logging. Log messages are
  // recevied on the IO thread, so the initial info will finish to be written
  // first.
  Send(new WebRtcLoggingMsg_StartLogging());
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &WebRtcLoggingHandlerHost::EnableBrowserProcessLoggingOnUIThread,
          this));

  // Log start time (current time). We don't use base/i18n/time_formatting.h
  // here because we don't want the format of the current locale.
  base::Time::Exploded now = {0};
  base::Time::Now().LocalExplode(&now);
  LogToCircularBuffer(base::StringPrintf(
      "Start %d-%02d-%02d %02d:%02d:%02d", now.year, now.month,
      now.day_of_month, now.hour, now.minute, now.second));

  // Write metadata if received before logging started.
  if (meta_data_.get() && !meta_data_->empty()) {
    std::string info;
    FormatMetaDataAsLogMessage(*meta_data_.get(), &info);
    LogToCircularBuffer(info);
  }

  // Chrome version
  LogToCircularBuffer("Chrome version: " + version_info::GetVersionNumber() +
                      " " + chrome::GetChannelString());

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
      ", vendor-id=" + base::UintToString(gpu_info.gpu.vendor_id) +
      ", device-id=" + base::UintToString(gpu_info.gpu.device_id) +
      ", driver-vendor=" + gpu_info.driver_vendor +
      ", driver-version=" + gpu_info.driver_version);
  LogToCircularBuffer(
      "OpenGL: gl-vendor=" + gpu_info.gl_vendor +
      ", gl-renderer=" + gpu_info.gl_renderer +
      ", gl-version=" + gpu_info.gl_version);

  // Network interfaces
  LogToCircularBuffer("Discovered " + base::SizeTToString(network_list.size()) +
                      " network interfaces:");
  for (net::NetworkInterfaceList::const_iterator it = network_list.begin();
       it != network_list.end(); ++it) {
    LogToCircularBuffer(
        "Name: " + it->friendly_name + ", Address: " +
        IPAddressToSensitiveString(it->address) + ", Type: " +
        net::NetworkChangeNotifier::ConnectionTypeToString(it->type));
  }

  logging_started_time_ = base::Time::Now();
  logging_state_ = STARTED;
  FireGenericDoneCallback(callback, true, "");
}

void WebRtcLoggingHandlerHost::EnableBrowserProcessLoggingOnUIThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::RenderProcessHost* host =
      content::RenderProcessHost::FromID(render_process_id_);
  if (host) {
    host->SetWebRtcLogMessageCallback(
        base::Bind(&WebRtcLoggingHandlerHost::LogMessage, this));
  }
}

void WebRtcLoggingHandlerHost::DisableBrowserProcessLoggingOnUIThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::RenderProcessHost* host =
      content::RenderProcessHost::FromID(render_process_id_);
  if (host)
    host->ClearWebRtcLogMessageCallback();
}

void WebRtcLoggingHandlerHost::LogToCircularBuffer(const std::string& message) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_NE(logging_state_, CLOSED);
  log_buffer_->Log(message);
}

base::FilePath WebRtcLoggingHandlerHost::GetLogDirectoryAndEnsureExists() {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
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
    const UploadDoneCallback& callback,
    const base::FilePath& log_directory) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (rtp_dump_handler_) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(stop_rtp_dump_callback_, true, true));

    rtp_dump_handler_->StopOngoingDumps(
        base::Bind(&WebRtcLoggingHandlerHost::DoUploadLogAndRtpDumps,
                   this,
                   log_directory,
                   callback));
    return;
  }

  DoUploadLogAndRtpDumps(log_directory, callback);
}

void WebRtcLoggingHandlerHost::StoreLogInDirectory(
    const std::string& log_id,
    std::unique_ptr<WebRtcLogPaths> log_paths,
    const GenericDoneCallback& done_callback,
    const base::FilePath& directory) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  log_paths->log_path = directory;

  log_buffer_->SetComplete();
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&WebRtcLogUploader::LoggingStoppedDoStore,
                 base::Unretained(log_uploader_),
                 *log_paths.get(), log_id, base::Passed(&log_buffer_),
                 base::Passed(&meta_data_), done_callback));

  logging_state_ = CLOSED;
}

void WebRtcLoggingHandlerHost::DoUploadLogAndRtpDumps(
    const base::FilePath& log_directory,
    const UploadDoneCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  WebRtcLogUploadDoneData upload_done_data;
  upload_done_data.log_path = log_directory;
  upload_done_data.callback = callback;
  upload_done_data.host = this;
  ReleaseRtpDumps(&upload_done_data);

  log_buffer_->SetComplete();
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&WebRtcLogUploader::LoggingStoppedDoUpload,
                 base::Unretained(log_uploader_), base::Passed(&log_buffer_),
                 base::Passed(&meta_data_), upload_done_data));

  logging_state_ = CLOSED;
}

void WebRtcLoggingHandlerHost::CreateRtpDumpHandlerAndStart(
    RtpDumpType type,
    const GenericDoneCallback& callback,
    const base::FilePath& dump_dir) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // |rtp_dump_handler_| may be non-NULL if StartRtpDump is called again before
  // GetLogDirectoryAndEnsureExists returns on the FILE thread for a previous
  // StartRtpDump.
  if (!rtp_dump_handler_)
    rtp_dump_handler_.reset(new WebRtcRtpDumpHandler(dump_dir));

  DoStartRtpDump(type, callback);
}

void WebRtcLoggingHandlerHost::DoStartRtpDump(
    RtpDumpType type, const GenericDoneCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(rtp_dump_handler_);

  std::string error;
  bool result = rtp_dump_handler_->StartDump(type, &error);
  FireGenericDoneCallback(callback, result, error);
}

bool WebRtcLoggingHandlerHost::ReleaseRtpDumps(WebRtcLogPaths* log_paths) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(log_paths);

  if (!rtp_dump_handler_)
    return false;

  WebRtcRtpDumpHandler::ReleasedDumps rtp_dumps(
      rtp_dump_handler_->ReleaseDumps());
  log_paths->incoming_rtp_dump = rtp_dumps.incoming_dump_path;
  log_paths->outgoing_rtp_dump = rtp_dumps.outgoing_dump_path;

  rtp_dump_handler_.reset();
  stop_rtp_dump_callback_.Reset();

  return true;
}

void WebRtcLoggingHandlerHost::FireGenericDoneCallback(
    const WebRtcLoggingHandlerHost::GenericDoneCallback& callback,
    bool success,
    const std::string& error_message) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!callback.is_null());

  if (error_message.empty()) {
    DCHECK(success);
    content::BrowserThread::PostTask(
        content::BrowserThread::UI,
        FROM_HERE,
        base::Bind(callback, success, error_message));
    return;
  }

  DCHECK(!success);

  // Add current logging state to error message.
  std::string error_message_with_state(error_message);
  switch (logging_state_) {
  case CLOSED:
    error_message_with_state += " State=closed.";
    break;
  case STARTING:
    error_message_with_state += " State=starting.";
    break;
  case STARTED:
    error_message_with_state += " State=started.";
    break;
  case STOPPING:
    error_message_with_state += " State=stopping.";
    break;
  case STOPPED:
    error_message_with_state += " State=stopped.";
    break;
  }

  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(callback, success, error_message_with_state));
}
