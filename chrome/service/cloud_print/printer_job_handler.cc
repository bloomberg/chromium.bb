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

PrinterJobHandler::PrinterJobHandler(
    const cloud_print::PrinterBasicInfo& printer_info,
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
      next_response_handler_(NULL),
      next_failure_handler_(NULL),
      server_error_count_(0),
      print_thread_("Chrome_CloudPrintJobPrintThread"),
      job_handler_message_loop_proxy_(
          base::MessageLoopProxy::CreateForCurrentThread()),
      shutting_down_(false),
      server_job_available_(false),
      printer_update_pending_(true),
      printer_delete_pending_(false),
      task_in_progress_(false) {
}

bool PrinterJobHandler::Initialize() {
  if (print_system_->IsValidPrinter(printer_info_.printer_name)) {
    printer_watcher_ = print_system_->CreatePrinterWatcher(
        printer_info_.printer_name);
    printer_watcher_->StartWatching(this);
    NotifyJobAvailable();
  } else {
    // This printer does not exist any more. Delete it from the server.
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
  request_.reset();
  print_thread_.Stop();
}

void PrinterJobHandler::Start() {
  LOG(INFO) << "CP_PROXY: Start printer job handler, id: " <<
      printer_info_cloud_.printer_id << ", task in progress: " <<
      task_in_progress_;
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
        MakeServerRequest(
            CloudPrintHelpers::GetUrlForPrinterDelete(
                cloud_print_server_url_, printer_info_cloud_.printer_id),
            &PrinterJobHandler::HandlePrinterDeleteResponse,
            &PrinterJobHandler::Stop);
      }
      if (!task_in_progress_ && printer_update_pending_) {
        printer_update_pending_ = false;
        task_in_progress_ = UpdatePrinterInfo();
      }
      if (!task_in_progress_ && server_job_available_) {
        task_in_progress_ = true;
        server_job_available_ = false;
        // We need to fetch any pending jobs for this printer
        MakeServerRequest(
            CloudPrintHelpers::GetUrlForJobFetch(
                cloud_print_server_url_, printer_info_cloud_.printer_id),
            &PrinterJobHandler::HandleJobMetadataResponse,
            &PrinterJobHandler::Stop);
      }
    }
  }
}

void PrinterJobHandler::Stop() {
  LOG(INFO) << "CP_PROXY: Stop printer job handler, id: " <<
      printer_info_cloud_.printer_id;
  task_in_progress_ = false;
  Reset();
  if (HavePendingTasks()) {
    MessageLoop::current()->PostTask(
        FROM_HERE, NewRunnableMethod(this, &PrinterJobHandler::Start));
  }
}

void PrinterJobHandler::NotifyJobAvailable() {
  LOG(INFO) << "CP_PROXY: Notify job available, id: " <<
      printer_info_cloud_.printer_id << ", task in progress: " <<
      task_in_progress_;
  server_job_available_ = true;
  if (!task_in_progress_) {
    MessageLoop::current()->PostTask(
        FROM_HERE, NewRunnableMethod(this, &PrinterJobHandler::Start));
  }
}

