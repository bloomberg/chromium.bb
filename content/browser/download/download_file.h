// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_FILE_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_FILE_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "content/common/content_export.h"
#include "content/public/browser/download_id.h"
#include "content/public/browser/download_interrupt_reasons.h"

namespace content {

class DownloadManager;

// These objects live exclusively on the file thread and handle the writing
// operations for one download. These objects live only for the duration that
// the download is 'in progress': once the download has been completed or
// cancelled, the DownloadFile is destroyed.
class CONTENT_EXPORT DownloadFile {
 public:
  virtual ~DownloadFile() {}

  // If calculate_hash is true, sha256 hash will be calculated.
  // Returns DOWNLOAD_INTERRUPT_REASON_NONE on success, or a network
  // error code on failure.
  virtual DownloadInterruptReason Initialize() = 0;

  // Rename the download file.
  // Returns net::OK on success, or a network error code on failure.
  virtual DownloadInterruptReason Rename(const FilePath& full_path) = 0;

  // Detach the file so it is not deleted on destruction.
  virtual void Detach() = 0;

  // Abort the download and automatically close the file.
  virtual void Cancel() = 0;

  // Informs the OS that this file came from the internet.
  virtual void AnnotateWithSourceInformation() = 0;

  virtual FilePath FullPath() const = 0;
  virtual bool InProgress() const = 0;
  virtual int64 BytesSoFar() const = 0;
  virtual int64 CurrentSpeed() const = 0;

  // Set |hash| with sha256 digest for the file.
  // Returns true if digest is successfully calculated.
  virtual bool GetHash(std::string* hash) = 0;

  // Returns the current (intermediate) state of the hash as a byte string.
  virtual std::string GetHashState() = 0;

  // Cancels the download request associated with this file.
  virtual void CancelDownloadRequest() = 0;

  virtual int Id() const = 0;
  virtual DownloadManager* GetDownloadManager() = 0;
  virtual const DownloadId& GlobalId() const = 0;

  virtual std::string DebugString() const = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_FILE_H_
