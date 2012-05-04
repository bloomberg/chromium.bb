// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/cloud_print/printer_job_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/md5.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/cloud_print/cloud_print_helpers.h"
#include "chrome/service/cloud_print/cloud_print_consts.h"
#include "chrome/service/cloud_print/cloud_print_helpers.h"
#include "chrome/service/cloud_print/job_status_updater.h"
#include "googleurl/src/gurl.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"

PrinterJobHandler::JobDetails::JobDetails() {}

PrinterJobHandler::JobDetails::~JobDetails() {}

void PrinterJobHandler::JobDetails::Clear() {
  job_id_.clear();
  job_title_.clear();
  print_ticket_.clear();
  print_data_mime_type_.clear();
  print_data_file_path_ = FilePath();
  tags_.clear();
}

PrinterJobHandler::PrinterJobHandler(
    const printing::PrinterBasicInfo& printer_info,
    const PrinterInfoFromCloud& printer_info_cloud,
    const GURL& cloud_print_server_url,
    cloud_print::PrintSystem* print_system,
    Delegate* delegate)
    : print_system_(print_system),
      printer_info_(printer_info),
      printer_info_cloud_(printer_info_cloud),
      cloud_print_server_url_(cloud_print_server_url),
      delegate_(delegate),
      local_job_id_(-1),
      next_json_data_handler_(NULL),
      next_data_handler_(NULL),
      server_error_count_(0),
      print_thread_("Chrome_CloudPrintJobPrintThread"),
      job_handler_message_loop_proxy_(
          base::MessageLoopProxy::current()),
      shutting_down_(false),
      job_check_pending_(false),
      printer_update_pending_(true),
      task_in_progress_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
}

bool PrinterJobHandler::Initialize() {
  if (!print_system_->IsValidPrinter(printer_info_.printer_name))
    return false;

  printer_watcher_ = print_system_->CreatePrinterWatcher(
      printer_info_.printer_name);
  printer_watcher_->StartWatching(this);
  CheckForJobs(kJobFetchReasonStartup);
  return true;
}

std::string PrinterJobHandler::GetPrinterName() const {
  return printer_info_.printer_name;
}

void PrinterJobHandler::CheckForJobs(const std::string& reason) {
  VLOG(1) << "CP_CONNECTOR: CheckForJobs, id: "
          << printer_info_cloud_.printer_id
          << ", reason: " << reason
          << ", task in progress: " << task_in_progress_;
  job_fetch_reason_ = reason;
  job_check_pending_ = true;
  if (!task_in_progress_) {
    MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(&PrinterJobHandler::Start, this));
  }
}

void PrinterJobHandler::Shutdown() {
  VLOG(1) << "CP_CONNECTOR: Printer job handler shutdown, id: "
          << printer_info_cloud_.printer_id;
  Reset();
  shutting_down_ = true;
  while (!job_status_updater_list_.empty()) {
    // Calling Stop() will cause the OnJobCompleted to be called which will
    // remove the updater object from the list.
    job_status_updater_list_.front()->Stop();
  }
}

// CloudPrintURLFetcher::Delegate implementation.
CloudPrintURLFetcher::ResponseAction PrinterJobHandler::HandleRawResponse(
    const content::URLFetcher* source,
    const GURL& url,
    const net::URLRequestStatus& status,
    int response_code,
    const net::ResponseCookies& cookies,
    const std::string& data) {
  // 415 (Unsupported media type) error while fetching data from the server
  // means data conversion error. Stop fetching process and mark job as error.
  if (next_data_handler_ == (&PrinterJobHandler::HandlePrintDataResponse) &&
      response_code == net::HTTP_UNSUPPORTED_MEDIA_TYPE) {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&PrinterJobHandler::JobFailed, this, JOB_DOWNLOAD_FAILED));
    return CloudPrintURLFetcher::STOP_PROCESSING;
  }
  return CloudPrintURLFetcher::CONTINUE_PROCESSING;
}

CloudPrintURLFetcher::ResponseAction PrinterJobHandler::HandleRawData(
    const content::URLFetcher* source,
    const GURL& url,
    const std::string& data) {
  if (!next_data_handler_)
    return CloudPrintURLFetcher::CONTINUE_PROCESSING;
  return (this->*next_data_handler_)(source, url, data);
}

