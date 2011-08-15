// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_file.h"

#include <string>

#include "base/file_util.h"
#include "base/stringprintf.h"
#include "chrome/browser/download/download_util.h"
#include "content/browser/browser_thread.h"
#include "content/browser/download/download_create_info.h"
#include "content/browser/download/download_manager.h"

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
