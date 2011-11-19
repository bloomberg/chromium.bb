// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_WEBDRIVER_COMMANDS_EXECUTE_COMMAND_H_
#define CHROME_TEST_WEBDRIVER_COMMANDS_EXECUTE_COMMAND_H_

#include <string>
#include <vector>

#include "chrome/test/webdriver/commands/webdriver_command.h"

namespace webdriver {

class Response;

// Inject a snippet of javascript into the page and return its result.
// WebElements that should be passed to the script as an argument should be
// specified in the arguments array as WebElement JSON arguments. Likewise,
// any WebElements in the script result will be returned to the client as
// WebElement JSON objects. See:
// http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/execute
class ExecuteCommand : public WebDriverCommand {
 public:
  ExecuteCommand(const std::vector<std::string>& path_segments,
                 const DictionaryValue* const parameters);
  virtual ~ExecuteCommand();

  virtual bool DoesPost() OVERRIDE;
  virtual void ExecutePost(Response* const response) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExecuteCommand);
};

}  // namespace webdriver

#endif  // CHROME_TEST_WEBDRIVER_COMMANDS_EXECUTE_COMMAND_H_
