// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_WEBDRIVER_COMMANDS_FIND_ELEMENT_COMMANDS_H_
#define CHROME_TEST_WEBDRIVER_COMMANDS_FIND_ELEMENT_COMMANDS_H_

#include <string>
#include <vector>

#include "chrome/test/webdriver/commands/webdriver_command.h"

namespace webdriver {

class Response;

// Base class for searching a page, this class can find either a single
// webelement or return multiple matches.
class FindElementCommand : public WebDriverCommand {
 public:
   FindElementCommand(const std::vector<std::string>& path_segments,
                      const DictionaryValue* const parameters,
                      const bool find_one_element);
  virtual ~FindElementCommand();

  virtual bool DoesPost() OVERRIDE;
  virtual void ExecutePost(Response* const response) OVERRIDE;

 private:
  const bool find_one_element_;
  std::string root_element_id_;
  std::string use_;
  std::string value_;

  DISALLOW_COPY_AND_ASSIGN(FindElementCommand);
};

// Search for an element on the page, starting from the document root.
// The located element will be returned as a WebElement JSON object. See:
// http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/element
class FindOneElementCommand : public FindElementCommand {
 public:
  FindOneElementCommand(const std::vector<std::string>& path_segments,
                        const DictionaryValue* const parameters)
      : FindElementCommand(path_segments, parameters, true) {}
  virtual ~FindOneElementCommand() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(FindOneElementCommand);
};

// Search for multiple elements on the page, starting from the identified
// element. The located elements will be returned as a WebElement JSON
// objects. See:
// http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/element/:id/elements
class FindManyElementsCommand : public FindElementCommand {
 public:
  FindManyElementsCommand(const std::vector<std::string>& path_segments,
                          const DictionaryValue* const parameters)
      : FindElementCommand(path_segments, parameters, false) {}
  virtual ~FindManyElementsCommand() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(FindManyElementsCommand);
};

}  // namespace webdriver

#endif  // CHROME_TEST_WEBDRIVER_COMMANDS_FIND_ELEMENT_COMMANDS_H_
