// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_FILE_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_FILE_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "content/browser/download/base_file.h"
#include "content/browser/download/download_request_handle.h"
#include "content/browser/download/download_types.h"

struct DownloadCreateInfo;
class DownloadManager;
class ResourceDispatcherHost;

// These objects live exclusively on the download thread and handle the writing
// operations for one download. These objects live only for the duration that
// the download is 'in progress': once the download has been completed or
// cancelled, the DownloadFile is destroyed.
class DownloadFile : public BaseFile {
 public:
  DownloadFile(const DownloadCreateInfo* info,
               DownloadManager* download_manager);
  virtual ~DownloadFile();

  // Cancels the download request associated with this file.
  void CancelDownloadRequest();

  int id() const { return id_; }
  DownloadManager* GetDownloadManager();

  virtual std::string DebugString() const;

  // Appends the passed the number between parenthesis the path before the
  // extension.
  static void AppendNumberToPath(FilePath* path, int number);

  // Attempts to find a number that can be appended to that path to make it
  // unique. If |path| does not exist, 0 is returned.  If it fails to find such
  // a number, -1 is returned.
  static int GetUniquePathNumber(const FilePath& path);

  static FilePath AppendSuffixToPath(const FilePath& path,
                                     const FilePath::StringType& suffix);

  // Same as GetUniquePathNumber, except that it also checks the existence
  // of it with the given suffix.
  // If |path| does not exist, 0 is returned.  If it fails to find such
  // a number, -1 is returned.
  static int GetUniquePathNumberWithSuffix(
      const FilePath& path,
      const FilePath::StringType& suffix);

 private:
  // The unique identifier for this download, assigned at creation by
  // the DownloadFileManager for its internal record keeping.
  int id_;

  // The handle to the request information.  Used for operations outside the
  // download system, specifically canceling a download.
  DownloadRequestHandle request_handle_;

  // DownloadManager this download belongs to.
  scoped_refptr<DownloadManager> download_manager_;

  DISALLOW_COPY_AND_ASSIGN(DownloadFile);
};

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_FILE_H_