CloudPrintURLFetcher::ResponseAction PrinterJobHandler::HandleJSONData(
    const content::URLFetcher* source,
    const GURL& url,
    DictionaryValue* json_data,
    bool succeeded) {
  DCHECK(next_json_data_handler_);
  return (this->*next_json_data_handler_)(source, url, json_data, succeeded);
}

void PrinterJobHandler::OnRequestGiveUp() {
  // The only time we call CloudPrintURLFetcher::StartGetRequest() with a
  // specified number of retries, is when we are trying to fetch print job
  // data. So, this function will be reached only if we failed to get job data.
  // In that case, we should make job as error and should not try it later.
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&PrinterJobHandler::JobFailed, this, JOB_DOWNLOAD_FAILED));
}

CloudPrintURLFetcher::ResponseAction PrinterJobHandler::OnRequestAuthError() {
  // We got an Auth error and have no idea how long it will take to refresh
  // auth information (may take forever). We'll drop current request and
  // propagate this error to the upper level. After auth issues will be
  // resolved, GCP connector will restart.
  OnAuthError();
  return CloudPrintURLFetcher::STOP_PROCESSING;
}

std::string PrinterJobHandler::GetAuthHeader() {
  return CloudPrintHelpers::GetCloudPrintAuthHeaderFromStore();
}

// JobStatusUpdater::Delegate implementation
bool PrinterJobHandler::OnJobCompleted(JobStatusUpdater* updater) {
  bool ret = false;
  for (JobStatusUpdaterList::iterator index = job_status_updater_list_.begin();
       index != job_status_updater_list_.end(); index++) {
    if (index->get() == updater) {
      job_status_updater_list_.erase(index);
      ret = true;
      break;
    }
  }
  return ret;
}

void PrinterJobHandler::OnAuthError() {
  if (delegate_)
    delegate_->OnAuthError();
}

void PrinterJobHandler::OnPrinterDeleted() {
  if (delegate_)
    delegate_->OnPrinterDeleted(printer_info_cloud_.printer_id);
}

void PrinterJobHandler::OnPrinterChanged() {
  printer_update_pending_ = true;
  if (!task_in_progress_) {
    MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(&PrinterJobHandler::Start, this));
  }
}

void PrinterJobHandler::OnJobChanged() {
  // Some job on the printer changed. Loop through all our JobStatusUpdaters
  // and have them check for updates.
  for (JobStatusUpdaterList::iterator index = job_status_updater_list_.begin();
       index != job_status_updater_list_.end(); index++) {
    MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(&JobStatusUpdater::UpdateStatus, index->get()));
  }
}

void PrinterJobHandler::OnJobSpoolSucceeded(
    const cloud_print::PlatformJobId& job_id) {
  DCHECK(MessageLoop::current() == print_thread_.message_loop());
  job_spooler_ = NULL;
  job_handler_message_loop_proxy_->PostTask(
      FROM_HERE, base::Bind(&PrinterJobHandler::JobSpooled, this, job_id));
}

void PrinterJobHandler::OnJobSpoolFailed() {
  DCHECK(MessageLoop::current() == print_thread_.message_loop());
  job_spooler_ = NULL;
  job_handler_message_loop_proxy_->PostTask(
      FROM_HERE, base::Bind(&PrinterJobHandler::JobFailed, this, PRINT_FAILED));
}

PrinterJobHandler::~PrinterJobHandler() {
  if (printer_watcher_)
    printer_watcher_->StopWatching();
}

// Begin Response handlers
CloudPrintURLFetcher::ResponseAction
PrinterJobHandler::HandlePrinterUpdateResponse(
    const content::URLFetcher* source,
    const GURL& url,
    DictionaryValue* json_data,
    bool succeeded) {
  VLOG(1) << "CP_CONNECTOR: Handle printer update response, id: "
          << printer_info_cloud_.printer_id;
  // We are done here. Go to the Stop state
  MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&PrinterJobHandler::Stop, this));
  return CloudPrintURLFetcher::STOP_PROCESSING;
}

