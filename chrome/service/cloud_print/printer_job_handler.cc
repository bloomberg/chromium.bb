// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/cloud_print/printer_job_handler.h"

#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/md5.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/net/http_return.h"
#include "chrome/service/cloud_print/cloud_print_consts.h"
#include "chrome/service/cloud_print/cloud_print_helpers.h"
#include "chrome/service/cloud_print/job_status_updater.h"
#include "googleurl/src/gurl.h"
#include "net/http/http_response_headers.h"

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
    const std::string& auth_token,
    const GURL& cloud_print_server_url,
    cloud_print::PrintSystem* print_system,
    Delegate* delegate)
    : print_system_(print_system),
      printer_info_(printer_info),
      printer_info_cloud_(printer_info_cloud),
      auth_token_(auth_token),
      cloud_print_server_url_(cloud_print_server_url),
      delegate_(delegate),
      local_job_id_(-1),
      next_json_data_handler_(NULL),
      next_data_handler_(NULL),
      server_error_count_(0),
      print_thread_("Chrome_CloudPrintJobPrintThread"),
      job_handler_message_loop_proxy_(
          base::MessageLoopProxy::CreateForCurrentThread()),
      shutting_down_(false),
      job_check_pending_(false),
      printer_update_pending_(true),
      printer_delete_pending_(false),
      task_in_progress_(false) {
}

bool PrinterJobHandler::Initialize() {
  if (print_system_->IsValidPrinter(
      printer_info_.printer_name)) {
    printer_watcher_ = print_system_->CreatePrinterWatcher(
        printer_info_.printer_name);
    printer_watcher_->StartWatching(this);
    CheckForJobs(kJobFetchReasonStartup);
  } else {
    // This printer does not exist any more. Check if we should delete it from
    // the server.
    bool delete_from_server = false;
    delegate_->OnPrinterNotFound(printer_info_.printer_name,
                                 &delete_from_server);
    if (delete_from_server)
      OnPrinterDeleted();
  }
  return true;
}

PrinterJobHandler::~PrinterJobHandler() {
  if (printer_watcher_)
    printer_watcher_->StopWatching();
}

void PrinterJobHandler::Reset() {
  print_data_url_.clear();
  job_details_.Clear();
  request_ = NULL;
  print_thread_.Stop();
}

void PrinterJobHandler::Start() {
  VLOG(1) << "CP_PROXY: Start printer job handler, id: "
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
      if (printer_delete_pending_) {
        printer_delete_pending_ = false;
        task_in_progress_ = true;
        SetNextJSONHandler(&PrinterJobHandler::HandlePrinterDeleteResponse);
        request_ = new CloudPrintURLFetcher;
        request_->StartGetRequest(
            CloudPrintHelpers::GetUrlForPrinterDelete(
                cloud_print_server_url_, printer_info_cloud_.printer_id),
            this,
            auth_token_,
            kCloudPrintAPIMaxRetryCount,
            std::string());
      }
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
            auth_token_,
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
  VLOG(1) << "CP_PROXY: Stop printer job handler, id: "
          << printer_info_cloud_.printer_id;
  task_in_progress_ = false;
  Reset();
  if (HavePendingTasks()) {
    MessageLoop::current()->PostTask(
        FROM_HERE, NewRunnableMethod(this, &PrinterJobHandler::Start));
  }
}

void PrinterJobHandler::CheckForJobs(const std::string& reason) {
  VLOG(1) << "CP_PROXY: CheckForJobs, id: "
          << printer_info_cloud_.printer_id
          << ", reason: " << reason
          << ", task in progress: " << task_in_progress_;
  job_fetch_reason_ = reason;
  job_check_pending_ = true;
  if (!task_in_progress_) {
    MessageLoop::current()->PostTask(
        FROM_HERE, NewRunnableMethod(this, &PrinterJobHandler::Start));
  }
}

