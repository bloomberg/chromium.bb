// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/image_writer/image_writer_handler.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/optional.h"
#include "chrome/common/extensions/removable_storage_writer.mojom.h"
#include "chrome/utility/image_writer/error_messages.h"

namespace {

bool IsTestDevice(const base::FilePath& device) {
  return device.AsUTF8Unsafe() ==
         extensions::mojom::RemovableStorageWriter::kTestDevice;
}

base::FilePath MakeTestDevicePath(const base::FilePath& image) {
  return image.ReplaceExtension(FILE_PATH_LITERAL("out"));
}

}  // namespace

namespace image_writer {

ImageWriterHandler::ImageWriterHandler() = default;

ImageWriterHandler::~ImageWriterHandler() = default;

void ImageWriterHandler::Write(
    const base::FilePath& image,
    const base::FilePath& device,
    extensions::mojom::RemovableStorageWriterClientPtr client) {
  client_ = std::move(client);
  client_.set_connection_error_handler(
      base::Bind(&ImageWriterHandler::Cancel, base::Unretained(this)));

  base::FilePath target_device = device;
  const bool test_mode = IsTestDevice(device);
  if (test_mode)
    target_device = MakeTestDevicePath(image);

  // https://crbug.com/352442
  if (!image_writer_.get() || image != image_writer_->GetImagePath() ||
      target_device != image_writer_->GetDevicePath()) {
    image_writer_.reset(new ImageWriter(this, image, target_device));
  }

  if (image_writer_->IsRunning()) {
    SendFailed(error::kOperationAlreadyInProgress);
    return;
  }

  if (test_mode) {
    image_writer_->Write();
    return;
  }

  if (!image_writer_->IsValidDevice()) {
    SendFailed(error::kInvalidDevice);
    return;
  }

  image_writer_->UnmountVolumes(
      base::Bind(&ImageWriter::Write, image_writer_->AsWeakPtr()));
}

void ImageWriterHandler::Verify(
    const base::FilePath& image,
    const base::FilePath& device,
    extensions::mojom::RemovableStorageWriterClientPtr client) {
  client_ = std::move(client);
  client_.set_connection_error_handler(
      base::Bind(&ImageWriterHandler::Cancel, base::Unretained(this)));

  base::FilePath target_device = device;
  const bool test_mode = IsTestDevice(device);
  if (test_mode)
    target_device = MakeTestDevicePath(image);

  // https://crbug.com/352442
  if (!image_writer_.get() || image != image_writer_->GetImagePath() ||
      target_device != image_writer_->GetDevicePath()) {
    image_writer_.reset(new ImageWriter(this, image, target_device));
  }

  if (image_writer_->IsRunning()) {
    SendFailed(error::kOperationAlreadyInProgress);
    return;
  }

  if (test_mode) {
    image_writer_->Verify();
    return;
  }

  if (!image_writer_->IsValidDevice()) {
    SendFailed(error::kInvalidDevice);
    return;
  }

  image_writer_->Verify();
}

void ImageWriterHandler::SendProgress(int64_t progress) {
  client_->Progress(progress);
}

void ImageWriterHandler::SendSucceeded() {
  client_->Complete(base::nullopt);
  client_.reset();
}

void ImageWriterHandler::SendFailed(const std::string& error) {
  client_->Complete(error);
  client_.reset();
}

void ImageWriterHandler::Cancel() {
  if (image_writer_)
    image_writer_->Cancel();
  client_.reset();
}

}  // namespace image_writer
