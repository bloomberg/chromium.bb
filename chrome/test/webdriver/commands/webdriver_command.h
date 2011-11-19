// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_WEBDRIVER_COMMANDS_WEBDRIVER_COMMAND_H_
#define CHROME_TEST_WEBDRIVER_COMMANDS_WEBDRIVER_COMMAND_H_

#include <string>
#include <vector>

#include "chrome/test/webdriver/commands/command.h"

namespace base {
class DictionaryValue;
}

namespace webdriver {

class Response;
class Session;

// All URLs that are found in the document:
// http://code.google.com/p/selenium/wiki/JsonWireProtocol
// and are to be supported for all browsers and platforms should inhert
// this class.  For cases which do not invlove interaction with the
// browser, such a transfering a file, inhert from the Command class
// directly.
class WebDriverCommand : public Command {
 public:
  WebDriverCommand(const std::vector<std::string>& path_segments,
                   const base::DictionaryValue* const parameters);
  virtual ~WebDriverCommand();

  // Initializes this webdriver command by fetching the command session.
  virtual bool Init(Response* const response) OVERRIDE;

  virtual void Finish() OVERRIDE;

 protected:
  Session* session_;
  std::string session_id_;

  DISALLOW_COPY_AND_ASSIGN(WebDriverCommand);
};

}  // namespace webdriver

#endif  // CHROME_TEST_WEBDRIVER_COMMANDS_WEBDRIVER_COMMAND_H_