bool PrinterJobHandler::UpdatePrinterInfo() {
  VLOG(1) << "CP_PROXY: Update printer info, id: "
          << printer_info_cloud_.printer_id;
  // We need to update the parts of the printer info that have changed
  // (could be printer name, description, status or capabilities).
  // First asynchronously fetch the capabilities.
  printing::PrinterBasicInfo printer_info;
  printer_watcher_->GetCurrentPrinterInfo(&printer_info);
  cloud_print::PrintSystem::PrinterCapsAndDefaultsCallback* callback =
       NewCallback(this,
                   &PrinterJobHandler::OnReceivePrinterCaps);
  // Asnchronously fetch the printer caps and defaults. The story will
  // continue in OnReceivePrinterCaps.
  print_system_->GetPrinterCapsAndDefaults(
      printer_info.printer_name.c_str(), callback);
  // While we are waiting for the data, pretend we have work to do and return
  // true.
  return true;
}

void PrinterJobHandler::OnReceivePrinterCaps(
    bool succeeded,
    const std::string& printer_name,
    const printing::PrinterCapsAndDefaults& caps_and_defaults) {
  printing::PrinterBasicInfo printer_info;
  printer_watcher_->GetCurrentPrinterInfo(&printer_info);

  std::string post_data;
  std::string mime_boundary;
  CloudPrintHelpers::CreateMimeBoundaryForUpload(&mime_boundary);

  if (succeeded) {
    std::string caps_hash = MD5String(caps_and_defaults.printer_capabilities);
    if (caps_hash != printer_info_cloud_.caps_hash) {
      // Hashes don't match, we need to upload new capabilities (the defaults
      // go for free along with the capabilities)
      printer_info_cloud_.caps_hash = caps_hash;
      CloudPrintHelpers::AddMultipartValueForUpload(
          kPrinterCapsValue, caps_and_defaults.printer_capabilities,
          mime_boundary, caps_and_defaults.caps_mime_type, &post_data);
      CloudPrintHelpers::AddMultipartValueForUpload(
          kPrinterDefaultsValue, caps_and_defaults.printer_defaults,
          mime_boundary, caps_and_defaults.defaults_mime_type,
          &post_data);
      CloudPrintHelpers::AddMultipartValueForUpload(
          kPrinterCapsHashValue, caps_hash, mime_boundary, std::string(),
          &post_data);
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
    CloudPrintHelpers::AddMultipartValueForUpload(
        kPrinterRemoveTagValue, cp_tag_wildcard, mime_boundary, std::string(),
        &post_data);
  }

  if (printer_info.printer_name != printer_info_.printer_name) {
    CloudPrintHelpers::AddMultipartValueForUpload(kPrinterNameValue,
                                                  printer_info.printer_name,
                                                  mime_boundary,
                                                  std::string(), &post_data);
  }
  if (printer_info.printer_description != printer_info_.printer_description) {
    CloudPrintHelpers::AddMultipartValueForUpload(
        kPrinterDescValue, printer_info.printer_description, mime_boundary,
        std::string() , &post_data);
  }
  if (printer_info.printer_status != printer_info_.printer_status) {
    CloudPrintHelpers::AddMultipartValueForUpload(
        kPrinterStatusValue, StringPrintf("%d", printer_info.printer_status),
        mime_boundary, std::string(), &post_data);
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
        auth_token_,
        kCloudPrintAPIMaxRetryCount,
        mime_type,
        post_data,
        std::string());
  } else {
    // We are done here. Go to the Stop state
    MessageLoop::current()->PostTask(
        FROM_HERE, NewRunnableMethod(this, &PrinterJobHandler::Stop));
  }
}

// CloudPrintURLFetcher::Delegate implementation.
CloudPrintURLFetcher::ResponseAction PrinterJobHandler::HandleRawData(
    const URLFetcher* source,
    const GURL& url,
    const std::string& data) {
  if (!next_data_handler_)
    return CloudPrintURLFetcher::CONTINUE_PROCESSING;
  return (this->*next_data_handler_)(source, url, data);
}

