// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_PROTOCOL_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_PROTOCOL_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/values.h"

// Utility class for processing DevTools remote debugging messages.
class DevToolsProtocol {
 public:
  // Caller maintains ownership of |command|. |*params| is owned by |command|.
  static bool ParseCommand(base::DictionaryValue* command,
                           int* command_id,
                           std::string* method,
                           base::DictionaryValue** params);

  static bool ParseNotification(const std::string& json,
                                std::string* method,
                                scoped_ptr<base::DictionaryValue>* params);

  static bool ParseResponse(const std::string& json,
                            int* command_id,
                            int* error_code);

  static std::string SerializeCommand(int command_id,
                                      const std::string& method,
                                      scoped_ptr<base::DictionaryValue> params);

  static scoped_ptr<base::DictionaryValue> CreateSuccessResponse(
      int command_id,
      scoped_ptr<base::DictionaryValue> result);

  static scoped_ptr<base::DictionaryValue> CreateInvalidParamsResponse(
      int command_id,
      const std::string& param);

 private:
  DevToolsProtocol() {}
  ~DevToolsProtocol() {}
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_PROTOCOL_H_
