// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_WEBDRIVER_COMMANDS_CHROME_COMMANDS_H_
#define CHROME_TEST_WEBDRIVER_COMMANDS_CHROME_COMMANDS_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "chrome/test/webdriver/commands/webdriver_command.h"

namespace base {
class DictionaryValue;
}

namespace webdriver {

class Response;

class ExtensionsCommand : public WebDriverCommand {
 public:
  ExtensionsCommand(const std::vector<std::string>& path_segments,
                    const base::DictionaryValue* const parameters);
  virtual ~ExtensionsCommand();

  virtual bool DoesGet() OVERRIDE;
  virtual bool DoesPost() OVERRIDE;

  virtual void ExecuteGet(Response* const response) OVERRIDE;
  virtual void ExecutePost(Response* const response) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionsCommand);
};

}  // namespace webdriver

#endif  // CHROME_TEST_WEBDRIVER_COMMANDS_CHROME_COMMANDS_H_
