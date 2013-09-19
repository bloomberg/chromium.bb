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

const int kBurningBlockSize = 8 * 1024;  // 8 KiB

void Operation::WriteStart() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  if (IsCancelled()) {
    return;
  }

  if (image_path_.empty()) {
    Error(error::kImageNotFound);
    return;
  }

  DVLOG(1) << "Starting write of " << image_path_.value()
           << " to " << storage_unit_id_;

  stage_ = image_writer_api::STAGE_WRITE;
  progress_ = 0;
  SendProgress();

  // TODO (haven): Unmount partitions before writing. http://crbug.com/284834

  scoped_ptr<image_writer_utils::ImageReader> reader(
      new image_writer_utils::ImageReader());
  scoped_ptr<image_writer_utils::ImageWriter> writer(
      new image_writer_utils::ImageWriter());
  base::FilePath storage_path(storage_unit_id_);

  if (reader->Open(image_path_)) {
    if (!writer->Open(storage_path)) {
      reader->Close();
      Error(error::kOpenDevice);
      return;
    }
  } else {
    Error(error::kOpenImage);
    return;
  }

  BrowserThread::PostTask(
    BrowserThread::FILE,
    FROM_HERE,
    base::Bind(&Operation::WriteChunk,
               this,
               base::Passed(&reader),
               base::Passed(&writer),
               0));
}

void Operation::WriteChunk(
    scoped_ptr<image_writer_utils::ImageReader> reader,
    scoped_ptr<image_writer_utils::ImageWriter> writer,
    int64 bytes_written) {
  if (IsCancelled()) {
    WriteCleanup(reader.Pass(), writer.Pass());
    return;
  }

  char buffer[kBurningBlockSize];
  int64 image_size = reader->GetSize();
  int len = reader->Read(buffer, kBurningBlockSize);

  if (len > 0) {
    if (writer->Write(buffer, len) == len) {
      int percent_prev = bytes_written * 100 / image_size;
      int percent_curr = (bytes_written + len) * 100 / image_size;
      progress_ = (bytes_written + len) * 100 / image_size;

      if (percent_curr > percent_prev) {
        SendProgress();
      }

      BrowserThread::PostTask(
        BrowserThread::FILE,
        FROM_HERE,
        base::Bind(&Operation::WriteChunk,
                   this,
                   base::Passed(&reader),
                   base::Passed(&writer),
                   bytes_written + len));
    } else {
      WriteCleanup(reader.Pass(), writer.Pass());
      Error(error::kWriteImage);
    }
  } else if (len == 0) {
    if (bytes_written == image_size) {
      if (WriteCleanup(reader.Pass(), writer.Pass())) {
        BrowserThread::PostTask(
          BrowserThread::FILE,
          FROM_HERE,
          base::Bind(&Operation::WriteComplete,
                     this));
      }
    } else {
      WriteCleanup(reader.Pass(), writer.Pass());
      Error(error::kPrematureEndOfFile);
    }
  } else { // len < 0
    WriteCleanup(reader.Pass(), writer.Pass());
    Error(error::kReadImage);
  }
}

bool Operation::WriteCleanup(
    scoped_ptr<image_writer_utils::ImageReader> reader,
    scoped_ptr<image_writer_utils::ImageWriter> writer) {

  bool success = true;
  if (!reader->Close()) {
    Error(error::kCloseImage);
    success = false;
  }

  if (!writer->Close()) {
    Error(error::kCloseDevice);
    success = false;
  }
  return success;
}

void Operation::WriteComplete() {

  DVLOG(2) << "Completed write of " << image_path_.value();
  progress_ = 100;
  SendProgress();

  BrowserThread::PostTask(
    BrowserThread::FILE,
    FROM_HERE,
    base::Bind(&Operation::VerifyWriteStart,
               this));
}

void Operation::VerifyWriteStart() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  if (IsCancelled()) {
    return;
  }

  DVLOG(1) << "Starting verification stage.";

  stage_ = image_writer_api::STAGE_VERIFYWRITE;
  progress_ = 0;
  SendProgress();

  scoped_ptr<base::FilePath> image_path(new base::FilePath(image_path_));

  GetMD5SumOfFile(
      image_path.Pass(),
      -1,
      0, // progress_offset
      50, // progress_scale
      base::Bind(&Operation::VerifyWriteStage2,
                 this));
}

void Operation::VerifyWriteStage2(
    scoped_ptr<std::string> image_hash) {
  DVLOG(1) << "Building MD5 sum of device: " << storage_unit_id_;

  int64 image_size;
  scoped_ptr<base::FilePath> device_path(new base::FilePath(storage_unit_id_));

  if (!file_util::GetFileSize(image_path_, &image_size)){
    Error(error::kImageSize);
    return;
  }

  GetMD5SumOfFile(
      device_path.Pass(),
      image_size,
      50, // progress_offset
      50, // progress_scale
      base::Bind(&Operation::VerifyWriteCompare,
                 this,
                 base::Passed(&image_hash)));
}

void Operation::VerifyWriteCompare(
    scoped_ptr<std::string> image_hash,
    scoped_ptr<std::string> device_hash) {
  DVLOG(1) << "Comparing hashes: " << *image_hash << " vs " << *device_hash;

  if (*image_hash != *device_hash) {
    Error(error::kWriteHash);
    return;
  }

  DVLOG(2) << "Completed write verification of " << image_path_.value();

  progress_ = 100;
  SendProgress();
  Finish();
}

} // namespace image_writer
} // namespace extensions
