// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_WEBDRIVER_COMMANDS_WEBELEMENT_COMMANDS_H_
#define CHROME_TEST_WEBDRIVER_COMMANDS_WEBELEMENT_COMMANDS_H_

#include <string>
#include <vector>

#include "chrome/test/webdriver/commands/webdriver_command.h"
#include "chrome/test/webdriver/web_element_id.h"

class DictionaryValue;

namespace webdriver {

class Response;

// Handles commands that interact with a web element in the WebDriver REST
// service.
class WebElementCommand : public WebDriverCommand {
 public:
  WebElementCommand(const std::vector<std::string>& path_segments,
                    const DictionaryValue* const parameters);
  virtual ~WebElementCommand();

  virtual bool Init(Response* const response);

 protected:
  bool GetElementSize(int* width, int* height);

  const std::vector<std::string>& path_segments_;
  WebElementId element;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebElementCommand);
};

// Retrieves element attributes.
// http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/element/:id/attribute/:name
class ElementAttributeCommand : public WebElementCommand {
 public:
  ElementAttributeCommand(const std::vector<std::string>& path_segments,
                          DictionaryValue* parameters);
  virtual ~ElementAttributeCommand();

  virtual bool DoesGet();
  virtual void ExecuteGet(Response* const response);

 private:
  DISALLOW_COPY_AND_ASSIGN(ElementAttributeCommand);
};

// Clears test input elements.
// http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/element/:id/clear
class ElementClearCommand : public WebElementCommand {
 public:
  ElementClearCommand(const std::vector<std::string>& path_segments,
                      DictionaryValue* parameters);
  virtual ~ElementClearCommand();

  virtual bool DoesPost();
  virtual void ExecutePost(Response* const response);

 private:
  DISALLOW_COPY_AND_ASSIGN(ElementClearCommand);
};

// Retrieves element style properties.
// http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/element/:id/css/:propertyName
class ElementCssCommand : public WebElementCommand {
 public:
  ElementCssCommand(const std::vector<std::string>& path_segments,
                    DictionaryValue* parameters);
  virtual ~ElementCssCommand();

  virtual bool DoesGet();
  virtual void ExecuteGet(Response* const response);

 private:
  DISALLOW_COPY_AND_ASSIGN(ElementCssCommand);
};

// Queries whether an element is currently displayed ot the user.
// http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/element/:id/displayed
class ElementDisplayedCommand : public WebElementCommand {
 public:
  ElementDisplayedCommand(const std::vector<std::string>& path_segments,
                          DictionaryValue* parameters);
  virtual ~ElementDisplayedCommand();

  virtual bool DoesGet();
  virtual void ExecuteGet(Response* const response);

 private:
  DISALLOW_COPY_AND_ASSIGN(ElementDisplayedCommand);
};

// Queries whether an element is currently enabled.
// http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/element/:id/enabled
class ElementEnabledCommand : public WebElementCommand {
 public:
  ElementEnabledCommand(const std::vector<std::string>& path_segments,
                        DictionaryValue* parameters);
  virtual ~ElementEnabledCommand();

  virtual bool DoesGet();
  virtual void ExecuteGet(Response* const response);

 private:
  DISALLOW_COPY_AND_ASSIGN(ElementEnabledCommand);
};

// Queries whether two elements are equal.
// http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/element/:id/equals/:other
class ElementEqualsCommand : public WebElementCommand {
 public:
  ElementEqualsCommand(const std::vector<std::string>& path_segments,
                       DictionaryValue* parameters);
  virtual ~ElementEqualsCommand();

  virtual bool DoesGet();
  virtual void ExecuteGet(Response* const response);

 private:
  DISALLOW_COPY_AND_ASSIGN(ElementEqualsCommand);
};

// Retrieves the element's location on the page.
// http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/element/:id/location
class ElementLocationCommand : public WebElementCommand {
 public:
  ElementLocationCommand(const std::vector<std::string>& path_segments,
                         DictionaryValue* parameters);
  virtual ~ElementLocationCommand();

