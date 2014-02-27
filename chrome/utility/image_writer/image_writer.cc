// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/platform_file.h"
#include "chrome/utility/image_writer/error_messages.h"
#include "chrome/utility/image_writer/image_writer.h"
#include "chrome/utility/image_writer/image_writer_handler.h"
#include "content/public/utility/utility_thread.h"

namespace image_writer {

// Since block devices like large sequential access and IPC is expensive we're
// doing work in 1MB chunks.
const int kBurningBlockSize = 1 << 20;

ImageWriter::ImageWriter(ImageWriterHandler* handler)
    : image_file_(base::kInvalidPlatformFileValue),
      device_file_(base::kInvalidPlatformFileValue),
      bytes_processed_(0),
      handler_(handler) {}

ImageWriter::~ImageWriter() { CleanUp(); }

void ImageWriter::Write(const base::FilePath& image_path,
                        const base::FilePath& device_path) {
  if (IsRunning()) {
    handler_->SendFailed(error::kOperationAlreadyInProgress);
    return;
  }

  image_path_ = image_path;
  device_path_ = device_path;
  bytes_processed_ = 0;

  base::PlatformFileError error;

  image_file_ = base::CreatePlatformFile(
      image_path_,
      base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ,
      NULL,
      &error);

  if (error != base::PLATFORM_FILE_OK) {
    DLOG(ERROR) << "Unable to open file for read: " << image_path_.value();
    Error(error::kOpenImage);
    return;
  }

#if defined(OS_WIN)
  // Windows requires that device files be opened with FILE_FLAG_NO_BUFFERING
  // and FILE_FLAG_WRITE_THROUGH.  These two flags are not part of
  // PlatformFile::CreatePlatformFile.
  device_file_ = CreateFile(device_path.value().c_str(),
                            GENERIC_WRITE,
                            FILE_SHARE_WRITE,
                            NULL,
                            OPEN_EXISTING,
                            FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH,
                            INVALID_HANDLE_VALUE);

  if (device_file_ == base::kInvalidPlatformFileValue) {
    Error(error::kOpenDevice);
    return;
  }
#else
  device_file_ = base::CreatePlatformFile(
      device_path_,
      base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_WRITE,
      NULL,
      &error);

  if (error != base::PLATFORM_FILE_OK) {
    DLOG(ERROR) << "Unable to open file for write(" << error
                << "): " << device_path_.value();
    Error(error::kOpenDevice);
    return;
  }
#endif

  PostProgress(0);

  PostTask(base::Bind(&ImageWriter::WriteChunk, AsWeakPtr()));
}

void ImageWriter::Verify(const base::FilePath& image_path,
                         const base::FilePath& device_path) {
  if (IsRunning()) {
    handler_->SendFailed(error::kOperationAlreadyInProgress);
    return;
  }

  image_path_ = image_path;
  device_path_ = device_path;
  bytes_processed_ = 0;

  base::PlatformFileError error;

  image_file_ = base::CreatePlatformFile(
      image_path_,
      base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ,
      NULL,
      &error);

  if (error != base::PLATFORM_FILE_OK) {
    DLOG(ERROR) << "Unable to open file for read: " << image_path_.value();
    Error(error::kOpenImage);
    return;
  }

  device_file_ = base::CreatePlatformFile(
      device_path_,
      base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ,
      NULL,
      &error);

  if (error != base::PLATFORM_FILE_OK) {
    DLOG(ERROR) << "Unable to open file for read: " << device_path_.value();
    Error(error::kOpenDevice);
    return;
  }

  PostProgress(0);

  PostTask(base::Bind(&ImageWriter::VerifyChunk, AsWeakPtr()));
}

void ImageWriter::Cancel() {
  CleanUp();
  handler_->SendCancelled();
}

bool ImageWriter::IsRunning() const {
  return image_file_ != base::kInvalidPlatformFileValue ||
         device_file_ != base::kInvalidPlatformFileValue;
}

void ImageWriter::PostTask(const base::Closure& task) {
  base::MessageLoop::current()->PostTask(FROM_HERE, task);
}

void ImageWriter::PostProgress(int64 progress) {
  handler_->SendProgress(progress);
}

void ImageWriter::Error(const std::string& message) {
  CleanUp();
  handler_->SendFailed(message);
}

void ImageWriter::WriteChunk() {
  if (!IsRunning()) {
    return;
  }

  scoped_ptr<char[]> buffer(new char[kBurningBlockSize]);
  memset(buffer.get(), 0, kBurningBlockSize);
  base::PlatformFileInfo info;

  int bytes_read = base::ReadPlatformFile(
      image_file_, bytes_processed_, buffer.get(), kBurningBlockSize);

  if (bytes_read > 0) {
    // Always attempt to write a whole block, as Windows requires 512-byte
    // aligned writes to devices.
    int bytes_written = base::WritePlatformFile(
        device_file_, bytes_processed_, buffer.get(), kBurningBlockSize);

    if (bytes_written < bytes_read) {
      Error(error::kWriteImage);
      return;
    }

    bytes_processed_ += bytes_read;
    PostProgress(bytes_processed_);

    PostTask(base::Bind(&ImageWriter::WriteChunk, AsWeakPtr()));
  } else if (bytes_read == 0) {
    // End of file.
    base::FlushPlatformFile(device_file_);
    CleanUp();
    handler_->SendSucceeded();
  } else {
    // Unable to read entire file.
    Error(error::kReadImage);
  }
}

void ImageWriter::VerifyChunk() {
  if (!IsRunning()) {
    return;
  }

  scoped_ptr<char[]> image_buffer(new char[kBurningBlockSize]);
  scoped_ptr<char[]> device_buffer(new char[kBurningBlockSize]);

  int bytes_read = base::ReadPlatformFile(
      image_file_, bytes_processed_, image_buffer.get(), kBurningBlockSize);

  if (bytes_read > 0) {
    if (base::ReadPlatformFile(device_file_,
                               bytes_processed_,
                               device_buffer.get(),
                               kBurningBlockSize) < bytes_read) {
      LOG(ERROR) << "Failed to read " << kBurningBlockSize << " bytes of "
                 << "device at offset " << bytes_processed_;
      Error(error::kReadDevice);
      return;
    }

    if (memcmp(image_buffer.get(), device_buffer.get(), bytes_read) != 0) {
      LOG(ERROR) << "Write verification failed when comparing " << bytes_read
                 << " bytes at " << bytes_processed_;
      Error(error::kVerificationFailed);
      return;
    }

    bytes_processed_ += bytes_read;
    PostProgress(bytes_processed_);

    PostTask(base::Bind(&ImageWriter::VerifyChunk, AsWeakPtr()));
  } else if (bytes_read == 0) {
    // End of file.
    CleanUp();
    handler_->SendSucceeded();
  } else {
    // Unable to read entire file.
    LOG(ERROR) << "Failed to read " << kBurningBlockSize << " bytes of image "
               << "at offset " << bytes_processed_;
    Error(error::kReadImage);
  }
}

void ImageWriter::CleanUp() {
  if (image_file_ != base::kInvalidPlatformFileValue) {
    base::ClosePlatformFile(image_file_);
    image_file_ = base::kInvalidPlatformFileValue;
  }
  if (device_file_ != base::kInvalidPlatformFileValue) {
    base::ClosePlatformFile(device_file_);
    device_file_ = base::kInvalidPlatformFileValue;
  }
}

}  // namespace image_writer
