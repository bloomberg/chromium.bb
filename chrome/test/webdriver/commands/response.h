// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_WEBDRIVER_COMMANDS_RESPONSE_H_
#define CHROME_TEST_WEBDRIVER_COMMANDS_RESPONSE_H_

#include <sstream>
#include <string>

#include "base/values.h"
#include "chrome/test/webdriver/webdriver_error.h"

namespace webdriver {

// A simple class that encapsulates the information describing the response to
// a |Command|. In Webdriver all responses must be sent back as a JSON value,
// conforming to the spec found at:
// http://code.google.com/p/selenium/wiki/JsonWireProtocol#Messages
class Response {
 public:
  // Creates a new |Response| with a default status of |kSuccess| and a
  // |NullValue|.
  Response();
  ~Response();

  ErrorCode GetStatus() const;
  void SetStatus(ErrorCode status);

  // Ownership of the returned pointer is kept by this object.
  const Value* GetValue() const;

  // Sets the |value| of this response, assuming ownership of the object in the
  // process.
  void SetValue(Value* value);

  // Configures this response to report the given error. Ownership of the error
  // is taken from the caller.
  void SetError(Error* error);

  // Sets a JSON field in this response. The |key| may be a "." delimitted
  // string to indicate the value should be set in a nested object. Any
  // previously set value for the |key| will be deleted.
  // This object assumes ownership of |value|.
  void SetField(const std::string& key, Value* value);

  // Returns this response as a JSON string.
  std::string ToJSON() const;

 private:
  DictionaryValue data_;

  DISALLOW_COPY_AND_ASSIGN(Response);
};

}  // namespace webdriver

#endif  // CHROME_TEST_WEBDRIVER_COMMANDS_RESPONSE_H_
