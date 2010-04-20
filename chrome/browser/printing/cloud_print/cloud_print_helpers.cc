// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/cloud_print/cloud_print_helpers.h"

#include "base/json/json_reader.h"
#include "base/rand_util.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/task.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/net/url_fetcher.h"
#include "chrome/browser/printing/cloud_print/cloud_print_consts.h"
#include "chrome/browser/profile.h"

std::string StringFromJobStatus(cloud_print::PrintJobStatus status) {
  std::string ret;
  switch (status) {
    case cloud_print::PRINT_JOB_STATUS_IN_PROGRESS:
      ret = "in_progress";
      break;
    case cloud_print::PRINT_JOB_STATUS_ERROR:
      ret = "error";
      break;
    case cloud_print::PRINT_JOB_STATUS_COMPLETED:
      ret = "done";
      break;
    default:
      ret = "unknown";
      NOTREACHED();
      break;
  }
  return ret;
}

GURL CloudPrintHelpers::GetUrlForPrinterRegistration() {
  return GURL(StringPrintf("%s/printing/register", kCloudPrintServerUrl));
}

GURL CloudPrintHelpers::GetUrlForPrinterUpdate(const std::string& printer_id) {
  return GURL(StringPrintf("%s/printing/update?printerid=%s",
                           kCloudPrintServerUrl, printer_id.c_str()));
}

GURL CloudPrintHelpers::GetUrlForPrinterDelete(const std::string& printer_id) {
  return GURL(StringPrintf("%s/printing/delete?printerid=%s",
                           kCloudPrintServerUrl, printer_id.c_str()));
}

GURL CloudPrintHelpers::GetUrlForPrinterList(const std::string& proxy_id) {
  return GURL(StringPrintf(
              "%s/printing/list?proxy=%s",
              kCloudPrintServerUrl, proxy_id.c_str()));
}

GURL CloudPrintHelpers::GetUrlForJobFetch(const std::string& printer_id) {
  return GURL(StringPrintf(
              "%s/printing/fetch?printerid=%s",
              kCloudPrintServerUrl, printer_id.c_str()));
}

GURL CloudPrintHelpers::GetUrlForJobStatusUpdate(
    const std::string& job_id, cloud_print::PrintJobStatus status) {
  std::string status_string = StringFromJobStatus(status);
  return GURL(StringPrintf(
              "%s/printing/control?jobid=%s&status=%s",
              kCloudPrintServerUrl, job_id.c_str(), status_string.c_str()));
}

GURL CloudPrintHelpers::GetUrlForJobStatusUpdate(
    const std::string& job_id, const cloud_print::PrintJobDetails& details) {
  std::string status_string = StringFromJobStatus(details.status);
  return GURL(StringPrintf(
              "%s/printing/control?jobid=%s&status=%s&code=%d&message=%s"
              "&numpages=%d&pagesprinted=%d",
              kCloudPrintServerUrl, job_id.c_str(), status_string.c_str(),
              details.platform_status_flags, details.status_message.c_str(),
              details.total_pages, details.pages_printed));
}

bool CloudPrintHelpers::ParseResponseJSON(
    const std::string& response_data, bool* succeeded,
    DictionaryValue** response_dict) {
  scoped_ptr<Value> message_value(base::JSONReader::Read(response_data, false));
  DCHECK(message_value.get());
  if (!message_value.get()) {
    return false;
  }
  if (!message_value->IsType(Value::TYPE_DICTIONARY)) {
    NOTREACHED();
    return false;
  }
  DictionaryValue* response_dict_local =
      static_cast<DictionaryValue*>(message_value.get());
  if (succeeded) {
    response_dict_local->GetBoolean(kSuccessValue, succeeded);
  }
  if (response_dict) {
    *response_dict = response_dict_local;
    message_value.release();
  }
  return true;
}

void CloudPrintHelpers::PrepCloudPrintRequest(URLFetcher* request,
                                              const std::string& auth_token) {
  request->set_request_context(
      Profile::GetDefaultRequestContext());
  std::string headers = "Authorization: GoogleLogin auth=";
  headers += auth_token;
  request->set_extra_request_headers(headers);
}

void CloudPrintHelpers::HandleServerError(int* error_count, int max_retry_count,
                                          int64 max_retry_interval,
                                          int64 base_retry_interval,
                                          Task* task_to_retry,
                                          Task* task_on_give_up) {
  (*error_count)++;
  if ((-1 != max_retry_count) && (*error_count > max_retry_count)) {
    if (task_on_give_up) {
      MessageLoop::current()->PostTask(FROM_HERE, task_on_give_up);
    }
  } else {
    int64 retry_interval = base_retry_interval * (*error_count);
    if ((-1 != max_retry_interval) && (retry_interval > max_retry_interval)) {
      retry_interval = max_retry_interval;
    }
    MessageLoop::current()->PostDelayedTask(FROM_HERE, task_to_retry,
        retry_interval);
  }
}

void CloudPrintHelpers::AddMultipartValueForUpload(
    const std::string& value_name, const std::string& value,
    const std::string& mime_boundary, const std::string& content_type,
    std::string* post_data) {
  DCHECK(post_data);
  // First line is the boundary
  post_data->append("--" + mime_boundary + "\r\n");
  // Next line is the Content-disposition
  post_data->append(StringPrintf("Content-Disposition: form-data; "
                   "name=\"%s\"\r\n", value_name.c_str()));
  if (!content_type.empty()) {
    // If Content-type is specified, the next line is that
    post_data->append(StringPrintf("Content-Type: %s\r\n",
                      content_type.c_str()));
  }
  // Leave an empty line and append the value.
  post_data->append(StringPrintf("\r\n%s\r\n", value.c_str()));
}

// Create a MIME boundary marker (27 '-' characters followed by 16 hex digits).
void CloudPrintHelpers::CreateMimeBoundaryForUpload(std::string* out) {
  int r1 = base::RandInt(0, kint32max);
  int r2 = base::RandInt(0, kint32max);
  SStringPrintf(out, "---------------------------%08X%08X", r1, r2);
}

