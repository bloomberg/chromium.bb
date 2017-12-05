// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_FAKES_FAKE_DOWNLOAD_TASK_H_
#define IOS_WEB_PUBLIC_TEST_FAKES_FAKE_DOWNLOAD_TASK_H_

#include <string>

#include "base/macros.h"
#include "base/observer_list.h"
#import "ios/web/public/download/download_task.h"
#include "url/gurl.h"

namespace web {

// Fake implementation for DownloadTask interface. Can be used for testing.
class FakeDownloadTask : public DownloadTask {
 public:
  FakeDownloadTask(const GURL& original_url, const std::string& mime_type);
  ~FakeDownloadTask() override;

  // DownloadTask overrides:
  void Start(std::unique_ptr<net::URLFetcherResponseWriter> writer) override;
  net::URLFetcherResponseWriter* GetResponseWriter() const override;
  NSString* GetIndentifier() const override;
  const GURL& GetOriginalUrl() const override;
  bool IsDone() const override;
  int GetErrorCode() const override;
  int64_t GetTotalBytes() const override;
  int GetPercentComplete() const override;
  std::string GetContentDisposition() const override;
  std::string GetMimeType() const override;
  base::string16 GetSuggestedFilename() const override;
  void AddObserver(DownloadTaskObserver* observer) override;
  void RemoveObserver(DownloadTaskObserver* observer) override;

  // Setters for task properties. Setters invoke OnDownloadUpdated callback.
  void SetDone(bool done);
  void SetErrorCode(int error_code);
  void SetTotalBytes(int64_t total_bytes);
  void SetPercentComplete(int percent_complete);
  void SetContentDisposition(const std::string& content_disposition);
  void SetMimeType(const std::string& mime_type);
  void SetSuggestedFilename(const base::string16& suggested_file_name);

 private:
  // Called when download task was updated.
  void OnDownloadUpdated();

  base::ObserverList<DownloadTaskObserver, true> observers_;

  std::unique_ptr<net::URLFetcherResponseWriter> writer_;
  GURL original_url_;
  bool is_done_ = false;
  int error_code_ = 0;
  std::string content_disposition_;
  int64_t total_bytes_ = -1;
  int percent_complete_ = -1;
  std::string mime_type_;
  base::string16 suggested_file_name_;
  __strong NSString* identifier_ = nil;

  DISALLOW_COPY_AND_ASSIGN(FakeDownloadTask);
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_FAKES_FAKE_DOWNLOAD_TASK_H_
