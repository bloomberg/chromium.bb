// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/serial/serial_connection.h"

#include <string>

namespace extensions {

SerialConnection::SerialConnection(const std::string& port,
                                   APIResourceEventNotifier* event_notifier)
    : APIResource(APIResource::SerialConnectionResource, event_notifier),
      port_(port), fd_(0) {
}

SerialConnection::~SerialConnection() {
}

bool SerialConnection::Open() {
  // TODO(miket): implement
  return false;
}

void SerialConnection::Close() {
  // TODO(miket): implement
}

int SerialConnection::Read(uint8* byte) {
  // TODO(miket): implement
  return -1;
}

int SerialConnection::Write(scoped_refptr<net::IOBuffer> io_buffer,
                            int byte_count) {
  // TODO(miket): implement
  return -1;
}

}  // namespace extensions
