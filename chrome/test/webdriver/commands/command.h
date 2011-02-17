// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_WEBDRIVER_COMMANDS_COMMAND_H_
#define CHROME_TEST_WEBDRIVER_COMMANDS_COMMAND_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/values.h"
#include "base/json/json_writer.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "chrome/test/webdriver/error_codes.h"
#include "chrome/test/webdriver/commands/response.h"

namespace webdriver {

// Base class for a command mapped to a URL in the WebDriver REST API. Each
// URL may respond to commands sent with a DELETE, GET/HEAD, or POST HTTP
// request. For more information on the WebDriver REST API, see
// http://code.google.com/p/selenium/wiki/JsonWireProtocol
class Command {
 public:
  Command(const std::vector<std::string>& path_segments,
          const DictionaryValue* const parameters);
  virtual ~Command();

  // Indicates which HTTP methods this command URL responds to.
  virtual bool DoesDelete();
  virtual bool DoesGet();
  virtual bool DoesPost();

  // Initializes this command for execution. If initialization fails, will
  // return |false| and populate the |response| with the necessary information
  // to return to the client.
  virtual bool Init(Response* const response);

  // Executes the corresponding variant of this command URL.
  // Always called after |Init()| and called from the Execute function.
  // Any failure is handled as a return code found in Response.
  virtual void ExecuteDelete(Response* const response) {}
  virtual void ExecuteGet(Response* const response) {}
  virtual void ExecutePost(Response* const response) {}

 protected:

  // Returns the path variable encoded at the |i|th index (0-based) in the
  // request URL for this command. If the index is out of bounds, an empty
  // string will be returned.
  inline std::string GetPathVariable(const size_t i) const {
    return i < path_segments_.size() ? path_segments_.at(i) : "";
  }

  // Returns true if the command parameter with the given |key| exists and is
  // a null value.
  bool IsNullParameter(const std::string& key) const;

  // Returns the command parameter with the given |key| as a UTF-16 string.
  // Returns true on success.
  bool GetStringParameter(const std::string& key, string16* out) const;

  // Provides the command parameter with the given |key| as a UTF-8 string.
  // Returns true on success.
  bool GetStringParameter(const std::string& key, std::string* out) const;

  // Provides the command parameter with the given |key| as a ASCII string.
  // Returns true on success.
  bool GetStringASCIIParameter(const std::string& key, std::string* out) const;

  // Provides the command parameter with the given |key| as a boolean. Returns
  // false if there is no such parameter, or if it is not a boolean.
  bool GetBooleanParameter(const std::string& key, bool* out) const;

  // Provides the command parameter with the given |key| as a int. Returns
  // false if there is no such parameter, or if it is not a int.
  bool GetIntegerParameter(const std::string& key, int* out) const;

  // Provides the command parameter with the given |key| as a Dictionary.
  // Returns false if there is no such parameter, or if it is not a Dictionary.
  bool GetDictionaryParameter(const std::string& key,
                              DictionaryValue** out) const;

  // Provides the command parameter with the given |key| as a list. Returns
  // false if there is no such parameter, or if it is not a list.
  bool GetListParameter(const std::string& key, ListValue** out) const;

 private:
  const std::vector<std::string> path_segments_;
  const scoped_ptr<const DictionaryValue> parameters_;

  // An autorelease pool must exist on any thread where Objective C is used,
  // even implicitly. Otherwise the warning:
  //   "Objects autoreleased with no pool in place."
  // is printed for every object deallocted.  Since every incomming command to
  // chrome driver is allocated a new thread, the release pool is declared here.
  base::mac::ScopedNSAutoreleasePool autorelease_pool;

  DISALLOW_COPY_AND_ASSIGN(Command);
};

}  // namespace webdriver

#endif  // CHROME_TEST_WEBDRIVER_COMMANDS_COMMAND_H_
