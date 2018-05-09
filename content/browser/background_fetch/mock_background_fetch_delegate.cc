// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/background_fetch/mock_background_fetch_delegate.h"
#include "content/public/browser/background_fetch_description.h"
#include "content/public/browser/background_fetch_response.h"
#include "content/public/browser/browser_thread.h"
#include "net/http/http_response_headers.h"

namespace content {

MockBackgroundFetchDelegate::TestResponse::TestResponse() = default;

MockBackgroundFetchDelegate::TestResponse::~TestResponse() = default;

MockBackgroundFetchDelegate::TestResponseBuilder::TestResponseBuilder(
    int response_code)
    : response_(std::make_unique<TestResponse>()) {
  response_->succeeded_ = (response_code >= 200 && response_code < 300);
  response_->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      "HTTP/1.1 " + std::to_string(response_code));
}

MockBackgroundFetchDelegate::TestResponseBuilder::~TestResponseBuilder() =
    default;

MockBackgroundFetchDelegate::TestResponseBuilder&
MockBackgroundFetchDelegate::TestResponseBuilder::AddResponseHeader(
    const std::string& name,
    const std::string& value) {
  DCHECK(response_);
  response_->headers->AddHeader(name + ": " + value);
  return *this;
}

MockBackgroundFetchDelegate::TestResponseBuilder&
MockBackgroundFetchDelegate::TestResponseBuilder::SetResponseData(
    std::string data) {
  DCHECK(response_);
  response_->data.swap(data);
  return *this;
}

std::unique_ptr<MockBackgroundFetchDelegate::TestResponse>
MockBackgroundFetchDelegate::TestResponseBuilder::Build() {
  return std::move(response_);
}

MockBackgroundFetchDelegate::MockBackgroundFetchDelegate() {}

MockBackgroundFetchDelegate::~MockBackgroundFetchDelegate() {}

void MockBackgroundFetchDelegate::GetIconDisplaySize(
    GetIconDisplaySizeCallback callback) {}

void MockBackgroundFetchDelegate::CreateDownloadJob(
    std::unique_ptr<BackgroundFetchDescription> fetch_description) {}

void MockBackgroundFetchDelegate::DownloadUrl(
    const std::string& job_unique_id,
    const std::string& guid,
    const std::string& method,
    const GURL& url,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    const net::HttpRequestHeaders& headers) {
  // TODO(delphick): Currently we just disallow re-using GUIDs but later when we
  // use the DownloadService, we should signal StartResult::UNEXPECTED_GUID.
  DCHECK(seen_guids_.find(guid) == seen_guids_.end());

  download_guid_to_job_id_map_[guid] = job_unique_id;

  auto url_iter = url_responses_.find(url);
  if (url_iter == url_responses_.end()) {
    // Since no response was provided, do not respond. This allows testing
    // long-lived fetches.
    return;
  }

  std::unique_ptr<TestResponse> test_response = std::move(url_iter->second);
  url_responses_.erase(url_iter);

  std::unique_ptr<BackgroundFetchResponse> response =
      std::make_unique<BackgroundFetchResponse>(std::vector<GURL>({url}),
                                                test_response->headers);

  PostAbortCheckingTask(
      job_unique_id,
      base::BindOnce(&BackgroundFetchDelegate::Client::OnDownloadStarted,
                     client(), job_unique_id, guid, std::move(response)));

  if (test_response->data.size()) {
    // Report progress at 50% complete.
    PostAbortCheckingTask(
        job_unique_id,
        base::BindOnce(&BackgroundFetchDelegate::Client::OnDownloadUpdated,
                       client(), job_unique_id, guid,
                       test_response->data.size() / 2));

    // Report progress at 100% complete.
    PostAbortCheckingTask(
        job_unique_id,
        base::BindOnce(&BackgroundFetchDelegate::Client::OnDownloadUpdated,
                       client(), job_unique_id, guid,
                       test_response->data.size()));
  }

  if (test_response->succeeded_) {
    base::FilePath response_path;
    if (!temp_directory_.IsValid()) {
      CHECK(temp_directory_.CreateUniqueTempDir());
    }

    // Write the |response|'s data to a temporary file.
    CHECK(base::CreateTemporaryFileInDir(temp_directory_.GetPath(),
                                         &response_path));

    CHECK_NE(-1 /* error */,
             base::WriteFile(response_path, test_response->data.c_str(),
                             test_response->data.size()));

    PostAbortCheckingTask(
        job_unique_id,
        base::BindOnce(
            &BackgroundFetchDelegate::Client::OnDownloadComplete, client(),
            job_unique_id, guid,
            std::make_unique<BackgroundFetchResult>(
                base::Time::Now(), response_path, test_response->data.size())));
  } else {
    PostAbortCheckingTask(
        job_unique_id,
        base::BindOnce(&BackgroundFetchDelegate::Client::OnDownloadComplete,
                       client(), job_unique_id, guid,
                       std::make_unique<BackgroundFetchResult>(
                           base::Time::Now(),
                           BackgroundFetchResult::FailureReason::UNKNOWN)));
  }

  seen_guids_.insert(guid);
}

void MockBackgroundFetchDelegate::Abort(const std::string& job_unique_id) {
  aborted_jobs_.insert(job_unique_id);
}

void MockBackgroundFetchDelegate::RegisterResponse(
    const GURL& url,
    std::unique_ptr<TestResponse> response) {
  DCHECK_EQ(0u, url_responses_.count(url));
  url_responses_[url] = std::move(response);
}

void MockBackgroundFetchDelegate::PostAbortCheckingTask(
    const std::string& job_unique_id,
    base::OnceCallback<void()> callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&MockBackgroundFetchDelegate::RunAbortCheckingTask,
                     base::Unretained(this), job_unique_id,
                     std::move(callback)));
}

void MockBackgroundFetchDelegate::RunAbortCheckingTask(
    const std::string& job_unique_id,
    base::OnceCallback<void()> callback) {
  if (!aborted_jobs_.count(job_unique_id))
    std::move(callback).Run();
}

}  // namespace content