CloudPrintURLFetcher::ResponseAction PrinterJobHandler::HandleJSONData(
    const URLFetcher* source,
    const GURL& url,
    DictionaryValue* json_data,
    bool succeeded) {
  DCHECK(next_json_data_handler_);
  return (this->*next_json_data_handler_)(source,
                                          url,
                                          json_data,
                                          succeeded);
}

void PrinterJobHandler::OnRequestGiveUp() {
  MessageLoop::current()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &PrinterJobHandler::Stop));
}

void PrinterJobHandler::OnRequestAuthError() {
  OnAuthError();
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
  printer_delete_pending_ = true;
  if (!task_in_progress_) {
    MessageLoop::current()->PostTask(
        FROM_HERE, NewRunnableMethod(this, &PrinterJobHandler::Start));
  }
}

void PrinterJobHandler::OnPrinterChanged() {
  printer_update_pending_ = true;
  if (!task_in_progress_) {
    MessageLoop::current()->PostTask(
        FROM_HERE, NewRunnableMethod(this, &PrinterJobHandler::Start));
  }
}

void PrinterJobHandler::OnJobChanged() {
  // Some job on the printer changed. Loop through all our JobStatusUpdaters
  // and have them check for updates.
  for (JobStatusUpdaterList::iterator index = job_status_updater_list_.begin();
       index != job_status_updater_list_.end(); index++) {
    MessageLoop::current()->PostTask(
        FROM_HERE, NewRunnableMethod(index->get(),
                                     &JobStatusUpdater::UpdateStatus));
  }
}

// Begin Response handlers
CloudPrintURLFetcher::ResponseAction
PrinterJobHandler::HandlePrinterUpdateResponse(
    const URLFetcher* source,
    const GURL& url,
    DictionaryValue* json_data,
    bool succeeded) {
  VLOG(1) << "CP_PROXY: Handle printer update response, id: "
          << printer_info_cloud_.printer_id;
  // We are done here. Go to the Stop state
  MessageLoop::current()->PostTask(
      FROM_HERE, NewRunnableMethod(this, &PrinterJobHandler::Stop));
  return CloudPrintURLFetcher::STOP_PROCESSING;
}

CloudPrintURLFetcher::ResponseAction
PrinterJobHandler::HandlePrinterDeleteResponse(
    const URLFetcher* source,
    const GURL& url,
    DictionaryValue* json_data,
    bool succeeded) {
  VLOG(1) << "CP_PROXY: Handler printer delete response, id: "
          << printer_info_cloud_.printer_id;
  // The printer has been deleted. Shutdown the handler class.
  MessageLoop::current()->PostTask(
      FROM_HERE, NewRunnableMethod(this, &PrinterJobHandler::Shutdown));
  return CloudPrintURLFetcher::STOP_PROCESSING;
}

CloudPrintURLFetcher::ResponseAction
PrinterJobHandler::HandleJobMetadataResponse(
    const URLFetcher* source,
    const GURL& url,
    DictionaryValue* json_data,
    bool succeeded) {
  VLOG(1) << "CP_PROXY: Handle job metadata response, id: "
          << printer_info_cloud_.printer_id;
  bool job_available = false;
  if (succeeded) {
    ListValue* job_list = NULL;
    json_data->GetList(kJobListValue, &job_list);
    if (job_list) {
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
                                  auth_token_,
                                  kCloudPrintAPIMaxRetryCount,
                                  std::string());
      }
    }
  }
  // If no jobs are available, go to the Stop state.
  if (!job_available)
    MessageLoop::current()->PostTask(
        FROM_HERE, NewRunnableMethod(this, &PrinterJobHandler::Stop));
  return CloudPrintURLFetcher::STOP_PROCESSING;
}

