// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_FILE_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_FILE_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
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
  // Callback used with Rename().  On a successful rename |reason| will be
  // DOWNLOAD_INTERRUPT_REASON_NONE and |path| the path the rename
  // was done to.  On a failed rename, |reason| will contain the
  // error.
  typedef base::Callback<void(content::DownloadInterruptReason reason,
                              const FilePath& path)> RenameCompletionCallback;

  virtual ~DownloadFile() {}

  // If calculate_hash is true, sha256 hash will be calculated.
  // Returns DOWNLOAD_INTERRUPT_REASON_NONE on success, or a network
  // error code on failure.
  virtual DownloadInterruptReason Initialize() = 0;

  // Rename the download file to |full_path|.  If that file exists and
  // |overwrite_existing_file| is false, |full_path| will be uniquified by
  // suffixing " (<number>)" to the file name before the extension.
  // Upon completion, |callback| will be called on the UI thread
  // as per the comment above.
  virtual void Rename(const FilePath& full_path,
                      bool overwrite_existing_file,
                      const RenameCompletionCallback& callback) = 0;

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
