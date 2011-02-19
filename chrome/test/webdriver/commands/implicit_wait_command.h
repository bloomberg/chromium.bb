// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_WEBDRIVER_COMMANDS_IMPLICIT_WAIT_COMMAND_H_
#define CHROME_TEST_WEBDRIVER_COMMANDS_IMPLICIT_WAIT_COMMAND_H_

#include <string>
#include <vector>

#include "chrome/test/webdriver/commands/webdriver_command.h"

namespace webdriver {

class Response;

// Set the amount of time the driver should wait when searching for elements.
// If this command is never sent, the driver will default to an implicit wait
// of 0 ms. Until the webelement commands are checked in we do no use this
// variable.  For more information see:
// http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/timeouts/implicit_wait
class ImplicitWaitCommand : public WebDriverCommand {
 public:
  ImplicitWaitCommand(const std::vector<std::string>& path_segments,
                      const DictionaryValue* const parameters);
  virtual ~ImplicitWaitCommand();

  virtual bool Init(Response* const response);

  virtual bool DoesPost();
  virtual void ExecutePost(Response* const response);

 private:
  int ms_to_wait_;
  virtual bool RequiresValidTab();

  DISALLOW_COPY_AND_ASSIGN(ImplicitWaitCommand);
};

}  // namespace webdriver

#endif  // CHROME_TEST_WEBDRIVER_COMMANDS_IMPLICIT_WAIT_COMMAND_H_

