// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_file.h"

#include <string>

#include "base/file_util.h"
#include "base/stringprintf.h"
#include "chrome/browser/download/download_create_info.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/download/download_util.h"
#include "content/browser/browser_thread.h"

DownloadFile::DownloadFile(const DownloadCreateInfo* info,
                           DownloadManager* download_manager)
    : BaseFile(info->save_info.file_path,
               info->url(),
               info->referrer_url,
               info->received_bytes,
               info->save_info.file_stream),
      id_(info->download_id),
      process_handle_(info->process_handle),
      download_manager_(download_manager) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
}

DownloadFile::~DownloadFile() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
}

void DownloadFile::CancelDownloadRequest(ResourceDispatcherHost* rdh) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  download_util::CancelDownloadRequest(rdh, process_handle_);
}

DownloadManager* DownloadFile::GetDownloadManager() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  return download_manager_.get();
}

std::string DownloadFile::DebugString() const {
  return base::StringPrintf("{"
                            " id_ = " "%d"
                            " child_id = " "%d"
                            " request_id = " "%d"
                            " render_view_id = " "%d"
                            " Base File = %s"
                            " }",
                            id_,
                            process_handle_.child_id(),
                            process_handle_.request_id(),
                            process_handle_.render_view_id(),
                            BaseFile::DebugString().c_str());
}
