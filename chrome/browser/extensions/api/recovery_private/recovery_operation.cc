// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/files/file_enumerator.h"
#include "base/threading/worker_pool.h"
#include "chrome/browser/extensions/api/recovery_private/error_messages.h"
#include "chrome/browser/extensions/api/recovery_private/recovery_operation.h"
#include "chrome/browser/extensions/api/recovery_private/recovery_operation_manager.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/zlib/google/zip.h"

namespace extensions {
namespace recovery {

using content::BrowserThread;

const int kBurningBlockSize = 8 * 1024;  // 8 KiB
const int kMD5BufferSize = 1024;

RecoveryOperation::RecoveryOperation(RecoveryOperationManager* manager,
                                     const ExtensionId& extension_id,
                                     const std::string& storage_unit_id)
    : manager_(manager),
      extension_id_(extension_id),
      storage_unit_id_(storage_unit_id) {
}

RecoveryOperation::~RecoveryOperation() {
}

void RecoveryOperation::Cancel() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  DVLOG(1) << "Cancelling recovery operation for ext: " << extension_id_;

  stage_ = recovery_api::STAGE_NONE;
}

void RecoveryOperation::Abort() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  Error(error::kAborted);
}

void RecoveryOperation::Error(const std::string& error_message) {
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&RecoveryOperationManager::OnError,
                 manager_->AsWeakPtr(),
                 extension_id_,
                 stage_,
                 progress_,
                 error_message));
}

void RecoveryOperation::SendProgress() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&RecoveryOperationManager::OnProgress,
                 manager_->AsWeakPtr(),
                 extension_id_,
                 stage_,
                 progress_));
}

void RecoveryOperation::Finish() {
  DVLOG(1) << "Write operation complete.";

  stage_ = recovery_api::STAGE_NONE;
  progress_ = 0;

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&RecoveryOperationManager::OnComplete,
                 manager_->AsWeakPtr(),
                 extension_id_));
}

bool RecoveryOperation::IsCancelled() {
  return stage_ == recovery_api::STAGE_NONE;
}

void RecoveryOperation::UnzipStart(scoped_ptr<base::FilePath> zip_file) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  if (IsCancelled()) {
    return;
  }

  DVLOG(1) << "Starting unzip stage for " << zip_file->value();

  stage_ = recovery_api::STAGE_UNZIP;
  progress_ = 0;

  SendProgress();

  base::FilePath tmp_dir;
  if (!file_util::CreateNewTempDirectory(FILE_PATH_LITERAL(""), &tmp_dir)) {
    Error(error::kTempDir);
    return;
  }

  if (!zip::Unzip(*zip_file, tmp_dir)) {
    Error(error::kUnzip);
    return;
  }

  base::FileEnumerator file_enumerator(tmp_dir,
                                       false,
                                       base::FileEnumerator::FILES);

  scoped_ptr<base::FilePath> unzipped_file(
      new base::FilePath(file_enumerator.Next()));

  if (unzipped_file->empty()) {
    Error(error::kEmptyUnzip);
    return;
  }

  if (!file_enumerator.Next().empty()) {
    Error(error::kMultiFileZip);
    return;
  }

  DVLOG(1) << "Successfully unzipped as " << unzipped_file->value();

  progress_ = 100;
  SendProgress();

  image_path_ = *unzipped_file;

  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&RecoveryOperation::WriteStart,
                 this));
}

void RecoveryOperation::GetMD5SumOfFile(
    scoped_ptr<base::FilePath> file_path,
    int64 file_size,
    int progress_offset,
    int progress_scale,
    const base::Callback<void(scoped_ptr<std::string>)>& callback) {
  if (IsCancelled()) {
    return;
  }

  base::MD5Init(&md5_context_);
  scoped_ptr<recovery_utils::ImageReader> reader(
      new recovery_utils::ImageReader());

  if (!reader->Open(*file_path)) {
    Error(error::kOpenImage);
    return;
  }
  if (file_size <= 0) {
    file_size = reader->GetSize();
  }

  BrowserThread::PostTask(BrowserThread::FILE,
                          FROM_HERE,
                          base::Bind(&RecoveryOperation::MD5Chunk,
                                     this,
                                     base::Passed(&reader),
                                     0,
                                     file_size,
                                     progress_offset,
                                     progress_scale,
                                     callback));
}

void RecoveryOperation::MD5Chunk(
    scoped_ptr<recovery_utils::ImageReader> reader,
    int64 bytes_processed,
    int64 bytes_total,
    int progress_offset,
    int progress_scale,
    const base::Callback<void(scoped_ptr<std::string>)>& callback) {
  if (IsCancelled()) {
    reader->Close();
    return;
  }

  char buffer[kMD5BufferSize];
  int len;

  if (bytes_total - bytes_processed <= kMD5BufferSize) {
    len = reader->Read(buffer, bytes_total - bytes_processed);
  } else {
    len = reader->Read(buffer, kMD5BufferSize);
  }

  if (len > 0) {
    base::MD5Update(&md5_context_, base::StringPiece(buffer, len));
    int percent_prev = (bytes_processed * progress_scale + progress_offset)
                       / (bytes_total);
    int percent_curr = ((bytes_processed + len) * progress_scale
                        + progress_offset) / (bytes_total);
    progress_ = percent_curr;

    if (percent_curr > percent_prev) {
      SendProgress();
    }

    BrowserThread::PostTask(BrowserThread::FILE,
                            FROM_HERE,
                            base::Bind(&RecoveryOperation::MD5Chunk,
                                       this,
                                       base::Passed(&reader),
                                       bytes_processed + len,
                                       bytes_total,
                                       progress_offset,
                                       progress_scale,
                                       callback));
  } else if (len == 0) {
    if (bytes_processed + len < bytes_total) {
      reader->Close();
      Error(error::kPrematureEndOfFile);
    } else {
      base::MD5Digest digest;
      base::MD5Final(&digest, &md5_context_);
      scoped_ptr<std::string> hash(
          new std::string(base::MD5DigestToBase16(digest)));
      callback.Run(hash.Pass());
    }
  } else {  // len < 0
    reader->Close();
    Error(error::kReadImage);
  }
}

} // namespace recovery
} // namespace extensions
