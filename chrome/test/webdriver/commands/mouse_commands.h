// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_WEBDRIVER_COMMANDS_MOUSE_COMMANDS_H_
#define CHROME_TEST_WEBDRIVER_COMMANDS_MOUSE_COMMANDS_H_

#include <string>
#include <vector>

#include "chrome/test/webdriver/commands/webelement_commands.h"

class DictionaryValue;

namespace webdriver {

class Response;

enum MouseAction {
  kClick,
  kHover,
  kDrag,
};

class MouseCommand : public WebElementCommand {
 public:
  MouseCommand(const std::vector<std::string>& path_segments,
               const DictionaryValue* const parameters,
               MouseAction cmd) :
      WebElementCommand(path_segments, parameters), cmd_(cmd) {}
  virtual ~MouseCommand();

  virtual bool DoesPost();
  virtual void ExecutePost(Response* const response);

 protected:
  int drag_x_, drag_y_;

 private:
  MouseAction cmd_;

  DISALLOW_COPY_AND_ASSIGN(MouseCommand);
};

// Click this element. If this causes a new page to load, this method will
// block until the page has loaded. At this point, you should discard all
// references to this element and any further operations performed on this
// element will have undefined behaviour unless you know that the element
// and the page will still be present. See:
// http://selenium.googlecode.com/svn/trunk/docs/api/java/org/openqa/selenium/WebElement.html#click()
class ClickCommand : public MouseCommand {
 public:
  ClickCommand(const std::vector<std::string>& path_segments,
               const DictionaryValue* const parameters);
  virtual ~ClickCommand();

 private:

  DISALLOW_COPY_AND_ASSIGN(ClickCommand);
};

// Move the mouse over an element. See:
// http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/element/:id/hover
class HoverCommand : public MouseCommand {
 public:
  HoverCommand(const std::vector<std::string>& path_segments,
               const DictionaryValue* const parameters);
  virtual ~HoverCommand();

 private:

  DISALLOW_COPY_AND_ASSIGN(HoverCommand);
};

// Drag and drop an element. The distance to drag an element should be
// specified relative to the upper-left corner of the page. See:
// http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/element/:id/drag
class DragCommand : public MouseCommand {
 public:
  DragCommand(const std::vector<std::string>& path_segments,
              const DictionaryValue* const parameters);
  virtual ~DragCommand();

  virtual bool Init(Response* const response);

 private:

  DISALLOW_COPY_AND_ASSIGN(DragCommand);
};

}  // namespace webdriver

#endif  // CHROME_TEST_WEBDRIVER_COMMANDS_MOUSE_COMMANDS_H_