bool PrinterJobHandler::UpdatePrinterInfo() {
  LOG(INFO) << "CP_PROXY: Update printer info, id: " <<
      printer_info_cloud_.printer_id;
  // We need to update the parts of the printer info that have changed
  // (could be printer name, description, status or capabilities).
  cloud_print::PrinterBasicInfo printer_info;
  printer_watcher_->GetCurrentPrinterInfo(&printer_info);
  cloud_print::PrinterCapsAndDefaults printer_caps;
  std::string post_data;
  std::string mime_boundary;
  CloudPrintHelpers::CreateMimeBoundaryForUpload(&mime_boundary);
  if (print_system_->GetPrinterCapsAndDefaults(printer_info.printer_name,
                                               &printer_caps)) {
    std::string caps_hash = MD5String(printer_caps.printer_capabilities);
    if (caps_hash != printer_info_cloud_.caps_hash) {
      // Hashes don't match, we need to upload new capabilities (the defaults
      // go for free along with the capabilities)
      printer_info_cloud_.caps_hash = caps_hash;
      CloudPrintHelpers::AddMultipartValueForUpload(
          kPrinterCapsValue, printer_caps.printer_capabilities,
          mime_boundary, printer_caps.caps_mime_type, &post_data);
      CloudPrintHelpers::AddMultipartValueForUpload(
          kPrinterDefaultsValue, printer_caps.printer_defaults,
          mime_boundary, printer_caps.defaults_mime_type,
          &post_data);
      CloudPrintHelpers::AddMultipartValueForUpload(
          kPrinterCapsHashValue, caps_hash, mime_boundary, std::string(),
          &post_data);
    }
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
  bool ret = false;
  if (!post_data.empty()) {
    // Terminate the request body
    post_data.append("--" + mime_boundary + "--\r\n");
    std::string mime_type("multipart/form-data; boundary=");
    mime_type += mime_boundary;
    request_.reset(
        new URLFetcher(CloudPrintHelpers::GetUrlForPrinterUpdate(
                           cloud_print_server_url_,
                           printer_info_cloud_.printer_id),
                       URLFetcher::POST, this));
    CloudPrintHelpers::PrepCloudPrintRequest(request_.get(), auth_token_);
    request_->set_upload_data(mime_type, post_data);
    next_response_handler_ = &PrinterJobHandler::HandlePrinterUpdateResponse;
    next_failure_handler_ = &PrinterJobHandler::Stop;
    request_->Start();
    ret = true;
  }
  return ret;
}

// URLFetcher::Delegate implementation.
void PrinterJobHandler::OnURLFetchComplete(
    const URLFetcher* source, const GURL& url, const URLRequestStatus& status,
    int response_code, const ResponseCookies& cookies,
    const std::string& data) {
  LOG(INFO) << "CP_PROXY: Printer job handler, OnURLFetchComplete, url: " <<
      url << ", response code: " << response_code;
  // If there was an auth error, we are done.
  if (RC_FORBIDDEN == response_code) {
    OnAuthError();
    return;
  }
  if (!shutting_down_) {
    DCHECK(source == request_.get());
    // We need a next response handler because we are strictly a sequential
    // state machine. We need each response handler to tell us which state to
    //  advance to next.
    DCHECK(next_response_handler_);
    if (!(this->*next_response_handler_)(source, url, status,
                                         response_code, cookies, data)) {
      // By contract, if the response handler returns false, it wants us to
      // retry the request (upto the usual limit after which we give up and
      // send the state machine to the Stop state);
      HandleServerError(url);
    }
  }
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

bool PrinterJobHandler::HandlePrinterUpdateResponse(
    const URLFetcher* source, const GURL& url, const URLRequestStatus& status,
    int response_code, const ResponseCookies& cookies,
    const std::string& data) {
  bool ret = false;
  LOG(INFO) << "CP_PROXY: Handle printer update response, id: " <<
      printer_info_cloud_.printer_id;
  // If there was a network error or a non-200 response (which, for our purposes
  // is the same as a network error), we want to retry.
  if (status.is_success() && (response_code == 200)) {
    bool succeeded = false;
    DictionaryValue* response_dict = NULL;
    CloudPrintHelpers::ParseResponseJSON(data, &succeeded, &response_dict);
    // If we get valid JSON back, we are done.
    if (NULL != response_dict) {
      ret = true;
    }
  }
  if (ret) {
    // We are done here. Go to the Stop state
    MessageLoop::current()->PostTask(
        FROM_HERE, NewRunnableMethod(this, &PrinterJobHandler::Stop));
  } else {
    // Since we failed to update the server, set the flag again.
    printer_update_pending_ = true;
  }
  return ret;
}

bool PrinterJobHandler::HandlePrinterDeleteResponse(
    const URLFetcher* source, const GURL& url, const URLRequestStatus& status,
    int response_code, const ResponseCookies& cookies,
    const std::string& data) {
  bool ret = false;
  LOG(INFO) << "CP_PROXY: Handler printer delete response, id: " <<
      printer_info_cloud_.printer_id;
  // If there was a network error or a non-200 response (which, for our purposes
  // is the same as a network error), we want to retry.
  if (status.is_success() && (response_code == 200)) {
    bool succeeded = false;
    DictionaryValue* response_dict = NULL;
    CloudPrintHelpers::ParseResponseJSON(data, &succeeded, &response_dict);
    // If we get valid JSON back, we are done.
    if (NULL != response_dict) {
      ret = true;
    }
  }
  if (ret) {
    // The printer has been deleted. Shutdown the handler class.
    MessageLoop::current()->PostTask(
        FROM_HERE, NewRunnableMethod(this, &PrinterJobHandler::Shutdown));
  } else {
    // Since we failed to update the server, set the flag again.
    printer_delete_pending_ = true;
  }
  return ret;
}

bool PrinterJobHandler::HandleJobMetadataResponse(
    const URLFetcher* source, const GURL& url, const URLRequestStatus& status,
    int response_code, const ResponseCookies& cookies,
    const std::string& data) {
  LOG(INFO) << "CP_PROXY: Handle job metadata response, id: " <<
      printer_info_cloud_.printer_id;
  // If there was a network error or a non-200 response (which, for our purposes
  // is the same as a network error), we want to retry.
  if (!status.is_success() || (response_code != 200)) {
    return false;
  }
  bool succeeded = false;
  DictionaryValue* response_dict = NULL;
  CloudPrintHelpers::ParseResponseJSON(data, &succeeded, &response_dict);
  if (NULL == response_dict) {
    // If we did not get a valid JSON response, we need to retry.
    return false;
  }
  Task* next_task = NULL;
  if (succeeded) {
    ListValue* job_list = NULL;
    response_dict->GetList(kJobListValue, &job_list);
    if (job_list) {
      // Even though it is a job list, for now we are only interested in the
      // first job
      DictionaryValue* job_data = NULL;
      if (job_list->GetDictionary(0, &job_data)) {
        job_data->GetString(kIdValue, &job_details_.job_id_);
        job_data->GetString(kTitleValue, &job_details_.job_title_);
        std::string print_ticket_url;
        job_data->GetString(kTicketUrlValue, &print_ticket_url);
        job_data->GetString(kFileUrlValue, &print_data_url_);
        next_task = NewRunnableMethod(
              this, &PrinterJobHandler::MakeServerRequest,
              GURL(print_ticket_url.c_str()),
              &PrinterJobHandler::HandlePrintTicketResponse,
              &PrinterJobHandler::FailedFetchingJobData);
      }
    }
  }
  if (!next_task) {
    // If we got a valid JSON but there were no jobs, we are done
    next_task = NewRunnableMethod(this, &PrinterJobHandler::Stop);
  }
  delete response_dict;
  DCHECK(next_task);
  MessageLoop::current()->PostTask(FROM_HERE, next_task);
  return true;
}

bool PrinterJobHandler::HandlePrintTicketResponse(
    const URLFetcher* source, const GURL& url, const URLRequestStatus& status,
    int response_code, const ResponseCookies& cookies,
    const std::string& data) {
  LOG(INFO) << "CP_PROXY: Handle print ticket response, id: " <<
      printer_info_cloud_.printer_id;
  // If there was a network error or a non-200 response (which, for our purposes
  // is the same as a network error), we want to retry.
  if (!status.is_success() || (response_code != 200)) {
    return false;
  }
  if (print_system_->ValidatePrintTicket(printer_info_.printer_name, data)) {
    job_details_.print_ticket_ = data;
    MessageLoop::current()->PostTask(
        FROM_HERE,
        NewRunnableMethod(this,
                          &PrinterJobHandler::MakeServerRequest,
                          GURL(print_data_url_.c_str()),
                          &PrinterJobHandler::HandlePrintDataResponse,
                          &PrinterJobHandler::FailedFetchingJobData));
  } else {
    // The print ticket was not valid. We are done here.
    FailedFetchingJobData();
  }
  return true;
}

bool PrinterJobHandler::HandlePrintDataResponse(const URLFetcher* source,
                                                const GURL& url,
                                                const URLRequestStatus& status,
                                                int response_code,
                                                const ResponseCookies& cookies,
                                                const std::string& data) {
  LOG(INFO) << "CP_PROXY: Handle print data response, id: " <<
      printer_info_cloud_.printer_id;
  // If there was a network error or a non-200 response (which, for our purposes
  // is the same as a network error), we want to retry.
  if (!status.is_success() || (response_code != 200)) {
    return false;
  }
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
  return true;
}

void PrinterJobHandler::StartPrinting() {
  LOG(INFO) << "CP_PROXY: Start printing, id: " <<
      printer_info_cloud_.printer_id;
  // We are done with the request object for now.
  request_.reset();
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
  LOG(INFO) << "CP_PROXY: Job failed, id: " << printer_info_cloud_.printer_id;
  if (!shutting_down_) {
    UpdateJobStatus(cloud_print::PRINT_JOB_STATUS_ERROR, error);
  }
}

void PrinterJobHandler::JobSpooled(cloud_print::PlatformJobId local_job_id) {
  LOG(INFO) << "CP_PROXY: Job spooled, printer id: " <<
    printer_info_cloud_.printer_id << ", job id: " << local_job_id;
  if (!shutting_down_) {
    local_job_id_ = local_job_id;
    UpdateJobStatus(cloud_print::PRINT_JOB_STATUS_IN_PROGRESS, SUCCESS);
    print_thread_.Stop();
  }
}

void PrinterJobHandler::Shutdown() {
  LOG(INFO) << "CP_PROXY: Printer job handler shutdown, id: " <<
      printer_info_cloud_.printer_id;
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

void PrinterJobHandler::HandleServerError(const GURL& url) {
  LOG(INFO) << "CP_PROXY: Handle server error, printer id: " <<
      printer_info_cloud_.printer_id << ", url: " << url;
  Task* task_to_retry = NewRunnableMethod(this,
                                          &PrinterJobHandler::FetchURL, url);
  Task* task_on_give_up = NewRunnableMethod(this, next_failure_handler_);
  CloudPrintHelpers::HandleServerError(&server_error_count_, kMaxRetryCount,
                                       -1, kBaseRetryInterval, task_to_retry,
                                       task_on_give_up);
}

void PrinterJobHandler::UpdateJobStatus(cloud_print::PrintJobStatus status,
                                        PrintJobError error) {
  LOG(INFO) << "CP_PROXY: Update job status, id: " <<
      printer_info_cloud_.printer_id;
  if (!shutting_down_) {
    if (!job_details_.job_id_.empty()) {
      LOG(INFO) << "CP_PROXY: Updating status, jod id: " <<
          job_details_.job_id_ << ", status: " << status;

      ResponseHandler response_handler = NULL;
      if (error == SUCCESS) {
        response_handler =
            &PrinterJobHandler::HandleSuccessStatusUpdateResponse;
      } else {
        response_handler =
            &PrinterJobHandler::HandleFailureStatusUpdateResponse;
      }
      MakeServerRequest(
          CloudPrintHelpers::GetUrlForJobStatusUpdate(cloud_print_server_url_,
                                                      job_details_.job_id_,
                                                      status),
          response_handler,
          &PrinterJobHandler::Stop);
    }
  }
}

bool PrinterJobHandler::HandleSuccessStatusUpdateResponse(
    const URLFetcher* source, const GURL& url, const URLRequestStatus& status,
    int response_code, const ResponseCookies& cookies,
    const std::string& data) {
  LOG(INFO) << "CP_PROXY: Handle success status update response, id: " <<
      printer_info_cloud_.printer_id;
  // If there was a network error or a non-200 response (which, for our purposes
  // is the same as a network error), we want to retry.
  if (!status.is_success() || (response_code != 200)) {
    return false;
  }
  // The print job has been spooled locally. We now need to create an object
  // that monitors the status of the job and updates the server.
  scoped_refptr<JobStatusUpdater> job_status_updater =
      new JobStatusUpdater(printer_info_.printer_name, job_details_.job_id_,
                           local_job_id_, auth_token_, cloud_print_server_url_,
                           print_system_.get(), this);
  job_status_updater_list_.push_back(job_status_updater);
  MessageLoop::current()->PostTask(
      FROM_HERE, NewRunnableMethod(job_status_updater.get(),
                                   &JobStatusUpdater::UpdateStatus));
  bool succeeded = false;
  CloudPrintHelpers::ParseResponseJSON(data, &succeeded, NULL);
  if (succeeded) {
    // Since we just printed successfully, we want to look for more jobs.
    server_job_available_ = true;
  }
  MessageLoop::current()->PostTask(
      FROM_HERE, NewRunnableMethod(this, &PrinterJobHandler::Stop));
  return true;
}

bool PrinterJobHandler::HandleFailureStatusUpdateResponse(
    const URLFetcher* source, const GURL& url, const URLRequestStatus& status,
    int response_code, const ResponseCookies& cookies,
    const std::string& data) {
  LOG(INFO) << "CP_PROXY: Handle failure status update response, id: " <<
      printer_info_cloud_.printer_id;
  // If there was a network error or a non-200 response (which, for our purposes
  // is the same as a network error), we want to retry.
  if (!status.is_success() || (response_code != 200)) {
    return false;
  }
  MessageLoop::current()->PostTask(
      FROM_HERE, NewRunnableMethod(this, &PrinterJobHandler::Stop));
  return true;
}

void PrinterJobHandler::MakeServerRequest(const GURL& url,
                                          ResponseHandler response_handler,
                                          FailureHandler failure_handler) {
  LOG(INFO) << "CP_PROXY: Printer job handle, make server request, id: " <<
    printer_info_cloud_.printer_id << ", url: " << url;
  if (!shutting_down_) {
    server_error_count_ = 0;
    // Set up the next response handler
    next_response_handler_ = response_handler;
    next_failure_handler_ = failure_handler;
    FetchURL(url);
  }
}

void PrinterJobHandler::FetchURL(const GURL& url) {
  LOG(INFO) << "CP_PROXY: PrinterJobHandler::FetchURL, url: " << url;
  request_.reset(new URLFetcher(url, URLFetcher::GET, this));
  CloudPrintHelpers::PrepCloudPrintRequest(request_.get(), auth_token_);
  request_->Start();
}

bool PrinterJobHandler::HavePendingTasks() {
  return server_job_available_ || printer_update_pending_ ||
         printer_delete_pending_;
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

