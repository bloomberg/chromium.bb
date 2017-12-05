// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/fakes/fake_download_task.h"

#include "ios/web/public/download/download_task_observer.h"
#include "net/url_request/url_fetcher_response_writer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

FakeDownloadTask::FakeDownloadTask(const GURL& original_url,
                                   const std::string& mime_type)
    : original_url_(original_url), mime_type_(mime_type) {}

FakeDownloadTask::~FakeDownloadTask() = default;

void FakeDownloadTask::Start(
    std::unique_ptr<net::URLFetcherResponseWriter> writer) {
  writer_ = std::move(writer);
  OnDownloadUpdated();
}

net::URLFetcherResponseWriter* FakeDownloadTask::GetResponseWriter() const {
  return writer_.get();
}

NSString* FakeDownloadTask::GetIndentifier() const {
  return identifier_;
}

const GURL& FakeDownloadTask::GetOriginalUrl() const {
  return original_url_;
}

bool FakeDownloadTask::IsDone() const {
  return is_done_;
}

int FakeDownloadTask::GetErrorCode() const {
  return error_code_;
}

int64_t FakeDownloadTask::GetTotalBytes() const {
  return total_bytes_;
}

int FakeDownloadTask::GetPercentComplete() const {
  return percent_complete_;
}

std::string FakeDownloadTask::GetContentDisposition() const {
  return content_disposition_;
}

std::string FakeDownloadTask::GetMimeType() const {
  return mime_type_;
}

base::string16 FakeDownloadTask::GetSuggestedFilename() const {
  return suggested_file_name_;
}

void FakeDownloadTask::AddObserver(DownloadTaskObserver* observer) {
  DCHECK(!observers_.HasObserver(observer));
  observers_.AddObserver(observer);
}

void FakeDownloadTask::RemoveObserver(DownloadTaskObserver* observer) {
  DCHECK(observers_.HasObserver(observer));
  observers_.RemoveObserver(observer);
}

void FakeDownloadTask::SetDone(bool done) {
  is_done_ = done;
  OnDownloadUpdated();
}

void FakeDownloadTask::SetErrorCode(int error_code) {
  error_code_ = error_code;
  OnDownloadUpdated();
}

void FakeDownloadTask::SetTotalBytes(int64_t total_bytes) {
  total_bytes_ = total_bytes;
  OnDownloadUpdated();
}

void FakeDownloadTask::SetPercentComplete(int percent_complete) {
  percent_complete_ = percent_complete;
  OnDownloadUpdated();
}

void FakeDownloadTask::SetContentDisposition(
    const std::string& content_disposition) {
  content_disposition_ = content_disposition;
  OnDownloadUpdated();
}

void FakeDownloadTask::SetMimeType(const std::string& mime_type) {
  mime_type_ = mime_type;
  OnDownloadUpdated();
}

void FakeDownloadTask::SetSuggestedFilename(
    const base::string16& suggested_file_name) {
  suggested_file_name_ = suggested_file_name;
  OnDownloadUpdated();
}

void FakeDownloadTask::OnDownloadUpdated() {
  for (auto& observer : observers_)
    observer.OnDownloadUpdated(this);
}

}  // namespace web
