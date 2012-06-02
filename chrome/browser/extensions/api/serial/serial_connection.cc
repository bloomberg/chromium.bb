// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/serial/serial_connection.h"

#include <string>

#include "base/file_path.h"
#include "base/string_util.h"

namespace extensions {

const char kSerialConnectionNotFoundError[] = "Serial connection not found";

SerialConnection::SerialConnection(const std::string& port,
                                   APIResourceEventNotifier* event_notifier)
    : APIResource(APIResource::SerialConnectionResource, event_notifier),
      port_(port),
      file_(base::kInvalidPlatformFileValue) {
}

SerialConnection::~SerialConnection() {
  Close();
}

bool SerialConnection::Open() {
  bool created = false;

  // It's the responsibility of the API wrapper around SerialConnection to
  // validate the supplied path against the set of valid port names, and
  // it is a reasonable assumption that serial port names are ASCII.
  CHECK(IsStringASCII(port_));
  FilePath file_path(FilePath::FromUTF8Unsafe(port_));

  file_ = base::CreatePlatformFile(file_path,
    base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ |
    base::PLATFORM_FILE_WRITE | base::PLATFORM_FILE_EXCLUSIVE_READ |
    base::PLATFORM_FILE_EXCLUSIVE_WRITE |
    base::PLATFORM_FILE_TERMINAL_DEVICE, &created, NULL);
  if (file_ == base::kInvalidPlatformFileValue) {
    return false;
  }
  return PostOpen();
}

void SerialConnection::Close() {
  if (file_ != base::kInvalidPlatformFileValue) {
    base::ClosePlatformFile(file_);
    file_ = base::kInvalidPlatformFileValue;
  }
}

int SerialConnection::Read(uint8* byte) {
  DCHECK(byte);
  return base::ReadPlatformFileAtCurrentPos(file_,
                                            reinterpret_cast<char*>(byte), 1);
}

int SerialConnection::Write(scoped_refptr<net::IOBuffer> io_buffer,
                            int byte_count) {
  DCHECK(io_buffer->data());
  DCHECK(byte_count >= 0);
  return base::WritePlatformFileAtCurrentPos(file_,
                                             io_buffer->data(), byte_count);
}

void SerialConnection::Flush() {
  base::FlushPlatformFile(file_);
}

}  // namespace extensions
