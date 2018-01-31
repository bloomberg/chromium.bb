// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_SAVE_INFO_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_SAVE_INFO_H_

#include <stdint.h>

#include <memory>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "components/download/public/common/download_export.h"
#include "crypto/secure_hash.h"

namespace download {

// Holds the information about how to save a download file.
// In the case of download continuation, |file_path| is set to the current file
// name, |offset| is set to the point where we left off, and |hash_state| will
// hold the state of the hash algorithm where we left off.
struct COMPONENTS_DOWNLOAD_EXPORT DownloadSaveInfo {
  // The default value for |length|. Used when request the rest of the file
  // starts from |offset|.
  static const int64_t kLengthFullContent;

  DownloadSaveInfo();
  ~DownloadSaveInfo();
  DownloadSaveInfo(DownloadSaveInfo&& that);

  // If non-empty, contains the full target path of the download that has been
  // determined prior to download initiation. This is considered to be a trusted
  // path.
  base::FilePath file_path;

  // If non-empty, contains an untrusted filename suggestion. This can't contain
  // a path (only a filename), and is only effective if |file_path| is empty.
  base::string16 suggested_name;

  // If valid, contains the source data stream for the file contents.
  base::File file;

  // The file offset at which to start the download.  May be 0.
  int64_t offset;

  // The number of the bytes to download from |offset|. Set to
  // |kLengthFullContent| by default.
  // Ask to retrieve segment of the download file when length is greater than 0.
  // Request the rest of the file starting from |offset|, when length is
  // |kLengthFullContent|.
  int64_t length;

  // The state of the hash. If specified, this hash state must indicate the
  // state of the partial file for the first |offset| bytes.
  std::unique_ptr<crypto::SecureHash> hash_state;

  // SHA-256 hash of the first |offset| bytes of the file. Only used if |offset|
  // is non-zero and either |file_path| or |file| specifies the file which
  // contains the |offset| number of bytes. Can be empty, in which case no
  // verification is done on the existing file.
  std::string hash_of_partial_file;

  // If |prompt_for_save_location| is true, and |file_path| is empty, then
  // the user will be prompted for a location to save the download. Otherwise,
  // the location will be determined automatically using |file_path| as a
  // basis if |file_path| is not empty.
  // |prompt_for_save_location| defaults to false.
  bool prompt_for_save_location;

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadSaveInfo);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_SAVE_INFO_H_
