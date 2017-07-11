// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_request_info.h"

#include <string>

#include "base/strings/string_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item.h"
#include "net/http/http_response_headers.h"

namespace content {

BackgroundFetchRequestInfo::BackgroundFetchRequestInfo(
    int request_index,
    const ServiceWorkerFetchRequest& fetch_request)
    : RefCountedDeleteOnSequence<BackgroundFetchRequestInfo>(
          base::SequencedTaskRunnerHandle::Get()),
      request_index_(request_index),
      fetch_request_(fetch_request) {}

BackgroundFetchRequestInfo::~BackgroundFetchRequestInfo() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void BackgroundFetchRequestInfo::PopulateDownloadStateOnUI(
    DownloadItem* download_item,
    DownloadInterruptReason download_interrupt_reason) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!download_state_populated_);

  download_guid_ = download_item->GetGuid();
  download_state_ = download_item->GetState();

  // The response code, text and headers all are stored in the
  // net::HttpResponseHeaders object, shared by the |download_item|.
  if (download_item->GetResponseHeaders()) {
    const auto& headers = download_item->GetResponseHeaders();

    response_code_ = headers->response_code();
    response_text_ = headers->GetStatusText();

    size_t iter = 0;
    std::string name, value;

    while (headers->EnumerateHeaderLines(&iter, &name, &value))
      response_headers_[base::ToLowerASCII(name)] = value;
  }
}

void BackgroundFetchRequestInfo::SetDownloadStatePopulated() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  download_state_populated_ = true;
}

void BackgroundFetchRequestInfo::PopulateResponseFromDownloadItemOnUI(
    DownloadItem* download_item) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!response_data_populated_);

  url_chain_ = download_item->GetUrlChain();
  file_path_ = download_item->GetTargetFilePath();
  file_size_ = download_item->GetReceivedBytes();
  response_time_ = download_item->GetEndTime();
}

void BackgroundFetchRequestInfo::SetResponseDataPopulated() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  response_data_populated_ = true;
}

int BackgroundFetchRequestInfo::GetResponseCode() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(download_state_populated_);
  return response_code_;
}

const std::string& BackgroundFetchRequestInfo::GetResponseText() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(download_state_populated_);
  return response_text_;
}

const std::map<std::string, std::string>&
BackgroundFetchRequestInfo::GetResponseHeaders() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(download_state_populated_);
  return response_headers_;
}

const std::vector<GURL>& BackgroundFetchRequestInfo::GetURLChain() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(response_data_populated_);
  return url_chain_;
}

const base::FilePath& BackgroundFetchRequestInfo::GetFilePath() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(response_data_populated_);
  return file_path_;
}

int64_t BackgroundFetchRequestInfo::GetFileSize() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(response_data_populated_);
  return file_size_;
}

const base::Time& BackgroundFetchRequestInfo::GetResponseTime() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(response_data_populated_);
  return response_time_;
}

}  // namespace content
