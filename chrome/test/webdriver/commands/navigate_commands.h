// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_WEBDRIVER_COMMANDS_NAVIGATE_COMMANDS_H_
#define CHROME_TEST_WEBDRIVER_COMMANDS_NAVIGATE_COMMANDS_H_

#include <string>
#include <vector>

#include "chrome/test/webdriver/commands/webdriver_command.h"

namespace webdriver {

class Response;

// Navigate forward in the browser history, if possible. See:
// http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/forward
class ForwardCommand : public WebDriverCommand {
 public:
  ForwardCommand(const std::vector<std::string>& path_segments,
                 const DictionaryValue* const parameters);
  virtual ~ForwardCommand();

  virtual bool DoesPost();
  virtual void ExecutePost(Response* const response);

 private:
  virtual bool RequiresValidTab();

  DISALLOW_COPY_AND_ASSIGN(ForwardCommand);
};

// Navigate backwards in the browser history, if possible. See:
// http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/back
class BackCommand : public WebDriverCommand {
 public:
  BackCommand(const std::vector<std::string>& path_segments,
              const DictionaryValue* const parameters);
  virtual ~BackCommand();

  virtual bool DoesPost();
  virtual void ExecutePost(Response* const response);

 private:
  virtual bool RequiresValidTab();

  DISALLOW_COPY_AND_ASSIGN(BackCommand);
};

// Performs a reload on the current page. See:
// http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/refresh
class RefreshCommand : public WebDriverCommand {
 public:
  RefreshCommand(const std::vector<std::string>& path_segments,
                 const DictionaryValue* const parameters);
  virtual ~RefreshCommand();

  virtual bool DoesPost();
  virtual void ExecutePost(Response* const response);

 private:
  virtual bool RequiresValidTab();

  DISALLOW_COPY_AND_ASSIGN(RefreshCommand);
};

}  // namespace webdriver

#endif  // CHROME_TEST_WEBDRIVER_COMMANDS_NAVIGATE_COMMANDS_H_

