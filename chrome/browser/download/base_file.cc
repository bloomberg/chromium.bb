// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/base_file.h"

#include "base/crypto/secure_hash.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "net/base/file_stream.h"
#include "net/base/net_errors.h"
#include "chrome/browser/download/download_util.h"
#include "content/browser/browser_thread.h"

#if defined(OS_WIN)
#include "chrome/common/win_safe_util.h"
#elif defined(OS_MACOSX)
#include "chrome/browser/ui/cocoa/file_metadata.h"
#endif

BaseFile::BaseFile(const FilePath& full_path,
                   const GURL& source_url,
                   const GURL& referrer_url,
                   int64 received_bytes,
                   const linked_ptr<net::FileStream>& file_stream)
    : full_path_(full_path),
      source_url_(source_url),
      referrer_url_(referrer_url),
      file_stream_(file_stream),
      bytes_so_far_(received_bytes),
      power_save_blocker_(true),
      calculate_hash_(false) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
}

BaseFile::~BaseFile() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  if (in_progress())
    Cancel();
  Close();
}

bool BaseFile::Initialize(bool calculate_hash) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  calculate_hash_ = calculate_hash;

  if (calculate_hash_)
    secure_hash_.reset(base::SecureHash::Create(base::SecureHash::SHA256));

  if (!full_path_.empty() ||
      download_util::CreateTemporaryFileForDownload(&full_path_))
    return Open();
  return false;
}

bool BaseFile::AppendDataToFile(const char* data, size_t data_len) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  if (!file_stream_.get())
    return false;

  // TODO(phajdan.jr): get rid of this check.
  if (data_len == 0)
    return true;

  bytes_so_far_ += data_len;

  // TODO(phajdan.jr): handle errors on file writes. http://crbug.com/58355
  size_t written = file_stream_->Write(data, data_len, NULL);
  if (written != data_len)
    return false;

  if (calculate_hash_)
    secure_hash_->Update(data, data_len);

  return true;
}

bool BaseFile::Rename(const FilePath& new_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  // Save the information whether the download is in progress because
  // it will be overwritten by closing the file.
  bool saved_in_progress = in_progress();

  // If the new path is same as the old one, there is no need to perform the
  // following renaming logic.
  if (new_path == full_path_) {
    // Don't close the file if we're not done (finished or canceled).
    if (!saved_in_progress)
      Close();

    return true;
  }

  Close();

  file_util::CreateDirectory(new_path.DirName());

#if defined(OS_WIN)
  // We cannot rename because rename will keep the same security descriptor
  // on the destination file. We want to recreate the security descriptor
  // with the security that makes sense in the new path.
  if (!file_util::RenameFileAndResetSecurityDescriptor(full_path_, new_path))
    return false;
#elif defined(OS_POSIX)
  {
    // Similarly, on Unix, we're moving a temp file created with permissions
    // 600 to |new_path|. Here, we try to fix up the destination file with
    // appropriate permissions.
    struct stat st;
    // First check the file existence and create an empty file if it doesn't
    // exist.
    if (!file_util::PathExists(new_path))
      file_util::WriteFile(new_path, "", 0);
    bool stat_succeeded = (stat(new_path.value().c_str(), &st) == 0);

    // TODO(estade): Move() falls back to copying and deleting when a simple
    // rename fails. Copying sucks for large downloads. crbug.com/8737
    if (!file_util::Move(full_path_, new_path))
      return false;

    if (stat_succeeded)
      chmod(new_path.value().c_str(), st.st_mode);
  }
#endif

  full_path_ = new_path;

  // We don't need to re-open the file if we're done (finished or canceled).
  if (!saved_in_progress)
    return true;

  if (!Open())
    return false;

  return true;
}

void BaseFile::Cancel() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  Close();
  if (!full_path_.empty())
    file_util::Delete(full_path_, false);
}

void BaseFile::Finish() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  if (calculate_hash_)
    secure_hash_->Finish(sha256_hash_, kSha256HashLen);

  Close();
}

bool BaseFile::GetSha256Hash(std::string* hash) {
  if (!calculate_hash_ || in_progress())
    return false;
  hash->assign(reinterpret_cast<const char*>(sha256_hash_),
               sizeof(sha256_hash_));
  return true;
}

void BaseFile::AnnotateWithSourceInformation() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
#if defined(OS_WIN)
  // Sets the Zone to tell Windows that this file comes from the internet.
  // We ignore the return value because a failure is not fatal.
  win_util::SetInternetZoneIdentifier(full_path_);
#elif defined(OS_MACOSX)
  file_metadata::AddQuarantineMetadataToFile(full_path_, source_url_,
                                             referrer_url_);
  file_metadata::AddOriginMetadataToFile(full_path_, source_url_,
                                         referrer_url_);
#endif
}

bool BaseFile::Open() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(!full_path_.empty());

  // Create a new file steram if it is not provided.
  if (!file_stream_.get()) {
    file_stream_.reset(new net::FileStream);
    if (file_stream_->Open(full_path_,
                           base::PLATFORM_FILE_OPEN_ALWAYS |
                               base::PLATFORM_FILE_WRITE) != net::OK) {
      file_stream_.reset();
      return false;
    }

    // We may be re-opening the file after rename. Always make sure we're
    // writing at the end of the file.
    if (file_stream_->Seek(net::FROM_END, 0) < 0) {
      file_stream_.reset();
      return false;
    }
  }

#if defined(OS_WIN)
  AnnotateWithSourceInformation();
#endif
  return true;
}

void BaseFile::Close() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  if (file_stream_.get()) {
#if defined(OS_CHROMEOS)
    // Currently we don't really care about the return value, since if it fails
    // theres not much we can do.  But we might in the future.
    file_stream_->Flush();
#endif
    file_stream_->Close();
    file_stream_.reset();
  }
}

std::string BaseFile::DebugString() const {
  return base::StringPrintf("{ source_url_ = \"%s\" full_path_ = \"%s\" }",
                            source_url_.spec().c_str(),
                            full_path_.value().c_str());
}