CloudPrintURLFetcher::ResponseAction
PrinterJobHandler::HandlePrintTicketResponse(const URLFetcher* source,
                                             const GURL& url,
                                             const std::string& data) {
  VLOG(1) << "CP_PROXY: Handle print ticket response, id: "
          << printer_info_cloud_.printer_id;
  if (print_system_->ValidatePrintTicket(printer_info_.printer_name, data)) {
    job_details_.print_ticket_ = data;
    SetNextDataHandler(&PrinterJobHandler::HandlePrintDataResponse);
    request_ = new CloudPrintURLFetcher;
    std::string accept_headers = "Accept: ";
    accept_headers += print_system_->GetSupportedMimeTypes();
    request_->StartGetRequest(GURL(print_data_url_.c_str()),
                              this,
                              auth_token_,
                              kJobDataMaxRetryCount,
                              accept_headers);
  } else {
    // The print ticket was not valid. We are done here.
    FailedFetchingJobData();
  }
  return CloudPrintURLFetcher::STOP_PROCESSING;
}

CloudPrintURLFetcher::ResponseAction
PrinterJobHandler::HandlePrintDataResponse(const URLFetcher* source,
                                           const GURL& url,
                                           const std::string& data) {
  VLOG(1) << "CP_PROXY: Handle print data response, id: "
          << printer_info_cloud_.printer_id;
  Task* next_task = NULL;
  if (file_util::CreateTemporaryFile(&job_details_.print_data_file_path_)) {
    int ret = file_util::WriteFile(job_details_.print_data_file_path_,
                                   data.c_str(),
                                   data.length());
    source->response_headers()->GetMimeType(
        &job_details_.print_data_mime_type_);
    DCHECK(ret == static_cast<int>(data.length()));
    if (ret == static_cast<int>(data.length())) {
      next_task = NewRunnableMethod(this, &PrinterJobHandler::StartPrinting);
    }
  }
  // If there was no task allocated above, then there was an error in
  // saving the print data, bail out here.
  if (!next_task) {
    next_task = NewRunnableMethod(this, &PrinterJobHandler::JobFailed,
                                  JOB_DOWNLOAD_FAILED);
  }
  MessageLoop::current()->PostTask(FROM_HERE, next_task);
  return CloudPrintURLFetcher::STOP_PROCESSING;
}

CloudPrintURLFetcher::ResponseAction
PrinterJobHandler::HandleSuccessStatusUpdateResponse(
    const URLFetcher* source,
    const GURL& url,
    DictionaryValue* json_data,
    bool succeeded) {
  VLOG(1) << "CP_PROXY: Handle success status update response, id: "
          << printer_info_cloud_.printer_id;
  // The print job has been spooled locally. We now need to create an object
  // that monitors the status of the job and updates the server.
  scoped_refptr<JobStatusUpdater> job_status_updater(
      new JobStatusUpdater(printer_info_.printer_name, job_details_.job_id_,
                           local_job_id_, auth_token_, cloud_print_server_url_,
                           print_system_.get(), this));
  job_status_updater_list_.push_back(job_status_updater);
  MessageLoop::current()->PostTask(
      FROM_HERE, NewRunnableMethod(job_status_updater.get(),
                                   &JobStatusUpdater::UpdateStatus));
  if (succeeded) {
    // Since we just printed successfully, we want to look for more jobs.
    CheckForJobs(kJobFetchReasonQueryMore);
  }
  MessageLoop::current()->PostTask(
      FROM_HERE, NewRunnableMethod(this, &PrinterJobHandler::Stop));
  return CloudPrintURLFetcher::STOP_PROCESSING;
}

CloudPrintURLFetcher::ResponseAction
PrinterJobHandler::HandleFailureStatusUpdateResponse(
    const URLFetcher* source,
    const GURL& url,
    DictionaryValue* json_data,
    bool succeeded) {
  VLOG(1) << "CP_PROXY: Handle failure status update response, id: "
          << printer_info_cloud_.printer_id;
  MessageLoop::current()->PostTask(
      FROM_HERE, NewRunnableMethod(this, &PrinterJobHandler::Stop));
  return CloudPrintURLFetcher::STOP_PROCESSING;
}
// End Response handlers

