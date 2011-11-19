// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_WEBDRIVER_COMMANDS_CREATE_SESSION_H_
#define CHROME_TEST_WEBDRIVER_COMMANDS_CREATE_SESSION_H_

#include <string>
#include <vector>

#include "chrome/test/webdriver/commands/command.h"

namespace webdriver {

class Response;

// Create a new session which is a new instance of the chrome browser with no
// page loaded.  A new session ID is passed back to the user which is used for
// all future commands that are sent to control this new instance.  The
// desired capabilities should be specified in a JSON object with the
// following properties:
// http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session
class CreateSession : public Command {
 public:
  CreateSession(const std::vector<std::string>& path_segments,
                const DictionaryValue* const parameters);
  virtual ~CreateSession();

  virtual bool DoesPost() OVERRIDE;
  virtual void ExecutePost(Response* const response) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(CreateSession);
};

}  // namespace webdriver

#endif  // CHROME_TEST_WEBDRIVER_COMMANDS_CREATE_SESSION_H_
