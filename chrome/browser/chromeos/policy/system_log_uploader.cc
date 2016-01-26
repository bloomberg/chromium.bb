// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "system_log_uploader.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/upload_job_impl.h"
#include "chrome/browser/chromeos/settings/device_oauth2_token_service.h"
#include "chrome/browser/chromeos/settings/device_oauth2_token_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "components/feedback/anonymizer_tool.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "content/public/browser/browser_thread.h"
#include "net/http/http_request_headers.h"

namespace {
// The maximum number of successive retries.
const int kMaxNumRetries = 1;

// String constant defining the url tail we upload system logs to.
const char* kSystemLogUploadUrlTail = "/upload";

// The file names of the system logs to upload.
// Note: do not add anything to this list without checking for PII in the file.
const char* const kSystemLogFileNames[] = {
    "/var/log/bios_info.txt", "/var/log/chrome/chrome",
    "/var/log/eventlog.txt",  "/var/log/platform_info.txt",
    "/var/log/messages",      "/var/log/messages.1",
    "/var/log/net.log",       "/var/log/net.1.log",
    "/var/log/ui/ui.LATEST",  "/var/log/update_engine.log"};

// Reads the system log files as binary files, anonymizes data, stores the files
// as pairs (file name, data) and returns. Called on blocking thread.
scoped_ptr<policy::SystemLogUploader::SystemLogs> ReadFiles() {
  scoped_ptr<policy::SystemLogUploader::SystemLogs> system_logs(
      new policy::SystemLogUploader::SystemLogs());
  feedback::AnonymizerTool anonymizer;
  for (auto const file_path : kSystemLogFileNames) {
    if (!base::PathExists(base::FilePath(file_path)))
      continue;
    std::string data = std::string();
    if (!base::ReadFileToString(base::FilePath(file_path), &data)) {
      LOG(ERROR) << "Failed to read the system log file from the disk "
                 << file_path << std::endl;
    }
    system_logs->push_back(std::make_pair(
        file_path,
        policy::SystemLogUploader::RemoveSensitiveData(&anonymizer, data)));
  }
  return system_logs;
}

// An implementation of the |SystemLogUploader::Delegate|, that is used to
// create an upload job and load system logs from the disk.
class SystemLogDelegate : public policy::SystemLogUploader::Delegate {
 public:
  SystemLogDelegate();
  ~SystemLogDelegate() override;

  // SystemLogUploader::Delegate:
  void LoadSystemLogs(const LogUploadCallback& upload_callback) override;

  scoped_ptr<policy::UploadJob> CreateUploadJob(
      const GURL& upload_url,
      policy::UploadJob::Delegate* delegate) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemLogDelegate);
};

SystemLogDelegate::SystemLogDelegate() {}

SystemLogDelegate::~SystemLogDelegate() {}

void SystemLogDelegate::LoadSystemLogs(
    const LogUploadCallback& upload_callback) {
  // Run ReadFiles() in the thread that interacts with the file system and
  // return system logs to |upload_callback| on the current thread.
  base::PostTaskAndReplyWithResult(content::BrowserThread::GetBlockingPool(),
                                   FROM_HERE, base::Bind(&ReadFiles),
                                   upload_callback);
}

scoped_ptr<policy::UploadJob> SystemLogDelegate::CreateUploadJob(
    const GURL& upload_url,
    policy::UploadJob::Delegate* delegate) {
  chromeos::DeviceOAuth2TokenService* device_oauth2_token_service =
      chromeos::DeviceOAuth2TokenServiceFactory::Get();

  scoped_refptr<net::URLRequestContextGetter> system_request_context =
      g_browser_process->system_request_context();
  std::string robot_account_id =
      device_oauth2_token_service->GetRobotAccountId();
  return scoped_ptr<policy::UploadJob>(new policy::UploadJobImpl(
      upload_url, robot_account_id, device_oauth2_token_service,
      system_request_context, delegate,
      make_scoped_ptr(new policy::UploadJobImpl::RandomMimeBoundaryGenerator)));
}

// Returns the system log upload frequency.
base::TimeDelta GetUploadFrequency() {
  base::TimeDelta upload_frequency(base::TimeDelta::FromMilliseconds(
      policy::SystemLogUploader::kDefaultUploadDelayMs));
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSystemLogUploadFrequency)) {
    std::string string_value =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kSystemLogUploadFrequency);
    int frequency;
    if (base::StringToInt(string_value, &frequency)) {
      upload_frequency = base::TimeDelta::FromMilliseconds(frequency);
    }
  }
  return upload_frequency;
}

std::string GetUploadUrl() {
  return policy::BrowserPolicyConnector::GetDeviceManagementUrl() +
         kSystemLogUploadUrlTail;
}

}  // namespace

