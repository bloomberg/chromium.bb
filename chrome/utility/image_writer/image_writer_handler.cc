// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/image_writer/image_writer_handler.h"

#include "base/files/file_path.h"
#include "chrome/common/extensions/chrome_utility_extensions_messages.h"
#include "chrome/utility/image_writer/error_messages.h"
#include "content/public/utility/utility_thread.h"

namespace image_writer {

ImageWriterHandler::ImageWriterHandler() {}
ImageWriterHandler::~ImageWriterHandler() {}

void ImageWriterHandler::SendSucceeded() {
  Send(new ChromeUtilityHostMsg_ImageWriter_Succeeded());
  content::UtilityThread::Get()->ReleaseProcessIfNeeded();
}

void ImageWriterHandler::SendCancelled() {
  Send(new ChromeUtilityHostMsg_ImageWriter_Cancelled());
  content::UtilityThread::Get()->ReleaseProcessIfNeeded();
}

void ImageWriterHandler::SendFailed(const std::string& message) {
  Send(new ChromeUtilityHostMsg_ImageWriter_Failed(message));
  content::UtilityThread::Get()->ReleaseProcessIfNeeded();
}

void ImageWriterHandler::SendProgress(int64 progress) {
  Send(new ChromeUtilityHostMsg_ImageWriter_Progress(progress));
}

void ImageWriterHandler::Send(IPC::Message* msg) {
  content::UtilityThread::Get()->Send(msg);
}

bool ImageWriterHandler::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ImageWriterHandler, message)
  IPC_MESSAGE_HANDLER(ChromeUtilityMsg_ImageWriter_Write, OnWriteStart)
  IPC_MESSAGE_HANDLER(ChromeUtilityMsg_ImageWriter_Verify, OnVerifyStart)
  IPC_MESSAGE_HANDLER(ChromeUtilityMsg_ImageWriter_Cancel, OnCancel)
  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ImageWriterHandler::OnWriteStart(const base::FilePath& image,
                                      const base::FilePath& device) {
  if (!image_writer_.get() || image != image_writer_->GetImagePath() ||
      device != image_writer_->GetDevicePath()) {
    image_writer_.reset(new ImageWriter(this, image, device));
  }

  if (image_writer_->IsRunning()) {
    SendFailed(error::kOperationAlreadyInProgress);
    return;
  }

  if (!image_writer_->IsValidDevice()) {
    SendFailed(error::kInvalidDevice);
    return;
  }

  image_writer_->UnmountVolumes(
      base::Bind(&ImageWriter::Write, image_writer_->AsWeakPtr()));
}

void ImageWriterHandler::OnVerifyStart(const base::FilePath& image,
                                       const base::FilePath& device) {
  if (!image_writer_.get() || image != image_writer_->GetImagePath() ||
      device != image_writer_->GetDevicePath()) {
    image_writer_.reset(new ImageWriter(this, image, device));
  }

  if (image_writer_->IsRunning()) {
    SendFailed(error::kOperationAlreadyInProgress);
    return;
  }

  if (!image_writer_->IsValidDevice()) {
    SendFailed(error::kInvalidDevice);
    return;
  }

  image_writer_->Verify();
}

void ImageWriterHandler::OnCancel() {
  if (image_writer_.get()) {
    image_writer_->Cancel();
  } else {
    SendCancelled();
  }
}

}  // namespace image_writer
