// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_WEBDRIVER_COMMANDS_SOURCE_COMMAND_H_
#define CHROME_TEST_WEBDRIVER_COMMANDS_SOURCE_COMMAND_H_

#include <string>
#include <vector>

#include "chrome/test/webdriver/commands/webdriver_command.h"

namespace webdriver {

// Controls navigate to new web pages for the current tab.  A call with
// an HTTP GET will return the source of the tab. See:
// http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/source
class SourceCommand : public WebDriverCommand {
 public:
  SourceCommand(const std::vector<std::string>& path_segments,
                const DictionaryValue* const parameters);
  virtual ~SourceCommand();

  virtual bool DoesGet();
  virtual void ExecuteGet(Response* const response);

 private:
  virtual bool RequiresValidTab();

  DISALLOW_COPY_AND_ASSIGN(SourceCommand);
};

}  // namespace webdriver

#endif  // CHROME_TEST_WEBDRIVER_COMMANDS_SOURCE_COMMAND_H_

