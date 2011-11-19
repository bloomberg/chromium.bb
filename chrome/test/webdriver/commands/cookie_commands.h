// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_WEBDRIVER_COMMANDS_COOKIE_COMMANDS_H_
#define CHROME_TEST_WEBDRIVER_COMMANDS_COOKIE_COMMANDS_H_
#pragma once

#include <string>
#include <vector>

#include "chrome/test/webdriver/commands/webdriver_command.h"
#include "googleurl/src/gurl.h"

namespace base {
class DictionaryValue;
}

namespace webdriver {

class Response;

// Retrieve all cookies visible to the current page. Each cookie will be
// returned as a JSON object with the following properties:
// name, value, path, domain, secure, and expiry. See:
// http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/cookie
class CookieCommand : public WebDriverCommand {
 public:
  CookieCommand(const std::vector<std::string>& path_segments,
                const base::DictionaryValue* const parameters);
  virtual ~CookieCommand();

  virtual bool DoesDelete() OVERRIDE;
  virtual bool DoesGet() OVERRIDE;
  virtual bool DoesPost() OVERRIDE;

  virtual void ExecuteDelete(Response* const response) OVERRIDE;
  virtual void ExecuteGet(Response* const response) OVERRIDE;
  virtual void ExecutePost(Response* const response) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(CookieCommand);
};

// Set a cookie. The cookie should be specified as a JSON object with the
// following properties: name, value, path, domain, secure, and expiry. See:
// http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/cookie/:name
class NamedCookieCommand : public WebDriverCommand {
 public:
  NamedCookieCommand(const std::vector<std::string>& path_segments,
                     const base::DictionaryValue* const parameters);
  virtual ~NamedCookieCommand();

  virtual bool Init(Response* const response) OVERRIDE;

  virtual bool DoesDelete() OVERRIDE;

  virtual void ExecuteDelete(Response* const response) OVERRIDE;

 private:
  std::string cookie_name_;

  DISALLOW_COPY_AND_ASSIGN(NamedCookieCommand);
};

}  // namespace webdriver

#endif  // CHROME_TEST_WEBDRIVER_COMMANDS_COOKIE_COMMANDS_H_
