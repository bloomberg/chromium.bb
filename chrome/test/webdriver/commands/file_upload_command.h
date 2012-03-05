// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_WEBDRIVER_COMMANDS_FILE_UPLOAD_COMMAND_H_
#define CHROME_TEST_WEBDRIVER_COMMANDS_FILE_UPLOAD_COMMAND_H_

#include <string>
#include <vector>

#include "chrome/test/webdriver/commands/webdriver_command.h"

namespace base {
class DictionaryValue;
}

namespace webdriver {

class Response;

class FileUploadCommand : public WebDriverCommand {
 public:
  FileUploadCommand(const std::vector<std::string>& path_segments,
                    base::DictionaryValue* parameters);
  virtual ~FileUploadCommand();

  virtual bool DoesPost() OVERRIDE;
  virtual void ExecutePost(Response* const response) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(FileUploadCommand);
};

}  // namespace webdriver

#endif  // CHROME_TEST_WEBDRIVER_COMMANDS_FILE_UPLOAD_COMMAND_H_