CloudPrintURLFetcher::ResponseAction
PrinterJobHandler::HandleJobMetadataResponse(
    const content::URLFetcher* source,
    const GURL& url,
    DictionaryValue* json_data,
    bool succeeded) {
  VLOG(1) << "CP_CONNECTOR: Handle job metadata response, id: "
          << printer_info_cloud_.printer_id;
  bool job_available = false;
  if (succeeded) {
    ListValue* job_list = NULL;
    if (json_data->GetList(kJobListValue, &job_list) && job_list) {
      // Even though it is a job list, for now we are only interested in the
      // first job
      DictionaryValue* job_data = NULL;
      if (job_list->GetDictionary(0, &job_data)) {
        job_available = true;
        job_data->GetString(kIdValue, &job_details_.job_id_);
        job_data->GetString(kTitleValue, &job_details_.job_title_);
        std::string print_ticket_url;
        job_data->GetString(kTicketUrlValue, &print_ticket_url);
        job_data->GetString(kFileUrlValue, &print_data_url_);

        // Get tags for print job.
        job_details_.tags_.clear();
        ListValue* tags = NULL;
        if (job_data->GetList(kTagsValue, &tags)) {
          for (size_t i = 0; i < tags->GetSize(); i++) {
            std::string value;
            if (tags->GetString(i, &value))
              job_details_.tags_.push_back(value);
          }
        }
        SetNextDataHandler(&PrinterJobHandler::HandlePrintTicketResponse);
        request_ = new CloudPrintURLFetcher;
        request_->StartGetRequest(GURL(print_ticket_url.c_str()),
                                  this,
                                  kCloudPrintAPIMaxRetryCount,
                                  std::string());
      }
    }
  }
  // If no jobs are available, go to the Stop state.
  if (!job_available)
    MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(&PrinterJobHandler::Stop, this));
  return CloudPrintURLFetcher::STOP_PROCESSING;
}

CloudPrintURLFetcher::ResponseAction
PrinterJobHandler::HandlePrintTicketResponse(const content::URLFetcher* source,
                                             const GURL& url,
                                             const std::string& data) {
  VLOG(1) << "CP_CONNECTOR: Handle print ticket response, id: "
          << printer_info_cloud_.printer_id;
  if (print_system_->ValidatePrintTicket(printer_info_.printer_name, data)) {
    job_details_.print_ticket_ = data;
    SetNextDataHandler(&PrinterJobHandler::HandlePrintDataResponse);
    request_ = new CloudPrintURLFetcher;
    std::string accept_headers = "Accept: ";
    accept_headers += print_system_->GetSupportedMimeTypes();
    request_->StartGetRequest(GURL(print_data_url_.c_str()),
                              this,
                              kJobDataMaxRetryCount,
                              accept_headers);
  } else {
    // The print ticket was not valid. We are done here.
    FailedFetchingJobData();
  }
  return CloudPrintURLFetcher::STOP_PROCESSING;
}

CloudPrintURLFetcher::ResponseAction
PrinterJobHandler::HandlePrintDataResponse(const content::URLFetcher* source,
                                           const GURL& url,
                                           const std::string& data) {
  VLOG(1) << "CP_CONNECTOR: Handle print data response, id: "
          << printer_info_cloud_.printer_id;
  base::Closure next_task;
  if (file_util::CreateTemporaryFile(&job_details_.print_data_file_path_)) {
    int ret = file_util::WriteFile(job_details_.print_data_file_path_,
                                   data.c_str(),
                                   data.length());
    source->GetResponseHeaders()->GetMimeType(
        &job_details_.print_data_mime_type_);
    DCHECK(ret == static_cast<int>(data.length()));
    if (ret == static_cast<int>(data.length())) {
      next_task = base::Bind(&PrinterJobHandler::StartPrinting, this);
    }
  }
  // If there was no task allocated above, then there was an error in
  // saving the print data, bail out here.
  if (next_task.is_null()) {
    next_task = base::Bind(&PrinterJobHandler::JobFailed, this,
                           JOB_DOWNLOAD_FAILED);
  }
  MessageLoop::current()->PostTask(FROM_HERE, next_task);
  return CloudPrintURLFetcher::STOP_PROCESSING;
}