void PrinterJobHandler::StartPrinting() {
  VLOG(1) << "CP_PROXY: Start printing, id: " << printer_info_cloud_.printer_id;
  // We are done with the request object for now.
  request_ = NULL;
  if (!shutting_down_) {
    if (!print_thread_.Start()) {
      JobFailed(PRINT_FAILED);
    } else {
      print_thread_.message_loop()->PostTask(
          FROM_HERE, NewRunnableMethod(this, &PrinterJobHandler::DoPrint,
                                       job_details_,
                                       printer_info_.printer_name));
    }
  }
}

void PrinterJobHandler::JobFailed(PrintJobError error) {
  VLOG(1) << "CP_PROXY: Job failed, id: " << printer_info_cloud_.printer_id;
  if (!shutting_down_) {
    UpdateJobStatus(cloud_print::PRINT_JOB_STATUS_ERROR, error);
  }
}

void PrinterJobHandler::JobSpooled(cloud_print::PlatformJobId local_job_id) {
  VLOG(1) << "CP_PROXY: Job spooled, printer id: "
          << printer_info_cloud_.printer_id << ", job id: " << local_job_id;
  if (!shutting_down_) {
    local_job_id_ = local_job_id;
    UpdateJobStatus(cloud_print::PRINT_JOB_STATUS_IN_PROGRESS, SUCCESS);
    print_thread_.Stop();
  }
}

void PrinterJobHandler::Shutdown() {
  VLOG(1) << "CP_PROXY: Printer job handler shutdown, id: "
          << printer_info_cloud_.printer_id;
  Reset();
  shutting_down_ = true;
  while (!job_status_updater_list_.empty()) {
    // Calling Stop() will cause the OnJobCompleted to be called which will
    // remove the updater object from the list.
    job_status_updater_list_.front()->Stop();
  }
  if (delegate_) {
    delegate_->OnPrinterJobHandlerShutdown(this,
                                           printer_info_cloud_.printer_id);
  }
}

void PrinterJobHandler::UpdateJobStatus(cloud_print::PrintJobStatus status,
                                        PrintJobError error) {
  VLOG(1) << "CP_PROXY: Update job status, id: "
          << printer_info_cloud_.printer_id;
  if (!shutting_down_) {
    if (!job_details_.job_id_.empty()) {
      VLOG(1) << "CP_PROXY: Updating status, job id: " << job_details_.job_id_
              << ", status: " << status;
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
          auth_token_,
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

bool PrinterJobHandler::HavePendingTasks() {
  return (job_check_pending_ || printer_update_pending_ ||
          printer_delete_pending_);
}

void PrinterJobHandler::FailedFetchingJobData() {
  if (!shutting_down_) {
    LOG(ERROR) << "CP_PROXY: Failed fetching job data for printer: " <<
        printer_info_.printer_name << ", job id: " << job_details_.job_id_;
    JobFailed(INVALID_JOB_DATA);
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

void PrinterJobHandler::OnJobSpoolSucceeded(
    const cloud_print::PlatformJobId& job_id) {
  DCHECK(MessageLoop::current() == print_thread_.message_loop());
  job_spooler_ = NULL;
  job_handler_message_loop_proxy_->PostTask(FROM_HERE,
                                            NewRunnableMethod(this,
                                                &PrinterJobHandler::JobSpooled,
                                                job_id));
}

void PrinterJobHandler::OnJobSpoolFailed() {
  DCHECK(MessageLoop::current() == print_thread_.message_loop());
  job_spooler_ = NULL;
  job_handler_message_loop_proxy_->PostTask(FROM_HERE,
                                            NewRunnableMethod(this,
                                                &PrinterJobHandler::JobFailed,
                                                PRINT_FAILED));
}