  virtual bool DoesGet();
  virtual void ExecuteGet(Response* const response);

 private:
  DISALLOW_COPY_AND_ASSIGN(ElementLocationCommand);
};

// Retrieves the element's location on the page after ensuring it is visible in
// the current viewport.
// http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/element/:id/location_in_view
class ElementLocationInViewCommand : public WebElementCommand {
 public:
  ElementLocationInViewCommand(const std::vector<std::string>& path_segments,
                               DictionaryValue* parameters);
  virtual ~ElementLocationInViewCommand();

  virtual bool DoesGet();
  virtual void ExecuteGet(Response* const response);

 private:
  DISALLOW_COPY_AND_ASSIGN(ElementLocationInViewCommand);
};

// Queries for an element's tag name.
// http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/element/:id/name
class ElementNameCommand : public WebElementCommand {
 public:
  ElementNameCommand(const std::vector<std::string>& path_segments,
                     DictionaryValue* parameters);
  virtual ~ElementNameCommand();

  virtual bool DoesGet();
  virtual void ExecuteGet(Response* const response);

 private:
  DISALLOW_COPY_AND_ASSIGN(ElementNameCommand);
};

// Handles selecting elements and querying whether they are currently selected.
// Queries whether an element is currently enabled.
// http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/element/:id/selected
class ElementSelectedCommand : public WebElementCommand {
 public:
  ElementSelectedCommand(const std::vector<std::string>& path_segments,
                         DictionaryValue* parameters);
  virtual ~ElementSelectedCommand();

  virtual bool DoesGet();
  virtual bool DoesPost();
  virtual void ExecuteGet(Response* const response);
  virtual void ExecutePost(Response* const response);

 private:
  DISALLOW_COPY_AND_ASSIGN(ElementSelectedCommand);
};

// Queries for an element's size.
// http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/element/:id/size
class ElementSizeCommand : public WebElementCommand {
 public:
  ElementSizeCommand(const std::vector<std::string>& path_segments,
                     DictionaryValue* parameters);
  virtual ~ElementSizeCommand();

  virtual bool DoesGet();
  virtual void ExecuteGet(Response* const response);

 private:
  DISALLOW_COPY_AND_ASSIGN(ElementSizeCommand);
};

// Submit's the form containing the target element.
// http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/element/:id/submit
class ElementSubmitCommand : public WebElementCommand {
 public:
  ElementSubmitCommand(const std::vector<std::string>& path_segments,
                       DictionaryValue* parameters);
  virtual ~ElementSubmitCommand();

  virtual bool DoesPost();
  virtual void ExecutePost(Response* const response);

 private:
  DISALLOW_COPY_AND_ASSIGN(ElementSubmitCommand);
};

// Toggle's whether an element is selected.
// http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/element/:id/toggle
class ElementToggleCommand : public WebElementCommand {
 public:
  ElementToggleCommand(const std::vector<std::string>& path_segments,
                       DictionaryValue* parameters);
  virtual ~ElementToggleCommand();

  virtual bool DoesPost();
  virtual void ExecutePost(Response* const response);

 private:
  DISALLOW_COPY_AND_ASSIGN(ElementToggleCommand);
};

// Sends keys to the specified web element. Also gets the value property of an
// element.
// http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/element/:id/value
class ElementValueCommand : public WebElementCommand {
 public:
  ElementValueCommand(const std::vector<std::string>& path_segments,
                      DictionaryValue* parameters);
  virtual ~ElementValueCommand();

  virtual bool DoesGet();
  virtual bool DoesPost();
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
                     DictionaryValue* parameters);
  virtual ~ElementTextCommand();

  virtual bool DoesGet();
  virtual void ExecuteGet(Response* const response);

 private:
  DISALLOW_COPY_AND_ASSIGN(ElementTextCommand);
};

}  // namespace webdriver

#endif  // CHROME_TEST_WEBDRIVER_COMMANDS_WEBELEMENT_COMMANDS_H_
