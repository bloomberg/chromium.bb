// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_FILE_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_FILE_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "content/browser/download/base_file.h"
#include "content/browser/download/download_id.h"
#include "content/common/content_export.h"
#include "net/base/net_errors.h"

class DownloadManager;

// These objects live exclusively on the file thread and handle the writing
// operations for one download. These objects live only for the duration that
// the download is 'in progress': once the download has been completed or
// cancelled, the DownloadFile is destroyed.
class CONTENT_EXPORT DownloadFile {
 public:
  virtual ~DownloadFile() {}

  // If calculate_hash is true, sha256 hash will be calculated.
  // Returns net::OK on success, or a network error code on failure.
  virtual net::Error Initialize(bool calculate_hash) = 0;

  // Write a new chunk of data to the file.
  // Returns net::OK on success (all bytes written to the file),
  // or a network error code on failure.
  virtual net::Error AppendDataToFile(const char* data, size_t data_len) = 0;

  // Rename the download file.
  // Returns net::OK on success, or a network error code on failure.
  virtual net::Error Rename(const FilePath& full_path) = 0;

  // Detach the file so it is not deleted on destruction.
  virtual void Detach() = 0;

  // Abort the download and automatically close the file.
  virtual void Cancel() = 0;

  // Indicate that the download has finished. No new data will be received.
  virtual void Finish() = 0;

  // Informs the OS that this file came from the internet.
  virtual void AnnotateWithSourceInformation() = 0;

  virtual FilePath FullPath() const = 0;
  virtual bool InProgress() const = 0;
  virtual int64 BytesSoFar() const = 0;

  // Set |hash| with sha256 digest for the file.
  // Returns true if digest is successfully calculated.
  virtual bool GetSha256Hash(std::string* hash) = 0;

  // Cancels the download request associated with this file.
  virtual void CancelDownloadRequest() = 0;

  virtual int Id() const = 0;
  virtual DownloadManager* GetDownloadManager() = 0;
  virtual const DownloadId& GlobalId() const = 0;

  virtual std::string DebugString() const = 0;

  // Appends the passed-in |number| between parenthesis to the |path| before
  // the file extension.
  static void AppendNumberToPath(FilePath* path, int number);

  // Appends the passed-in |suffix| to the |path|.
  static FilePath AppendSuffixToPath(const FilePath& path,
                                     const FilePath::StringType& suffix);

  // Attempts to find a number that can be appended to the |path| to make it
  // unique. If |path| does not exist, 0 is returned.  If it fails to find such
  // a number, -1 is returned.
  static int GetUniquePathNumber(const FilePath& path);

  // Same as GetUniquePathNumber, except that it also checks the existence
  // of it with the given suffix.
  // If |path| does not exist, 0 is returned.  If it fails to find such
  // a number, -1 is returned.
  static int GetUniquePathNumberWithSuffix(
      const FilePath& path,
      const FilePath::StringType& suffix);
};

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_FILE_H_
