// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/cloud_print/job_status_updater.h"

#include "base/json/json_reader.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/net/http_return.h"
#include "chrome/service/cloud_print/cloud_print_consts.h"
#include "chrome/service/cloud_print/cloud_print_helpers.h"
#include "googleurl/src/gurl.h"

JobStatusUpdater::JobStatusUpdater(const std::string& printer_name,
                           const std::string& job_id,
                           cloud_print::PlatformJobId& local_job_id,
                           const std::string& auth_token,
                           const GURL& cloud_print_server_url,
                           cloud_print::PrintSystem* print_system,
                           Delegate* delegate)
    : printer_name_(printer_name), job_id_(job_id),
      local_job_id_(local_job_id), auth_token_(auth_token),
      cloud_print_server_url_(cloud_print_server_url),
      print_system_(print_system), delegate_(delegate), stopped_(false) {
  DCHECK(delegate_);
}

// Start checking the status of the local print job.
void JobStatusUpdater::UpdateStatus() {
  // It does not matter if we had already sent out an update and are waiting for
  // a response. This is a new update and we will simply cancel the old request
  // and send a new one.
  if (!stopped_) {
    bool need_update = false;
    // If the job has already been completed, we just need to update the server
    // with that status. The *only* reason we would come back here in that case
    // is if our last server update attempt failed.
    if (last_job_details_.status == cloud_print::PRINT_JOB_STATUS_COMPLETED) {
      need_update = true;
    } else {
      cloud_print::PrintJobDetails details;
      if (print_system_->GetJobDetails(printer_name_, local_job_id_,
              &details)) {
        if (details != last_job_details_) {
          last_job_details_ = details;
          need_update = true;
        }
      } else {
        // If GetJobDetails failed, the most likely case is that the job no
        // longer exists in the OS queue. We are going to assume it is done in
        // this case.
        last_job_details_.Clear();
        last_job_details_.status = cloud_print::PRINT_JOB_STATUS_COMPLETED;
        need_update = true;
      }
    }
    if (need_update) {
      GURL update_url = CloudPrintHelpers::GetUrlForJobStatusUpdate(
          cloud_print_server_url_, job_id_, last_job_details_);
      request_.reset(new URLFetcher(update_url, URLFetcher::GET, this));
      CloudPrintHelpers::PrepCloudPrintRequest(request_.get(), auth_token_);
      request_->Start();
    }
  }
}

void JobStatusUpdater::Stop() {
  request_.reset();
  DCHECK(delegate_);
  stopped_ = true;
  delegate_->OnJobCompleted(this);
}

// URLFetcher::Delegate implementation.
void JobStatusUpdater::OnURLFetchComplete(const URLFetcher* source,
                                          const GURL& url,
                                          const URLRequestStatus& status,
                                          int response_code,
                                          const ResponseCookies& cookies,
                                          const std::string& data) {
  // If there was an auth error, we are done.
  if (RC_FORBIDDEN == response_code) {
    if (delegate_) {
      delegate_->OnAuthError();
    }
    return;
  }
  int64 next_update_interval = kJobStatusUpdateInterval;
  if (!status.is_success() || (response_code != 200)) {
    next_update_interval *= 10;
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE, NewRunnableMethod(this, &JobStatusUpdater::UpdateStatus),
        next_update_interval);
  } else if (last_job_details_.status ==
             cloud_print::PRINT_JOB_STATUS_COMPLETED) {
    MessageLoop::current()->PostTask(
        FROM_HERE, NewRunnableMethod(this, &JobStatusUpdater::Stop));
  }
}
