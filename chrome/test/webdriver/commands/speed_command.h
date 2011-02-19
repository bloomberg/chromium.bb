// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_WEBDRIVER_COMMANDS_SPEED_COMMAND_H_
#define CHROME_TEST_WEBDRIVER_COMMANDS_SPEED_COMMAND_H_

#include <string>
#include <vector>

#include "chrome/test/webdriver/session.h"
#include "chrome/test/webdriver/commands/webdriver_command.h"

namespace webdriver {

class Response;

// Controls how fast chrome should simulate user typing and mouse movements.
// By default the speed is set to medium however webdriver has not defined
// what this speed means accross browsers.  Currently speed is ignored.
// See: http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/speed
class SpeedCommand : public WebDriverCommand {
 public:
  SpeedCommand(const std::vector<std::string>& path_segments,
               const DictionaryValue* const parameters);
  virtual ~SpeedCommand();

  virtual bool Init(Response* const response);

  virtual bool DoesGet();
  virtual bool DoesPost();
  virtual void ExecuteGet(Response* const response);
  virtual void ExecutePost(Response* const response);

 private:
  virtual bool RequiresValidTab();

  Session::Speed speed_;

  DISALLOW_COPY_AND_ASSIGN(SpeedCommand);
};

}  // namespace webdriver

#endif  // CHROME_TEST_WEBDRIVER_COMMANDS_SPEED_COMMAND_H_

