// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_WEBDRIVER_COMMANDS_SET_TIMEOUT_COMMANDS_H_
#define CHROME_TEST_WEBDRIVER_COMMANDS_SET_TIMEOUT_COMMANDS_H_

#include <string>
#include <vector>

#include "chrome/test/webdriver/commands/webdriver_command.h"

namespace base {
class DictionaryValue;
}

namespace webdriver {

class Response;

class SetTimeoutCommand : public WebDriverCommand {
 public:
  SetTimeoutCommand(const std::vector<std::string>& path_segments,
                    const base::DictionaryValue* const parameters);
  virtual ~SetTimeoutCommand();

  virtual bool DoesPost() OVERRIDE;
  virtual void ExecutePost(Response* const response) OVERRIDE;
  virtual void SetTimeout(int timeout_ms) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SetTimeoutCommand);
};

// Set timeout for asynchronous script execution (/session/*/execute_async).
class SetAsyncScriptTimeoutCommand : public SetTimeoutCommand {
 public:
  SetAsyncScriptTimeoutCommand(const std::vector<std::string>& path_segments,
                               const base::DictionaryValue* const parameters);
  virtual ~SetAsyncScriptTimeoutCommand();
  virtual void SetTimeout(int timeout_ms) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(SetAsyncScriptTimeoutCommand);
};

// Set the amount of time the driver should wait when searching for elements.
class ImplicitWaitCommand : public SetTimeoutCommand {
 public:
  ImplicitWaitCommand(const std::vector<std::string>& path_segments,
                      const base::DictionaryValue* const parameters);
  virtual ~ImplicitWaitCommand();
  virtual void SetTimeout(int timeout_ms) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ImplicitWaitCommand);
};

}  // namespace webdriver

#endif  // CHROME_TEST_WEBDRIVER_COMMANDS_SET_TIMEOUT_COMMANDS_H_
