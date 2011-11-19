// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_WEBDRIVER_COMMANDS_MOUSE_COMMANDS_H_
#define CHROME_TEST_WEBDRIVER_COMMANDS_MOUSE_COMMANDS_H_

#include <string>
#include <vector>

#include "chrome/test/webdriver/commands/webelement_commands.h"
#include "chrome/test/webdriver/webdriver_element_id.h"

namespace base {
class DictionaryValue;
}

namespace webdriver {

class Response;

// Click an element. See:
// http://selenium.googlecode.com/svn/trunk/docs/api/java/org/openqa/selenium/WebElement.html#click()
class MoveAndClickCommand : public WebElementCommand {
 public:
  MoveAndClickCommand(const std::vector<std::string>& path_segments,
                      const base::DictionaryValue* const parameters);
  virtual ~MoveAndClickCommand();

  virtual bool DoesPost() OVERRIDE;
  virtual void ExecutePost(Response* const response) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(MoveAndClickCommand);
};

// Move the mouse over an element. See:
// http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/element/:id/hover
class HoverCommand : public WebElementCommand {
 public:
  HoverCommand(const std::vector<std::string>& path_segments,
               const base::DictionaryValue* const parameters);
  virtual ~HoverCommand();

  virtual bool DoesPost() OVERRIDE;
  virtual void ExecutePost(Response* const response) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(HoverCommand);
};

// Drag and drop an element. The distance to drag an element should be
// specified relative to the upper-left corner of the page. See:
// http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/element/:id/drag
class DragCommand : public WebElementCommand {
 public:
  DragCommand(const std::vector<std::string>& path_segments,
              const base::DictionaryValue* const parameters);
  virtual ~DragCommand();

  virtual bool Init(Response* const response) OVERRIDE;
  virtual bool DoesPost() OVERRIDE;
  virtual void ExecutePost(Response* const response) OVERRIDE;

 private:
  int drag_x_, drag_y_;

  DISALLOW_COPY_AND_ASSIGN(DragCommand);
};

// Base class for the following API command classes.
// - /session/:sessionId/moveto
// - /session/:sessionId/click
// - /session/:sessionId/buttondown
// - /session/:sessionId/buttonup
// - /session/:sessionId/doubleclick
class AdvancedMouseCommand : public WebDriverCommand {
 public:
  AdvancedMouseCommand(const std::vector<std::string>& path_segments,
                       const base::DictionaryValue* const parameters);
  virtual ~AdvancedMouseCommand();

  virtual bool DoesPost() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(AdvancedMouseCommand);
};

// Move the mouse by an offset of the specified element. If no element is
// specified, the move is relative to the current mouse cursor. If an element is
// provided but no offset, the mouse will be moved to the center of the element.
// If the element is not visible, it will be scrolled into view.
// http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/moveto
class MoveToCommand : public AdvancedMouseCommand {
 public:
  MoveToCommand(const std::vector<std::string>& path_segments,
                const base::DictionaryValue* const parameters);
  virtual ~MoveToCommand();

  virtual bool Init(Response* const response) OVERRIDE;
  virtual void ExecutePost(Response* const response) OVERRIDE;

 private:
  bool has_element_;
  ElementId element_;
  bool has_offset_;
  int x_offset_;
  int y_offset_;

  DISALLOW_COPY_AND_ASSIGN(MoveToCommand);
};

// Click any mouse button (at the coordinates set by the last moveto command).
// Note that calling this command after calling buttondown and before calling
// button up (or any out-of-order interactions sequence) will yield undefined
// behaviour).
// http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/click
class ClickCommand : public AdvancedMouseCommand {
 public:
  ClickCommand(const std::vector<std::string>& path_segments,
               const base::DictionaryValue* const parameters);
  virtual ~ClickCommand();

  virtual bool Init(Response* const response) OVERRIDE;
  virtual void ExecutePost(Response* const response) OVERRIDE;

 private:
  int button_;

  DISALLOW_COPY_AND_ASSIGN(ClickCommand);
};

// Click and hold the left mouse button (at the coordinates set by the last
// moveto command). Note that the next mouse-related command that should follow
// is buttondown . Any other mouse command (such as click or another call to
// buttondown) will yield undefined behaviour.
// http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/buttondown
class ButtonDownCommand : public AdvancedMouseCommand {
 public:
  ButtonDownCommand(const std::vector<std::string>& path_segments,
                    const base::DictionaryValue* const parameters);
  virtual ~ButtonDownCommand();

  virtual void ExecutePost(Response* const response) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ButtonDownCommand);
};

// Releases the mouse button previously held (where the mouse is currently at).
// Must be called once for every buttondown command issued. See the note in
// click and buttondown about implications of out-of-order commands.
// http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/buttonup
class ButtonUpCommand : public AdvancedMouseCommand {
 public:
  ButtonUpCommand(const std::vector<std::string>& path_segments,
                  const base::DictionaryValue* const parameters);
  virtual ~ButtonUpCommand();

  virtual void ExecutePost(Response* const response) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ButtonUpCommand);
};

// Double-clicks at the current mouse coordinates (set by moveto).
// http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/doubleclick
class DoubleClickCommand : public AdvancedMouseCommand {
 public:
  DoubleClickCommand(const std::vector<std::string>& ps,
                     const base::DictionaryValue* const parameters);
  virtual ~DoubleClickCommand();

  virtual void ExecutePost(Response* const response) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DoubleClickCommand);
};

}  // namespace webdriver

#endif  // CHROME_TEST_WEBDRIVER_COMMANDS_MOUSE_COMMANDS_H_
