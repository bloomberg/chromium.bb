// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_WEBDRIVER_COMMANDS_TARGET_LOCATOR_COMMANDS_H_
#define CHROME_TEST_WEBDRIVER_COMMANDS_TARGET_LOCATOR_COMMANDS_H_

#include <string>
#include <vector>

#include "chrome/test/webdriver/commands/webdriver_command.h"

namespace base {
class DictionaryValue;
}

namespace webdriver {

class Response;
class ElementId;

// Gets the current window handle.
// REST URL: /session/:sessionId/window_handle
class WindowHandleCommand : public WebDriverCommand {
 public:
  WindowHandleCommand(const std::vector<std::string>& path_segments,
                      base::DictionaryValue* parameters);
  virtual ~WindowHandleCommand();

  virtual bool DoesGet() OVERRIDE;
  virtual void ExecuteGet(Response* const response) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(WindowHandleCommand);
};

// Gets a list of all window handles.
// REST URL: /session/:sessionId/window_handles
class WindowHandlesCommand : public WebDriverCommand {
 public:
  WindowHandlesCommand(const std::vector<std::string>& path_segments,
                       base::DictionaryValue* parameters);
  virtual ~WindowHandlesCommand();

  virtual bool DoesGet() OVERRIDE;
  virtual void ExecuteGet(Response* const response) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(WindowHandlesCommand);
};

// Switches to the given window as the default window to execute commands on
// or closes it.
// REST URL: /session/:sessionId/window
class WindowCommand : public WebDriverCommand {
 public:
  WindowCommand(const std::vector<std::string>& path_segments,
                base::DictionaryValue* parameters);
  virtual ~WindowCommand();

  virtual bool DoesPost() OVERRIDE;
  virtual bool DoesDelete() OVERRIDE;
  virtual void ExecutePost(Response* const response) OVERRIDE;
  virtual void ExecuteDelete(Response* const response) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(WindowCommand);
};

// Switches to the given frame as the default frame to execute commands in.
// REST URL: /session/:sessionId/frame
class SwitchFrameCommand : public WebDriverCommand {
 public:
  SwitchFrameCommand(const std::vector<std::string>& path_segments,
                     base::DictionaryValue* parameters);
  virtual ~SwitchFrameCommand();

  virtual bool DoesPost() OVERRIDE;
  virtual void ExecutePost(Response* const response) OVERRIDE;

 private:
  bool GetWebElementParameter(const std::string& key, ElementId* out) const;

  DISALLOW_COPY_AND_ASSIGN(SwitchFrameCommand);
};

// Retrieves the active element on the current page.
class ActiveElementCommand : public WebDriverCommand {
 public:
  ActiveElementCommand(const std::vector<std::string>& path_segments,
                       base::DictionaryValue* parameters);
  virtual ~ActiveElementCommand();

  virtual bool DoesPost() OVERRIDE;
  virtual void ExecutePost(Response* const response) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ActiveElementCommand);
};

}  // namespace webdriver

#endif  // CHROME_TEST_WEBDRIVER_COMMANDS_TARGET_LOCATOR_COMMANDS_H_
