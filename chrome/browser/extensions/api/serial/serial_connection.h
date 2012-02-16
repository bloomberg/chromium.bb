// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SERIAL_SERIAL_CONNECTION_H_
#define CHROME_BROWSER_EXTENSIONS_API_SERIAL_SERIAL_CONNECTION_H_
#pragma once

#include <string>

#include "chrome/browser/extensions/api/api_resource.h"

namespace extensions {

class APIResourceEventNotifier;

class SerialConnection : public APIResource {
 public:
  SerialConnection(const std::string& port,
                   APIResourceEventNotifier* event_notifier);
  virtual ~SerialConnection();

  bool Open();
  void Close();

  int Read(unsigned char* byte);
  int Write(const std::string& data);

 private:
  std::string port_;
  int fd_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SERIAL_SERIAL_CONNECTION_H_
