// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_file.h"

#include "base/file_util.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/history/download_types.h"
#include "net/base/net_errors.h"

#if defined(OS_WIN)
#include "app/win_util.h"
#include "chrome/common/win_safe_util.h"
#elif defined(OS_MACOSX)
#include "chrome/browser/cocoa/file_metadata.h"
#endif

DownloadFile::DownloadFile(const DownloadCreateInfo* info)
    : file_stream_(info->save_info.file_stream),
      source_url_(info->url),
      referrer_url_(info->referrer_url),
      id_(info->download_id),
      child_id_(info->child_id),
      request_id_(info->request_id),
      full_path_(info->save_info.file_path),
      path_renamed_(false),
      dont_sleep_(true) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
}

DownloadFile::~DownloadFile() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
  Close();
}

bool DownloadFile::Initialize() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
  if (!full_path_.empty() ||
      download_util::CreateTemporaryFileForDownload(&full_path_))
    return Open();
  return false;
}

bool DownloadFile::AppendDataToFile(const char* data, size_t data_len) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));

  if (!file_stream_.get())
    return false;

  // FIXME bug 595247: handle errors on file writes.
  size_t written = file_stream_->Write(data, data_len, NULL);
  return (written == data_len);
}

void DownloadFile::Cancel() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
  Close();
  if (!full_path_.empty())
    file_util::Delete(full_path_, false);
}

// The UI has provided us with our finalized name.
bool DownloadFile::Rename(const FilePath& new_path, bool is_final_rename) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));

  // Save the information whether the download is in progress because
  // it will be overwritten by closing the file.
  bool saved_in_progress = in_progress();

  // If the new path is same as the old one, there is no need to perform the
  // following renaming logic.
  if (new_path == full_path_) {
    path_renamed_ = is_final_rename;

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
  path_renamed_ = is_final_rename;

  // We don't need to re-open the file if we're done (finished or canceled).
  if (!saved_in_progress)
    return true;

  if (!Open())
    return false;

  // Move to the end of the new file.
  if (file_stream_->Seek(net::FROM_END, 0) < 0)
    return false;

  return true;
}

void DownloadFile::DeleteCrDownload() {
  FilePath crdownload = download_util::GetCrDownloadPath(full_path_);
  file_util::Delete(crdownload, false);
}

void DownloadFile::Finish() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
  Close();
}

void DownloadFile::Close() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
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

bool DownloadFile::Open() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
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
  }

#if defined(OS_WIN)
  AnnotateWithSourceInformation();
#endif
  return true;
}

void DownloadFile::AnnotateWithSourceInformation() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
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

void DownloadFile::CancelDownloadRequest(ResourceDispatcherHost* rdh) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableFunction(&download_util::CancelDownloadRequest,
                          rdh,
                          child_id_,
                          request_id_));
}
