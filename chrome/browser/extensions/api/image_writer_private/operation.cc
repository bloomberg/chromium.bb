// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/files/file_enumerator.h"
#include "base/threading/worker_pool.h"
#include "chrome/browser/extensions/api/image_writer_private/error_messages.h"
#include "chrome/browser/extensions/api/image_writer_private/operation.h"
#include "chrome/browser/extensions/api/image_writer_private/operation_manager.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/zlib/google/zip.h"

namespace extensions {
namespace image_writer {

using content::BrowserThread;

namespace {

const int kMD5BufferSize = 1024;

void RemoveTempDirectory(const base::FilePath path) {
  base::DeleteFile(path, true);
}

}  // namespace

Operation::Operation(base::WeakPtr<OperationManager> manager,
                     const ExtensionId& extension_id,
                     const std::string& storage_unit_id)
    : manager_(manager),
      extension_id_(extension_id),
      storage_unit_id_(storage_unit_id) {
}

Operation::~Operation() {
}

void Operation::Cancel() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  DVLOG(1) << "Cancelling image writing operation for ext: " << extension_id_;

  stage_ = image_writer_api::STAGE_NONE;

  CleanUp();
}

void Operation::Abort() {
  Error(error::kAborted);
}

void Operation::Error(const std::string& error_message) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::FILE)) {
    BrowserThread::PostTask(BrowserThread::FILE,
                            FROM_HERE,
                            base::Bind(&Operation::Error, this, error_message));
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&OperationManager::OnError,
                 manager_,
                 extension_id_,
                 stage_,
                 progress_,
                 error_message));

  CleanUp();
}

void Operation::SetProgress(int progress) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::FILE)) {
    BrowserThread::PostTask(
        BrowserThread::FILE,
        FROM_HERE,
        base::Bind(&Operation::SetProgress,
                   this,
                   progress));
    return;
  }

  if (IsCancelled()) {
    return;
  }

  progress_ = progress;

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&OperationManager::OnProgress,
                 manager_,
                 extension_id_,
                 stage_,
                 progress_));
}

void Operation::SetStage(image_writer_api::Stage stage) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::FILE)) {
    BrowserThread::PostTask(
        BrowserThread::FILE,
        FROM_HERE,
        base::Bind(&Operation::SetStage,
                   this,
                   stage));
    return;
  }

  if (IsCancelled()) {
    return;
  }

  stage_ = stage;
  progress_ = 0;

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&OperationManager::OnProgress,
                 manager_,
                 extension_id_,
                 stage_,
                 progress_));
}

void Operation::Finish() {
  if (BrowserThread::CurrentlyOn(BrowserThread::FILE)) {
    BrowserThread::PostTask(BrowserThread::FILE,
                            FROM_HERE,
                            base::Bind(&Operation::Finish, this));
  }
  DVLOG(1) << "Write operation complete.";

  CleanUp();

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&OperationManager::OnComplete,
                 manager_,
                 extension_id_));
}

bool Operation::IsCancelled() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  return stage_ == image_writer_api::STAGE_NONE;
}

void Operation::AddCleanUpFunction(base::Closure callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  cleanup_functions_.push_back(callback);
}

void Operation::CleanUp() {
  for (std::vector<base::Closure>::iterator it = cleanup_functions_.begin();
       it != cleanup_functions_.end();
       ++it) {
    it->Run();
  }
  cleanup_functions_.clear();
}

void Operation::UnzipStart(scoped_ptr<base::FilePath> zip_file) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  if (IsCancelled()) {
    return;
  }

  DVLOG(1) << "Starting unzip stage for " << zip_file->value();

  SetStage(image_writer_api::STAGE_UNZIP);

  base::FilePath tmp_dir;
  if (!file_util::CreateTemporaryDirInDir(zip_file->DirName(),
                                          FILE_PATH_LITERAL("image_writer"),
                                          &tmp_dir)) {
    Error(error::kTempDir);
    return;
  }

  AddCleanUpFunction(base::Bind(&RemoveTempDirectory, tmp_dir));

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

  SetProgress(kProgressComplete);

  image_path_ = *unzipped_file;

  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&Operation::WriteStart,
                 this));
}

void Operation::GetMD5SumOfFile(
    scoped_ptr<base::FilePath> file_path,
    int64 file_size,
    int progress_offset,
    int progress_scale,
    const base::Callback<void(scoped_ptr<std::string>)>& callback) {
  if (IsCancelled()) {
    return;
  }

  base::MD5Init(&md5_context_);
  scoped_ptr<image_writer_utils::ImageReader> reader(
      new image_writer_utils::ImageReader());

  if (!reader->Open(*file_path)) {
    Error(error::kOpenImage);
    return;
  }
  if (file_size <= 0) {
    file_size = reader->GetSize();
  }

  BrowserThread::PostTask(BrowserThread::FILE,
                          FROM_HERE,
                          base::Bind(&Operation::MD5Chunk,
                                     this,
                                     base::Passed(&reader),
                                     0,
                                     file_size,
                                     progress_offset,
                                     progress_scale,
                                     callback));
}

void Operation::MD5Chunk(
    scoped_ptr<image_writer_utils::ImageReader> reader,
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
    int percent_prev = (bytes_processed * progress_scale + progress_offset) /
                       (bytes_total);
    int percent_curr = ((bytes_processed + len) * progress_scale +
                        progress_offset) /
                       (bytes_total);
    if (percent_curr > percent_prev) {
      SetProgress(progress_);
    }

    BrowserThread::PostTask(BrowserThread::FILE,
                            FROM_HERE,
                            base::Bind(&Operation::MD5Chunk,
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

}  // namespace image_writer
}  // namespace extensions
