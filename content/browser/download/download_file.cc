// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_file.h"

#include <string>

#include "base/file_util.h"
#include "base/stringprintf.h"
#include "content/browser/browser_thread.h"
#include "content/browser/download/download_create_info.h"
#include "content/browser/download/download_manager.h"

namespace {

  // The maximum number of 'uniquified' files we will try to create.
// This is used when the filename we're trying to download is already in use,
// so we create a new unique filename by appending " (nnn)" before the
// extension, where 1 <= nnn <= kMaxUniqueFiles.
// Also used by code that cleans up said files.
static const int kMaxUniqueFiles = 100;

}

DownloadFile::DownloadFile(const DownloadCreateInfo* info,
                           DownloadManager* download_manager)
    : BaseFile(info->save_info.file_path,
               info->url(),
               info->referrer_url,
               info->received_bytes,
               info->save_info.file_stream),
      id_(info->download_id),
      request_handle_(info->request_handle),
      download_manager_(download_manager) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
}

DownloadFile::~DownloadFile() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
}

void DownloadFile::CancelDownloadRequest() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  request_handle_.CancelRequest();
}

DownloadManager* DownloadFile::GetDownloadManager() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  return download_manager_.get();
}

std::string DownloadFile::DebugString() const {
  return base::StringPrintf("{"
                            " id_ = " "%d"
                            " request_handle = %s"
                            " Base File = %s"
                            " }",
                            id_,
                            request_handle_.DebugString().c_str(),
                            BaseFile::DebugString().c_str());
}

void DownloadFile::AppendNumberToPath(FilePath* path, int number) {
  *path = path->InsertBeforeExtensionASCII(StringPrintf(" (%d)", number));
}

int DownloadFile::GetUniquePathNumber(const FilePath& path) {
  if (!file_util::PathExists(path))
    return 0;

  FilePath new_path;
  for (int count = 1; count <= kMaxUniqueFiles; ++count) {
    new_path = FilePath(path);
    AppendNumberToPath(&new_path, count);

    if (!file_util::PathExists(new_path))
      return count;
  }

  return -1;
}

FilePath DownloadFile::AppendSuffixToPath(
    const FilePath& path,
    const FilePath::StringType& suffix) {
  FilePath::StringType file_name;
  base::SStringPrintf(
      &file_name, PRFilePathLiteral PRFilePathLiteral, path.value().c_str(),
      suffix.c_str());
  return FilePath(file_name);
}

int DownloadFile::GetUniquePathNumberWithSuffix(
    const FilePath& path,
    const FilePath::StringType& suffix) {
  if (!file_util::PathExists(path) &&
      !file_util::PathExists(AppendSuffixToPath(path, suffix)))
    return 0;

  FilePath new_path;
  for (int count = 1; count <= kMaxUniqueFiles; ++count) {
    new_path = FilePath(path);
    AppendNumberToPath(&new_path, count);

    if (!file_util::PathExists(new_path) &&
        !file_util::PathExists(AppendSuffixToPath(new_path, suffix)))
      return count;
  }

  return -1;
}
