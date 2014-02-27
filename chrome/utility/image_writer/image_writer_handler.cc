// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "chrome/common/chrome_utility_messages.h"
#include "chrome/utility/image_writer/error_messages.h"
#include "chrome/utility/image_writer/image_writer_handler.h"
#include "content/public/utility/utility_thread.h"

#if defined(OS_WIN)
#include <windows.h>
#include <setupapi.h>
#include <winioctl.h>
#endif

namespace image_writer {

#if defined(OS_WIN)
const size_t kStorageQueryBufferSize = 1024;
#endif

ImageWriterHandler::ImageWriterHandler() : image_writer_(this) {}
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
  if (!IsValidDevice(device)) {
    Send(new ChromeUtilityHostMsg_ImageWriter_Failed(error::kInvalidDevice));
    return;
  }

  if (image_writer_.IsRunning()) {
    Send(new ChromeUtilityHostMsg_ImageWriter_Failed(
        error::kOperationAlreadyInProgress));
    return;
  }
  image_writer_.Write(image, device);
}

void ImageWriterHandler::OnVerifyStart(const base::FilePath& image,
                                       const base::FilePath& device) {
  if (!IsValidDevice(device)) {
    Send(new ChromeUtilityHostMsg_ImageWriter_Failed(error::kInvalidDevice));
    return;
  }

  if (image_writer_.IsRunning()) {
    Send(new ChromeUtilityHostMsg_ImageWriter_Failed(
        error::kOperationAlreadyInProgress));
    return;
  }
  image_writer_.Verify(image, device);
}

void ImageWriterHandler::OnCancel() {
  if (image_writer_.IsRunning()) {
    image_writer_.Cancel();
  } else {
    SendCancelled();
  }
}

bool ImageWriterHandler::IsValidDevice(const base::FilePath& device) {
#if defined(OS_WIN)
  base::win::ScopedHandle device_handle(
      CreateFile(device.value().c_str(),
                 // Desired access, which is none as we only need metadata.
                 0,
                 // Required to be read + write for devices.
                 FILE_SHARE_READ | FILE_SHARE_WRITE,
                 NULL,           // Optional security attributes.
                 OPEN_EXISTING,  // Devices already exist.
                 0,              // No optional flags.
                 NULL));         // No template file.

  if (!device_handle) {
    PLOG(ERROR) << "Opening device handle failed.";
    return false;
  }

  STORAGE_PROPERTY_QUERY query = STORAGE_PROPERTY_QUERY();
  query.PropertyId = StorageDeviceProperty;
  query.QueryType = PropertyStandardQuery;
  DWORD bytes_returned;

  scoped_ptr<char[]> output_buf(new char[kStorageQueryBufferSize]);
  BOOL status = DeviceIoControl(
      device_handle,                   // Device handle.
      IOCTL_STORAGE_QUERY_PROPERTY,    // Flag to request device properties.
      &query,                          // Query parameters.
      sizeof(STORAGE_PROPERTY_QUERY),  // query parameters size.
      output_buf.get(),                // output buffer.
      kStorageQueryBufferSize,         // Size of buffer.
      &bytes_returned,                 // Number of bytes returned.
                                       // Must not be null.
      NULL);                           // Optional unused overlapped perameter.

  if (status == FALSE) {
    PLOG(ERROR) << "Storage property query failed.";
    return false;
  }

  STORAGE_DEVICE_DESCRIPTOR* device_descriptor =
      reinterpret_cast<STORAGE_DEVICE_DESCRIPTOR*>(output_buf.get());

  if (device_descriptor->RemovableMedia)
    return true;

#else
  // Other platforms will have to be added as they are supported.
  NOTIMPLEMENTED();
#endif
  return false;
}

}  // namespace image_writer
