// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_TYPES_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_TYPES_H_
#pragma once

#include <vector>

#include "base/file_path.h"
#include "base/memory/linked_ptr.h"
#include "base/synchronization/lock.h"
#include "content/common/content_export.h"
#include "net/base/file_stream.h"

namespace net {
class IOBuffer;
}

// DownloadBuffer is created and populated on the IO thread, and passed to the
// file thread for writing. In order to avoid flooding the file thread with too
// many small write messages, each write is appended to the DownloadBuffer while
// waiting for the task to run on the file thread. Access to the write buffers
// is synchronized via the lock. Each entry in 'contents' represents one data
// buffer and its size in bytes.
struct CONTENT_EXPORT DownloadBuffer {
  DownloadBuffer();
  ~DownloadBuffer();

  base::Lock lock;
  typedef std::pair<net::IOBuffer*, int> Contents;
  std::vector<Contents> contents;
};

// Holds the information about how to save a download file.
struct CONTENT_EXPORT DownloadSaveInfo {
  DownloadSaveInfo();
  DownloadSaveInfo(const DownloadSaveInfo& info);
  ~DownloadSaveInfo();
  DownloadSaveInfo& operator=(const DownloadSaveInfo& info);

  FilePath file_path;
  linked_ptr<net::FileStream> file_stream;
  string16 suggested_name;
};

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_TYPES_H_
