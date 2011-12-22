// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_TYPES_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_TYPES_H_
#pragma once

#include "base/file_path.h"
#include "base/memory/linked_ptr.h"
#include "content/common/content_export.h"
#include "net/base/file_stream.h"

// Holds the information about how to save a download file.
// In the case of download continuation, |file_path| is set to the current file
// name, |offset| is set to the point where we left off, and |hash_state| will
// hold the state of the hash algorithm where we left off.
struct CONTENT_EXPORT DownloadSaveInfo {
  DownloadSaveInfo();
  DownloadSaveInfo(const DownloadSaveInfo& info);
  ~DownloadSaveInfo();
  DownloadSaveInfo& operator=(const DownloadSaveInfo& info);

  // This is usually the tentative final name, but not during resumption
  // where it will be the intermediate file name.
  FilePath file_path;

  linked_ptr<net::FileStream> file_stream;

  string16 suggested_name;

  // The file offset at which to start the download.  May be 0.
  int64 offset;

  // The state of the hash at the start of the download.  May be empty.
  std::string hash_state;
};

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_TYPES_H_