CloudPrintURLFetcher::ResponseAction
PrinterJobHandler::HandleSuccessStatusUpdateResponse(
    const content::URLFetcher* source,
    const GURL& url,
    DictionaryValue* json_data,
    bool succeeded) {
  VLOG(1) << "CP_CONNECTOR: Handle success status update response, id: "
          << printer_info_cloud_.printer_id;
  // The print job has been spooled locally. We now need to create an object
  // that monitors the status of the job and updates the server.
  scoped_refptr<JobStatusUpdater> job_status_updater(
      new JobStatusUpdater(printer_info_.printer_name, job_details_.job_id_,
                           local_job_id_, cloud_print_server_url_,
                           print_system_.get(), this));
  job_status_updater_list_.push_back(job_status_updater);
  MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&JobStatusUpdater::UpdateStatus,
                            job_status_updater.get()));
  if (succeeded) {
    // Since we just printed successfully, we want to look for more jobs.
    CheckForJobs(kJobFetchReasonQueryMore);
  }
  MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&PrinterJobHandler::Stop, this));
  return CloudPrintURLFetcher::STOP_PROCESSING;
}

CloudPrintURLFetcher::ResponseAction
PrinterJobHandler::HandleFailureStatusUpdateResponse(
    const content::URLFetcher* source,
    const GURL& url,
    DictionaryValue* json_data,
    bool succeeded) {
  VLOG(1) << "CP_CONNECTOR: Handle failure status update response, id: "
          << printer_info_cloud_.printer_id;
  MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&PrinterJobHandler::Stop, this));
  return CloudPrintURLFetcher::STOP_PROCESSING;
}

void PrinterJobHandler::Start() {
  VLOG(1) << "CP_CONNECTOR: Start printer job handler, id: "
          << printer_info_cloud_.printer_id
          << ", task in progress: " << task_in_progress_;
  if (task_in_progress_) {
    // Multiple Starts can get posted because of multiple notifications
    // We want to ignore the other ones that happen when a task is in progress.
    return;
  }
  Reset();
  if (!shutting_down_) {
    // Check if we have work to do.
    if (HavePendingTasks()) {
      if (!task_in_progress_ && printer_update_pending_) {
        printer_update_pending_ = false;
        task_in_progress_ = UpdatePrinterInfo();
      }
      if (!task_in_progress_ && job_check_pending_) {
        task_in_progress_ = true;
        job_check_pending_ = false;
        // We need to fetch any pending jobs for this printer
        SetNextJSONHandler(&PrinterJobHandler::HandleJobMetadataResponse);
        request_ = new CloudPrintURLFetcher;
        request_->StartGetRequest(
            CloudPrintHelpers::GetUrlForJobFetch(
                cloud_print_server_url_, printer_info_cloud_.printer_id,
                job_fetch_reason_),
            this,
            kCloudPrintAPIMaxRetryCount,
            std::string());
        last_job_fetch_time_ = base::TimeTicks::Now();
        VLOG(1) << "Last job fetch time for printer "
                << printer_info_.printer_name.c_str() << " is "
                << last_job_fetch_time_.ToInternalValue();
        job_fetch_reason_.clear();
      }
    }
  }
}

void PrinterJobHandler::Stop() {
  VLOG(1) << "CP_CONNECTOR: Stop printer job handler, id: "
          << printer_info_cloud_.printer_id;
  task_in_progress_ = false;
  Reset();
  if (HavePendingTasks()) {
    MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(&PrinterJobHandler::Start, this));
  }
}

void PrinterJobHandler::StartPrinting() {
  VLOG(1) << "CP_CONNECTOR: Start printing, id: "
          << printer_info_cloud_.printer_id;
  // We are done with the request object for now.
  request_ = NULL;
  if (!shutting_down_) {
    if (!print_thread_.Start()) {
      JobFailed(PRINT_FAILED);
    } else {
      print_thread_.message_loop()->PostTask(
          FROM_HERE, base::Bind(&PrinterJobHandler::DoPrint, this, job_details_,
                                printer_info_.printer_name));
    }
  }
}

void PrinterJobHandler::Reset() {
  print_data_url_.clear();
  job_details_.Clear();
  request_ = NULL;
  print_thread_.Stop();
}

