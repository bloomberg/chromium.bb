// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_WEBDRIVER_COMMANDS_ALERT_COMMANDS_H_
#define CHROME_TEST_WEBDRIVER_COMMANDS_ALERT_COMMANDS_H_

#include <string>
#include <vector>

#include "chrome/test/webdriver/commands/webdriver_command.h"

namespace base {
class DictionaryValue;
}

namespace webdriver {

class Response;

// Gets the message of the JavaScript modal dialog, or sets the prompt text.
class AlertTextCommand : public WebDriverCommand {
 public:
  AlertTextCommand(const std::vector<std::string>& path_segments,
                   base::DictionaryValue* parameters);
  virtual ~AlertTextCommand();

  virtual bool DoesGet() OVERRIDE;
  virtual bool DoesPost() OVERRIDE;
  virtual void ExecuteGet(Response* const response) OVERRIDE;
  virtual void ExecutePost(Response* const response) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(AlertTextCommand);
};

class AcceptAlertCommand : public WebDriverCommand {
 public:
  AcceptAlertCommand(const std::vector<std::string>& path_segments,
                     base::DictionaryValue* parameters);
  virtual ~AcceptAlertCommand();

  virtual bool DoesPost() OVERRIDE;
  virtual void ExecutePost(Response* const response) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(AcceptAlertCommand);
};

class DismissAlertCommand : public WebDriverCommand {
 public:
  DismissAlertCommand(const std::vector<std::string>& path_segments,
                      base::DictionaryValue* parameters);
  virtual ~DismissAlertCommand();

  virtual bool DoesPost() OVERRIDE;
  virtual void ExecutePost(Response* const response) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DismissAlertCommand);
};

}  // namespace webdriver

#endif  // CHROME_TEST_WEBDRIVER_COMMANDS_ALERT_COMMANDS_H_
