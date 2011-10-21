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
