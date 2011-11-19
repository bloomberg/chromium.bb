// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_WEBDRIVER_COMMANDS_EXECUTE_ASYNC_SCRIPT_COMMAND_H_
#define CHROME_TEST_WEBDRIVER_COMMANDS_EXECUTE_ASYNC_SCRIPT_COMMAND_H_

#include <string>
#include <vector>

#include "chrome/test/webdriver/commands/webdriver_command.h"

namespace base {
class DictionaryValue;
}

namespace webdriver {

class Response;

// Inject a snippet of javascript into the page and return its result.
// WebElements that should be passed to the script as an argument should be
// specified in the arguments array as WebElement JSON arguments. Likewise,
// any WebElements in the script result will be returned to the client as
// WebElement JSON objects.
class ExecuteAsyncScriptCommand : public WebDriverCommand {
 public:
  ExecuteAsyncScriptCommand(const std::vector<std::string>& path_segments,
                            const base::DictionaryValue* const parameters);
  virtual ~ExecuteAsyncScriptCommand();

  virtual bool DoesPost() OVERRIDE;
  virtual void ExecutePost(Response* const response) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExecuteAsyncScriptCommand);
};

}  // namespace webdriver

#endif  // CHROME_TEST_WEBDRIVER_COMMANDS_EXECUTE_ASYNC_SCRIPT_COMMAND_H_
