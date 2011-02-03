// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_WEBDRIVER_COMMANDS_WEBELEMENT_COMMAND_H_
#define CHROME_TEST_WEBDRIVER_COMMANDS_WEBELEMENT_COMMAND_H_

#include <string>
#include <vector>

#include "chrome/test/webdriver/commands/webdriver_command.h"

namespace webdriver {

// Handles commands that interact with a web element in the WebDriver REST
// service.
class WebElementCommand : public WebDriverCommand {
 public:
  inline WebElementCommand(const std::vector<std::string>& path_segments,
                           const DictionaryValue* const parameters)
      : WebDriverCommand(path_segments, parameters),
                         path_segments_(path_segments) {}
  virtual ~WebElementCommand() {}

  virtual bool Init(Response* const response);

 protected:
  bool GetElementLocation(bool in_view, int* x, int* y);
  bool GetElementSize(int* width, int* height);

  const std::vector<std::string>& path_segments_;
  std::string element_id;

 private:
  virtual bool RequiresValidTab() { return true; }

  DISALLOW_COPY_AND_ASSIGN(WebElementCommand);
};

}  // namespace webdriver

#endif  // CHROME_TEST_WEBDRIVER_COMMANDS_WEBELEMENT_COMMAND_H_
