// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_WEBDRIVER_COMMANDS_WINDOW_COMMANDS_H_
#define CHROME_TEST_WEBDRIVER_COMMANDS_WINDOW_COMMANDS_H_

#include <string>
#include <vector>

#include "chrome/test/webdriver/commands/webdriver_command.h"

namespace base {
class DictionaryValue;
}

namespace webdriver {

class Response;

class WindowSizeCommand : public WebDriverCommand {
 public:
  WindowSizeCommand(const std::vector<std::string>& path_segments,
                    const base::DictionaryValue* parameters);
  virtual ~WindowSizeCommand();

  virtual bool DoesGet() OVERRIDE;
  virtual bool DoesPost() OVERRIDE;
  virtual void ExecuteGet(Response* const response) OVERRIDE;
  virtual void ExecutePost(Response* const response) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(WindowSizeCommand);
};

class WindowPositionCommand : public WebDriverCommand {
 public:
  WindowPositionCommand(const std::vector<std::string>& path_segments,
                        const base::DictionaryValue* parameters);
  virtual ~WindowPositionCommand();

  virtual bool DoesGet() OVERRIDE;
  virtual bool DoesPost() OVERRIDE;
  virtual void ExecuteGet(Response* const response) OVERRIDE;
  virtual void ExecutePost(Response* const response) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(WindowPositionCommand);
};

class WindowMaximizeCommand : public WebDriverCommand {
 public:
  WindowMaximizeCommand(const std::vector<std::string>& path_segments,
                        const base::DictionaryValue* parameters);
  virtual ~WindowMaximizeCommand();

  virtual bool DoesPost() OVERRIDE;
  virtual void ExecutePost(Response* const response) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(WindowMaximizeCommand);
};


}  // namespace webdriver

#endif  // CHROME_TEST_WEBDRIVER_COMMANDS_WINDOW_COMMANDS_H_