void PrinterJobHandler::UpdateJobStatus(cloud_print::PrintJobStatus status,
                                        PrintJobError error) {
  VLOG(1) << "CP_CONNECTOR: Update job status, id: "
          << printer_info_cloud_.printer_id;
  if (!shutting_down_) {
    if (!job_details_.job_id_.empty()) {
      VLOG(1) << "CP_CONNECTOR: Updating status, job id: "
              << job_details_.job_id_ << ", status: " << status;
      if (error == SUCCESS) {
        SetNextJSONHandler(
            &PrinterJobHandler::HandleSuccessStatusUpdateResponse);
      } else {
        SetNextJSONHandler(
            &PrinterJobHandler::HandleFailureStatusUpdateResponse);
      }
      request_ = new CloudPrintURLFetcher;
      request_->StartGetRequest(
          CloudPrintHelpers::GetUrlForJobStatusUpdate(cloud_print_server_url_,
                                                      job_details_.job_id_,
                                                      status),
          this,
          kCloudPrintAPIMaxRetryCount,
          std::string());
    }
  }
}

void PrinterJobHandler::SetNextJSONHandler(JSONDataHandler handler) {
  next_json_data_handler_ = handler;
  next_data_handler_ = NULL;
}

void PrinterJobHandler::SetNextDataHandler(DataHandler handler) {
  next_data_handler_ = handler;
  next_json_data_handler_ = NULL;
}

void PrinterJobHandler::JobFailed(PrintJobError error) {
  VLOG(1) << "CP_CONNECTOR: Job failed, id: " << printer_info_cloud_.printer_id;
  if (!shutting_down_) {
    UpdateJobStatus(cloud_print::PRINT_JOB_STATUS_ERROR, error);
  }
}

void PrinterJobHandler::JobSpooled(cloud_print::PlatformJobId local_job_id) {
  VLOG(1) << "CP_CONNECTOR: Job spooled, printer id: "
          << printer_info_cloud_.printer_id << ", job id: " << local_job_id;
  if (!shutting_down_) {
    local_job_id_ = local_job_id;
    UpdateJobStatus(cloud_print::PRINT_JOB_STATUS_IN_PROGRESS, SUCCESS);
    print_thread_.Stop();
  }
}

bool PrinterJobHandler::UpdatePrinterInfo() {
  if (!printer_watcher_) {
    LOG(ERROR) << "CP_CONNECTOR: Printer watcher is missing."
               << "Check printer server url for printer id: "
               << printer_info_cloud_.printer_id;
    return false;
  }

  VLOG(1) << "CP_CONNECTOR: Update printer info, id: "
          << printer_info_cloud_.printer_id;
  // We need to update the parts of the printer info that have changed
  // (could be printer name, description, status or capabilities).
  // First asynchronously fetch the capabilities.
  printing::PrinterBasicInfo printer_info;
  printer_watcher_->GetCurrentPrinterInfo(&printer_info);

  // Asynchronously fetch the printer caps and defaults. The story will
  // continue in OnReceivePrinterCaps.
  print_system_->GetPrinterCapsAndDefaults(
      printer_info.printer_name.c_str(),
      base::Bind(&PrinterJobHandler::OnReceivePrinterCaps,
                 weak_ptr_factory_.GetWeakPtr()));

  // While we are waiting for the data, pretend we have work to do and return
  // true.
  return true;
}

bool PrinterJobHandler::HavePendingTasks() {
  return (job_check_pending_ || printer_update_pending_);
}

void PrinterJobHandler::FailedFetchingJobData() {
  if (!shutting_down_) {
    LOG(ERROR) << "CP_CONNECTOR: Failed fetching job data for printer: " <<
        printer_info_.printer_name << ", job id: " << job_details_.job_id_;
    JobFailed(INVALID_JOB_DATA);
  }
}