namespace policy {

// Determines the time between log uploads.
const int64_t SystemLogUploader::kDefaultUploadDelayMs =
    12 * 60 * 60 * 1000;  // 12 hours

// Determines the time, measured from the time of last failed upload,
// after which the log upload is retried.
const int64_t SystemLogUploader::kErrorUploadDelayMs =
    120 * 1000;  // 120 seconds

// String constant identifying the header field which stores the file type.
const char* const SystemLogUploader::kFileTypeHeaderName = "File-Type";

// String constant signalling that the data segment contains log files.
const char* const SystemLogUploader::kFileTypeLogFile = "log_file";

// String constant signalling that the segment contains a plain text.
const char* const SystemLogUploader::kContentTypePlainText = "text/plain";

// Template string constant for populating the name field.
const char* const SystemLogUploader::kNameFieldTemplate = "file%d";

SystemLogUploader::SystemLogUploader(
    scoped_ptr<Delegate> syslog_delegate,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : retry_count_(0),
      upload_frequency_(GetUploadFrequency()),
      task_runner_(task_runner),
      syslog_delegate_(std::move(syslog_delegate)),
      upload_enabled_(false),
      weak_factory_(this) {
  if (!syslog_delegate_)
    syslog_delegate_.reset(new SystemLogDelegate());
  DCHECK(syslog_delegate_);

  // Watch for policy changes.
  upload_enabled_observer_ = chromeos::CrosSettings::Get()->AddSettingsObserver(
      chromeos::kSystemLogUploadEnabled,
      base::Bind(&SystemLogUploader::RefreshUploadSettings,
                 base::Unretained(this)));

  // Fetch the current value of the policy.
  RefreshUploadSettings();

  // Immediately schedule the next system log upload (last_upload_attempt_ is
  // set to the start of the epoch, so this will trigger an update upload in the
  // immediate future).
  ScheduleNextSystemLogUpload(upload_frequency_);
}

SystemLogUploader::~SystemLogUploader() {}

void SystemLogUploader::OnSuccess() {
  upload_job_.reset();
  last_upload_attempt_ = base::Time::NowFromSystemTime();
  retry_count_ = 0;

  // On successful log upload schedule the next log upload after
  // upload_frequency_ time from now.
  ScheduleNextSystemLogUpload(upload_frequency_);
}

void SystemLogUploader::OnFailure(UploadJob::ErrorCode error_code) {
  upload_job_.reset();
  last_upload_attempt_ = base::Time::NowFromSystemTime();

  //  If we have hit the maximum number of retries, terminate this upload
  //  attempt and schedule the next one using the normal delay. Otherwise, retry
  //  uploading after kErrorUploadDelayMs milliseconds.
  if (retry_count_++ < kMaxNumRetries) {
    ScheduleNextSystemLogUpload(
        base::TimeDelta::FromMilliseconds(kErrorUploadDelayMs));
  } else {
    // No more retries.
    retry_count_ = 0;
    ScheduleNextSystemLogUpload(upload_frequency_);
  }
}

// static
std::string SystemLogUploader::RemoveSensitiveData(
    feedback::AnonymizerTool* const anonymizer,
    const std::string& data) {
  return anonymizer->Anonymize(data);
}
void SystemLogUploader::RefreshUploadSettings() {
  // Attempt to fetch the current value of the reporting settings.
  // If trusted values are not available, register this function to be called
  // back when they are available.
  chromeos::CrosSettings* settings = chromeos::CrosSettings::Get();
  if (chromeos::CrosSettingsProvider::TRUSTED !=
      settings->PrepareTrustedValues(
          base::Bind(&SystemLogUploader::RefreshUploadSettings,
                     weak_factory_.GetWeakPtr()))) {
    return;
  }

  // CrosSettings are trusted - we want to use the last trusted values, by
  // default do not upload system logs.
  if (!settings->GetBoolean(chromeos::kSystemLogUploadEnabled,
                            &upload_enabled_))
    upload_enabled_ = false;
}

void SystemLogUploader::UploadSystemLogs(scoped_ptr<SystemLogs> system_logs) {
  // Must be called on the main thread.
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!upload_job_);

  GURL upload_url(GetUploadUrl());
  DCHECK(upload_url.is_valid());
  upload_job_ = syslog_delegate_->CreateUploadJob(upload_url, this);

  // Start a system log upload.
  int file_number = 1;
  for (const auto& syslog_entry : *system_logs) {
    std::map<std::string, std::string> header_fields;
    scoped_ptr<std::string> data =
        make_scoped_ptr(new std::string(syslog_entry.second));
    header_fields.insert(std::make_pair(kFileTypeHeaderName, kFileTypeLogFile));
    header_fields.insert(std::make_pair(net::HttpRequestHeaders::kContentType,
                                        kContentTypePlainText));
    upload_job_->AddDataSegment(
        base::StringPrintf(kNameFieldTemplate, file_number), syslog_entry.first,
        header_fields, std::move(data));
    ++file_number;
  }
  upload_job_->Start();
}

void SystemLogUploader::StartLogUpload() {
  // Must be called on the main thread.
  DCHECK(thread_checker_.CalledOnValidThread());

  if (upload_enabled_) {
    syslog_delegate_->LoadSystemLogs(base::Bind(
        &SystemLogUploader::UploadSystemLogs, weak_factory_.GetWeakPtr()));
  } else {
    // If upload is disabled, schedule the next attempt after 12h.
    retry_count_ = 0;
    last_upload_attempt_ = base::Time::NowFromSystemTime();
    ScheduleNextSystemLogUpload(upload_frequency_);
  }
}

void SystemLogUploader::ScheduleNextSystemLogUpload(base::TimeDelta frequency) {
  // Calculate when to fire off the next update.
  base::TimeDelta delay = std::max(
      (last_upload_attempt_ + frequency) - base::Time::NowFromSystemTime(),
      base::TimeDelta());
  // Ensure that we never have more than one pending delayed task.
  weak_factory_.InvalidateWeakPtrs();
  task_runner_->PostDelayedTask(FROM_HERE,
                                base::Bind(&SystemLogUploader::StartLogUpload,
                                           weak_factory_.GetWeakPtr()),
                                delay);
}

}  // namespace policy
