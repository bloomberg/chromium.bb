// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_WEBDRIVER_COMMANDS_BROWSER_CONNECTION_COMMANDS_H_
#define CHROME_TEST_WEBDRIVER_COMMANDS_BROWSER_CONNECTION_COMMANDS_H_

#include <string>
#include <vector>

#include "chrome/test/webdriver/commands/webdriver_command.h"

namespace base {
class DictionaryValue;
}

namespace webdriver {

class Response;

// TODO(alnayeem): set online/offline mode needs to be done.
// It cannot be solved only by sending ViewMsg_NetworkStateChanged from
// the browser process to the renderer process.
class BrowserConnectionCommand : public WebDriverCommand {
 public:
  BrowserConnectionCommand(const std::vector<std::string>& path_segments,
                           const base::DictionaryValue* const parameters);
  virtual ~BrowserConnectionCommand();

  virtual bool DoesGet() OVERRIDE;

  // A call with HTTP GET will return the browser connection state.
  virtual void ExecuteGet(Response* const response) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserConnectionCommand);
};

}  // namespace webdriver

#endif  // CHROME_TEST_WEBDRIVER_COMMANDS_BROWSER_CONNECTION_COMMANDS_H_
