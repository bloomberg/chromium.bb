// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_BASE_FILE_H_
#define CHROME_BROWSER_DOWNLOAD_BASE_FILE_H_
#pragma once

#include <string>

#include "base/file_path.h"
#include "base/linked_ptr.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/power_save_blocker.h"
#include "googleurl/src/gurl.h"

namespace base {
class SecureHash;
}
namespace net {
class FileStream;
}

// File being downloaded and saved to disk. This is a base class
// for DownloadFile and SaveFile, which keep more state information.
class BaseFile {
 public:
  BaseFile(const FilePath& full_path,
           const GURL& source_url,
           const GURL& referrer_url,
           int64 received_bytes,
           const linked_ptr<net::FileStream>& file_stream);
  ~BaseFile();

  // If calculate_hash is true, sha256 hash will be calculated.
  bool Initialize(bool calculate_hash);

  // Write a new chunk of data to the file. Returns true on success (all bytes
  // written to the file).
  bool AppendDataToFile(const char* data, size_t data_len);

  // Rename the download file. Returns true on success.
  virtual bool Rename(const FilePath& full_path);

  // Abort the download and automatically close the file.
  void Cancel();

  // Indicate that the download has finished. No new data will be received.
  void Finish();

  // Informs the OS that this file came from the internet.
  void AnnotateWithSourceInformation();

  FilePath full_path() const { return full_path_; }
  bool in_progress() const { return file_stream_ != NULL; }
  int64 bytes_so_far() const { return bytes_so_far_; }

  // Set |hash| with sha256 digest for the file.
  // Returns true if digest is successfully calculated.
  virtual bool GetSha256Hash(std::string* hash);

  virtual std::string DebugString() const;

 protected:
  bool Open();
  void Close();

  // Full path to the file including the file name.
  FilePath full_path_;

 private:
  static const size_t kSha256HashLen = 32;

  // Source URL for the file being downloaded.
  GURL source_url_;

  // The URL where the download was initiated.
  GURL referrer_url_;

  // OS file stream for writing
  linked_ptr<net::FileStream> file_stream_;

  // Amount of data received up so far, in bytes.
  int64 bytes_so_far_;

  // RAII handle to keep the system from sleeping while we're downloading.
  PowerSaveBlocker power_save_blocker_;

  // Indicates if sha256 hash should be calculated for the file.
  bool calculate_hash_;

  // Used to calculate sha256 hash for the file when calculate_hash_
  // is set.
  scoped_ptr<base::SecureHash> secure_hash_;

  unsigned char sha256_hash_[kSha256HashLen];

  DISALLOW_COPY_AND_ASSIGN(BaseFile);
};

#endif  // CHROME_BROWSER_DOWNLOAD_BASE_FILE_H_
