// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_WEBDRIVER_COMMANDS_WEBELEMENT_COMMANDS_H_
#define CHROME_TEST_WEBDRIVER_COMMANDS_WEBELEMENT_COMMANDS_H_

#include <string>
#include <vector>

#include "chrome/test/webdriver/commands/webdriver_command.h"

class DictionaryValue;

namespace webdriver {

class Response;

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

// Sends keys to the specified web element. Also gets the value property of an
// element.
// http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/element/:id/value
class ElementValueCommand : public WebElementCommand {
 public:
  ElementValueCommand(const std::vector<std::string>& path_segments,
                      DictionaryValue* parameters)
      : WebElementCommand(path_segments, parameters) {}
  virtual ~ElementValueCommand() {}

  virtual bool DoesGet() { return true; }
  virtual bool DoesPost() { return true; }
  virtual void ExecuteGet(Response* const response);
  virtual void ExecutePost(Response* const response);

 private:
  DISALLOW_COPY_AND_ASSIGN(ElementValueCommand);
};

// Gets the visible text of the specified web element.
// http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/element/:id/text
class ElementTextCommand : public WebElementCommand {
 public:
  ElementTextCommand(const std::vector<std::string>& path_segments,
                     DictionaryValue* parameters)
      : WebElementCommand(path_segments, parameters) {}
  virtual ~ElementTextCommand() {}

  virtual bool DoesGet() { return true; }
  virtual void ExecuteGet(Response* const response);

 private:
  DISALLOW_COPY_AND_ASSIGN(ElementTextCommand);
};

}  // namespace webdriver

#endif  // CHROME_TEST_WEBDRIVER_COMMANDS_WEBELEMENT_COMMANDS_H_
