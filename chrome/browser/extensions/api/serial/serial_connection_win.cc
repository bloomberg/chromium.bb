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

int SerialConnection::Read(unsigned char* byte) {
  // TODO(miket): implement
  return -1;
}

int SerialConnection::Write(const std::string& data) {
  // TODO(miket): implement
  return -1;
}

// static
SerialConnection::StringSet SerialConnection::GenerateValidPatterns() {
  // TODO(miket): implement
  StringSet valid_patterns;
  return valid_patterns;
}

// static
SerialConnection::StringSet SerialConnection::GenerateValidSerialPortNames() {
  // TODO(miket): implement
  StringSet valid_names;
  return valid_names;
}

}  // namespace extensions
