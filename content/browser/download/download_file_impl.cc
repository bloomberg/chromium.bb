// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_file_impl.h"

#include <string>

#include "base/stringprintf.h"
#include "content/browser/download/download_create_info.h"
#include "content/browser/download/download_manager.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

DownloadFileImpl::DownloadFileImpl(
    const DownloadCreateInfo* info,
    DownloadRequestHandleInterface* request_handle,
    DownloadManager* download_manager)
    : file_(info->save_info.file_path,
            info->url(),
            info->referrer_url,
            info->received_bytes,
            info->save_info.file_stream),
      id_(info->download_id),
      request_handle_(request_handle),
      download_manager_(download_manager) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
}

DownloadFileImpl::~DownloadFileImpl() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
}

// BaseFile delegated functions.
net::Error DownloadFileImpl::Initialize(bool calculate_hash) {
  return file_.Initialize(calculate_hash);
}

net::Error DownloadFileImpl::AppendDataToFile(const char* data,
                                              size_t data_len) {
  return file_.AppendDataToFile(data, data_len);
}

net::Error DownloadFileImpl::Rename(const FilePath& full_path) {
  return file_.Rename(full_path);
}

void DownloadFileImpl::Detach() {
  file_.Detach();
}

void DownloadFileImpl::Cancel() {
  file_.Cancel();
}

void DownloadFileImpl::Finish() {
  file_.Finish();
}

void DownloadFileImpl::AnnotateWithSourceInformation() {
  file_.AnnotateWithSourceInformation();
}

FilePath DownloadFileImpl::FullPath() const {
  return file_.full_path();
}

bool DownloadFileImpl::InProgress() const {
  return file_.in_progress();
}

int64 DownloadFileImpl::BytesSoFar() const {
  return file_.bytes_so_far();
}

bool DownloadFileImpl::GetSha256Hash(std::string* hash) {
  return file_.GetSha256Hash(hash);
}

// DownloadFileInterface implementation.
void DownloadFileImpl::CancelDownloadRequest() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  request_handle_->CancelRequest();
}

int DownloadFileImpl::Id() const {
  return id_.local();
}

DownloadManager* DownloadFileImpl::GetDownloadManager() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  return download_manager_.get();
}

const DownloadId& DownloadFileImpl::GlobalId() const {
  return id_;
}

std::string DownloadFileImpl::DebugString() const {
  return base::StringPrintf("{"
                            " id_ = " "%d"
                            " request_handle = %s"
                            " Base File = %s"
                            " }",
                            id_.local(),
                            request_handle_->DebugString().c_str(),
                            file_.DebugString().c_str());
}
