// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_WEBDRIVER_COMMANDS_KEYS_COMMAND_H_
#define CHROME_TEST_WEBDRIVER_COMMANDS_KEYS_COMMAND_H_

#include <string>
#include <vector>

#include "chrome/test/webdriver/commands/webdriver_command.h"

namespace base {
class DictionaryValue;
}

namespace webdriver {

class Error;
class Response;

// Send a sequence of key strokes to the page.
class KeysCommand : public WebDriverCommand {
 public:
  KeysCommand(const std::vector<std::string>& path_segments,
              const DictionaryValue* const parameters);
  virtual ~KeysCommand();

  virtual bool DoesPost() OVERRIDE;
  virtual void ExecutePost(Response* const response) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(KeysCommand);
};

}  // namespace webdriver

#endif  // CHROME_TEST_WEBDRIVER_COMMANDS_KEYS_COMMAND_H_
