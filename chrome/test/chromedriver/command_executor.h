// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_COMMAND_EXECUTOR_H_
#define CHROME_TEST_CHROMEDRIVER_COMMAND_EXECUTOR_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/test/chromedriver/status.h"

namespace base {
class DictionaryValue;
class Value;
}

// Executes WebDriver commands.
class CommandExecutor {
 public:
  virtual ~CommandExecutor() {}

  virtual void Init() = 0;

  // Executes a command synchronously. This function must be thread safe.
  virtual void ExecuteCommand(const std::string& name,
                              const base::DictionaryValue& params,
                              const std::string& session_id,
                              StatusCode* status_code,
                              scoped_ptr<base::Value>* value,
                              std::string* out_session_id) = 0;
};

#endif  // CHROME_TEST_CHROMEDRIVER_COMMAND_EXECUTOR_H_
