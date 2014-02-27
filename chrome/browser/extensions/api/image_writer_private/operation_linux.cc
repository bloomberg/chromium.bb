// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/files/file_enumerator.h"
#include "base/platform_file.h"
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

void Operation::Write(const base::Closure& continuation) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  if (IsCancelled()) {
    return;
  }

  SetStage(image_writer_api::STAGE_WRITE);

  // TODO (haven): Unmount partitions before writing. http://crbug.com/284834

  base::PlatformFileError result;
  image_file_ = base::CreatePlatformFile(
      image_path_,
      base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ,
      NULL,
      &result);
  if (result != base::PLATFORM_FILE_OK) {
    Error(error::kImageOpenError);
    return;
  }

  device_file_ = base::CreatePlatformFile(
      device_path_,
      base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_WRITE,
      NULL,
      &result);
  if (result != base::PLATFORM_FILE_OK) {
    Error(error::kDeviceOpenError);
    base::ClosePlatformFile(image_file_);
    return;
  }

  int64 total_size;
  base::GetFileSize(image_path_, &total_size);

  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&Operation::WriteChunk, this, 0, total_size, continuation));
}

void Operation::WriteChunk(const int64& bytes_written,
                           const int64& total_size,
                           const base::Closure& continuation) {
  if (!IsCancelled()) {
    scoped_ptr<char[]> buffer(new char[kBurningBlockSize]);
    int64 len = base::ReadPlatformFile(
        image_file_, bytes_written, buffer.get(), kBurningBlockSize);

    if (len > 0) {
      if (base::WritePlatformFile(
              device_file_, bytes_written, buffer.get(), len) == len) {
        int percent_curr =
            kProgressComplete * (bytes_written + len) / total_size;

        SetProgress(percent_curr);

        BrowserThread::PostTask(BrowserThread::FILE,
                                FROM_HERE,
                                base::Bind(&Operation::WriteChunk,
                                           this,
                                           bytes_written + len,
                                           total_size,
                                           continuation));
        return;
      } else {
        Error(error::kDeviceWriteError);
      }
    } else if (len == 0) {
      WriteComplete(continuation);
    } else {  // len < 0
      Error(error::kImageReadError);
    }
  }

  base::ClosePlatformFile(image_file_);
  base::ClosePlatformFile(device_file_);
}

void Operation::WriteComplete(const base::Closure& continuation) {
  SetProgress(kProgressComplete);
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE, continuation);
}

void Operation::VerifyWrite(const base::Closure& continuation) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  if (IsCancelled()) {
    return;
  }

  SetStage(image_writer_api::STAGE_VERIFYWRITE);

  base::PlatformFileError result;

  image_file_ = base::CreatePlatformFile(
      image_path_,
      base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ,
      NULL,
      &result);
  if (result != base::PLATFORM_FILE_OK) {
    Error(error::kImageOpenError);
    return;
  }

  device_file_ = base::CreatePlatformFile(
      device_path_,
      base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ,
      NULL,
      &result);
  if (result != base::PLATFORM_FILE_OK) {
    Error(error::kDeviceOpenError);
    base::ClosePlatformFile(image_file_);
    return;
  }

  int64 total_size;
  base::GetFileSize(image_path_, &total_size);

  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(
          &Operation::VerifyWriteChunk, this, 0, total_size, continuation));
}

void Operation::VerifyWriteChunk(const int64& bytes_processed,
                                 const int64& total_size,
                                 const base::Closure& continuation) {
  if (!IsCancelled()) {
    scoped_ptr<char[]> source_buffer(new char[kBurningBlockSize]);
    scoped_ptr<char[]> target_buffer(new char[kBurningBlockSize]);

    int64 image_bytes_read = base::ReadPlatformFile(
        image_file_, bytes_processed, source_buffer.get(), kBurningBlockSize);

    if (image_bytes_read > 0) {
      int64 device_bytes_read = base::ReadPlatformFile(
          device_file_, bytes_processed, target_buffer.get(), image_bytes_read);
      if (image_bytes_read == device_bytes_read &&
          memcmp(source_buffer.get(), target_buffer.get(), image_bytes_read) ==
              0) {
        int percent_curr = kProgressComplete *
                           (bytes_processed + image_bytes_read) / total_size;

        SetProgress(percent_curr);

        BrowserThread::PostTask(BrowserThread::FILE,
                                FROM_HERE,
                                base::Bind(&Operation::VerifyWriteChunk,
                                           this,
                                           bytes_processed + image_bytes_read,
                                           total_size,
                                           continuation));
        return;
      } else {
        Error(error::kVerificationFailed);
      }
    } else if (image_bytes_read == 0) {
      VerifyWriteComplete(continuation);
    } else {  // len < 0
      Error(error::kImageReadError);
    }
  }

  base::ClosePlatformFile(image_file_);
  base::ClosePlatformFile(device_file_);
}

void Operation::VerifyWriteComplete(const base::Closure& continuation) {
  SetProgress(kProgressComplete);
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE, continuation);
}

}  // namespace image_writer
}  // namespace extensions