void PrinterJobHandler::OnReceivePrinterCaps(
    bool succeeded,
    const std::string& printer_name,
    const printing::PrinterCapsAndDefaults& caps_and_defaults) {
  printing::PrinterBasicInfo printer_info;
  if (printer_watcher_)
    printer_watcher_->GetCurrentPrinterInfo(&printer_info);

  std::string post_data;
  std::string mime_boundary;
  cloud_print::CreateMimeBoundaryForUpload(&mime_boundary);

  if (succeeded) {
    std::string caps_hash =
        base::MD5String(caps_and_defaults.printer_capabilities);
    if (caps_hash != printer_info_cloud_.caps_hash) {
      // Hashes don't match, we need to upload new capabilities (the defaults
      // go for free along with the capabilities)
      printer_info_cloud_.caps_hash = caps_hash;
      cloud_print::AddMultipartValueForUpload(kPrinterCapsValue,
          caps_and_defaults.printer_capabilities, mime_boundary,
          caps_and_defaults.caps_mime_type, &post_data);
      cloud_print::AddMultipartValueForUpload(kPrinterDefaultsValue,
          caps_and_defaults.printer_defaults, mime_boundary,
          caps_and_defaults.defaults_mime_type, &post_data);
      cloud_print::AddMultipartValueForUpload(kPrinterCapsHashValue,
          caps_hash, mime_boundary, std::string(), &post_data);
    }
  } else {
    LOG(ERROR) << "Failed to get printer caps and defaults for printer: "
               << printer_name;
  }

  std::string tags_hash =
      CloudPrintHelpers::GenerateHashOfStringMap(printer_info.options);
  if (tags_hash != printer_info_cloud_.tags_hash) {
    printer_info_cloud_.tags_hash = tags_hash;
    CloudPrintHelpers::GenerateMultipartPostDataForPrinterTags(
        printer_info.options, mime_boundary, &post_data);
    // Remove all the exising proxy tags.
    std::string cp_tag_wildcard(kProxyTagPrefix);
    cp_tag_wildcard += ".*";
    cloud_print::AddMultipartValueForUpload(kPrinterRemoveTagValue,
        cp_tag_wildcard, mime_boundary, std::string(), &post_data);
  }

  if (printer_info.printer_name != printer_info_.printer_name) {
    cloud_print::AddMultipartValueForUpload(kPrinterNameValue,
        printer_info.printer_name, mime_boundary, std::string(), &post_data);
  }
  if (printer_info.printer_description != printer_info_.printer_description) {
    cloud_print::AddMultipartValueForUpload(kPrinterDescValue,
      printer_info.printer_description, mime_boundary,
      std::string(), &post_data);
  }
  if (printer_info.printer_status != printer_info_.printer_status) {
    cloud_print::AddMultipartValueForUpload(kPrinterStatusValue,
        base::StringPrintf("%d", printer_info.printer_status), mime_boundary,
        std::string(), &post_data);
  }
  printer_info_ = printer_info;
  if (!post_data.empty()) {
    // Terminate the request body
    post_data.append("--" + mime_boundary + "--\r\n");
    std::string mime_type("multipart/form-data; boundary=");
    mime_type += mime_boundary;
    SetNextJSONHandler(&PrinterJobHandler::HandlePrinterUpdateResponse);
    request_ = new CloudPrintURLFetcher;
    request_->StartPostRequest(
        CloudPrintHelpers::GetUrlForPrinterUpdate(
            cloud_print_server_url_, printer_info_cloud_.printer_id),
        this,
        kCloudPrintAPIMaxRetryCount,
        mime_type,
        post_data,
        std::string());
  } else {
    // We are done here. Go to the Stop state
    MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(&PrinterJobHandler::Stop, this));
  }
}

// The following methods are called on |print_thread_|. It is not safe to
// access any members other than |job_handler_message_loop_proxy_|,
// |job_spooler_| and |print_system_|.
void PrinterJobHandler::DoPrint(const JobDetails& job_details,
                                const std::string& printer_name) {
  job_spooler_ = print_system_->CreateJobSpooler();
  DCHECK(job_spooler_);
  if (!job_spooler_ || !job_spooler_->Spool(job_details.print_ticket_,
                                            job_details.print_data_file_path_,
                                            job_details.print_data_mime_type_,
                                            printer_name,
                                            job_details.job_title_,
                                            job_details.tags_,
                                            this)) {
    OnJobSpoolFailed();
  }
}
