// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_BASE_FILE_H_
#define CONTENT_BROWSER_DOWNLOAD_BASE_FILE_H_

#include <string>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "content/common/content_export.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "googleurl/src/gurl.h"
#include "net/base/file_stream.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"

namespace crypto {
class SecureHash;
}
namespace net {
class FileStream;
}

namespace content {

// File being downloaded and saved to disk. This is a base class
// for DownloadFile and SaveFile, which keep more state information.
class CONTENT_EXPORT BaseFile {
 public:
  // May be constructed on any thread.  All other routines (including
  // destruction) must occur on the FILE thread.
  BaseFile(const base::FilePath& full_path,
           const GURL& source_url,
           const GURL& referrer_url,
           int64 received_bytes,
           bool calculate_hash,
           const std::string& hash_state,
           scoped_ptr<net::FileStream> file_stream,
           const net::BoundNetLog& bound_net_log);
  virtual ~BaseFile();

  // Returns DOWNLOAD_INTERRUPT_REASON_NONE on success, or a
  // DownloadInterruptReason on failure.  |default_directory| specifies the
  // directory to create the temporary file in if |full_path()| is empty. If
  // |default_directory| and |full_path()| are empty, then a temporary file will
  // be created in the default download location as determined by
  // ContentBrowserClient.
  DownloadInterruptReason Initialize(const base::FilePath& default_directory);

  // Write a new chunk of data to the file. Returns a DownloadInterruptReason
  // indicating the result of the operation.
  DownloadInterruptReason AppendDataToFile(const char* data, size_t data_len);

  // Rename the download file. Returns a DownloadInterruptReason indicating the
  // result of the operation.
  virtual DownloadInterruptReason Rename(const base::FilePath& full_path);

  // Detach the file so it is not deleted on destruction.
  virtual void Detach();

  // Abort the download and automatically close the file.
  void Cancel();

  // Indicate that the download has finished. No new data will be received.
  void Finish();

  // Informs the OS that this file came from the internet. Returns a
  // DownloadInterruptReason indicating the result of the operation.
  DownloadInterruptReason AnnotateWithSourceInformation();

  // Calculate and return the current speed in bytes per second.
  int64 CurrentSpeed() const;

  base::FilePath full_path() const { return full_path_; }
  bool in_progress() const { return file_stream_.get() != NULL; }
  int64 bytes_so_far() const { return bytes_so_far_; }

  // Fills |hash| with the hash digest for the file.
  // Returns true if digest is successfully calculated.
  virtual bool GetHash(std::string* hash);

  // Returns the current (intermediate) state of the hash as a byte string.
  virtual std::string GetHashState();

  // Returns true if the given hash is considered empty.  An empty hash is
  // a string of size kSha256HashLen that contains only zeros (initial value
  // for the hash).
  static bool IsEmptyHash(const std::string& hash);

  virtual std::string DebugString() const;

 private:
  friend class BaseFileTest;
  FRIEND_TEST_ALL_PREFIXES(BaseFileTest, IsEmptyHash);

  // Re-initializes file_stream_ with a newly allocated net::FileStream().
  void CreateFileStream();

  // Creates and opens the file_stream_ if it is NULL.
  DownloadInterruptReason Open();

  // Closes and resets file_stream_.
  void Close();

  // Resets file_stream_.
  void ClearStream();

  // Platform specific method that moves a file to a new path and adjusts the
  // security descriptor / permissions on the file to match the defaults for the
  // new directory.
  DownloadInterruptReason MoveFileAndAdjustPermissions(
      const base::FilePath& new_path);

  // Split out from CurrentSpeed to enable testing.
  int64 CurrentSpeedAtTime(base::TimeTicks current_time) const;

  // Log a TYPE_DOWNLOAD_FILE_ERROR NetLog event with |error| and passes error
  // on through, converting to a |DownloadInterruptReason|.
  DownloadInterruptReason LogNetError(const char* operation, net::Error error);

  // Log the system error in |os_error| and converts it into a
  // |DownloadInterruptReason|.
  DownloadInterruptReason LogSystemError(const char* operation, int os_error);

  // Log a TYPE_DOWNLOAD_FILE_ERROR NetLog event with |os_error| and |reason|.
  // Returns |reason|.
  DownloadInterruptReason LogInterruptReason(
      const char* operation, int os_error,
      DownloadInterruptReason reason);

  static const size_t kSha256HashLen = 32;
  static const unsigned char kEmptySha256Hash[kSha256HashLen];

  // Full path to the file including the file name.
  base::FilePath full_path_;

  // Source URL for the file being downloaded.
  GURL source_url_;

  // The URL where the download was initiated.
  GURL referrer_url_;

  // OS file stream for writing
  scoped_ptr<net::FileStream> file_stream_;

  // Amount of data received up so far, in bytes.
  int64 bytes_so_far_;

  // Start time for calculating speed.
  base::TimeTicks start_tick_;

  // Indicates if hash should be calculated for the file.
  bool calculate_hash_;

  // Used to calculate hash for the file when calculate_hash_
  // is set.
  scoped_ptr<crypto::SecureHash> secure_hash_;

  unsigned char sha256_hash_[kSha256HashLen];

  // Indicates that this class no longer owns the associated file, and so
  // won't delete it on destruction.
  bool detached_;

  net::BoundNetLog bound_net_log_;

  DISALLOW_COPY_AND_ASSIGN(BaseFile);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_BASE_FILE_H_
