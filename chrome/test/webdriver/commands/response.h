// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_WEBDRIVER_COMMANDS_RESPONSE_H_
#define CHROME_TEST_WEBDRIVER_COMMANDS_RESPONSE_H_

#include <sstream>
#include <string>

#include "base/basictypes.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/test/webdriver/error_codes.h"

namespace webdriver {

// All errors in webdriver must use this macro in order to send back
// a proper stack trace to the client
#define SET_WEBDRIVER_ERROR(response, msg, err) { \
  LOG(ERROR) << msg; \
  response->set_error(msg, __FILE__, __LINE__); \
  response->set_status(err); \
}

// A simple class that encapsulates the information describing the response to
// a |Command|. In Webdriver all responses must be sent back as a JSON value,
// conforming to the spec found at:
// http://code.google.com/p/selenium/wiki/JsonWireProtocol#Messages
class Response {
 public:
  inline Response() : status_(kSuccess) {
    set_value(Value::CreateNullValue());
  }

  inline ErrorCode status() const { return status_; }
  inline void set_status(const ErrorCode status) {
    status_ = status;
    data_.SetInteger(kStatusKey, status_);
  }

  // Ownership of the returned pointer is kept by the class and held in
  // the Dictiionary Value data_.
  inline const Value* value() const {
    Value* out = NULL;
    LOG_IF(WARNING, !data_.Get(kValueKey, &out))
        << "Accessing unset response value.";  // Should never happen.
    return out;
  }

  // Sets the |value| of this response, assuming ownership of the object in the
  // process.
  inline void set_value(Value* value) {
    data_.Set(kValueKey, value);
  }

  // Sets the |value| of this response, assuming ownership of the object in the
  // process.  This function is mostly used to report error values.
  inline void set_error(const char msg[], const char file[], const int line) {
    DictionaryValue* error = new DictionaryValue();
    DictionaryValue* stack = new DictionaryValue();

    error->SetString(std::string(kMessageKey), msg);
    stack->SetString(std::string(kFileName), std::string(file));
    stack->SetInteger(std::string(kLineNumber), line);

    error->Set(std::string(kStackTrace), stack);
    data_.Set(kValueKey, error);
  }

  // Sets a JSON field in this response. The |key| may be a "." delimitted
  // string to indicate the value should be set in a nested object. Any
  // previously set value for the |key| will be deleted.
  // This object assumes ownership of |in_value|.
  inline void SetField(const std::string& key, Value* value) {
    data_.Set(key, value);
  }

  // Returns this response as a JSON string.
  std::string ToJSON() const {
    std::string json;
    base::JSONWriter::Write(static_cast<const Value*>(&data_), false, &json);
    return json;
  }

 private:
  // The hard coded values for the keys below are set in the command.cc file.
  static const char* const kStatusKey;
  static const char* const kValueKey;

  // Optional values used for errors.
  static const char* const kMessageKey;
  static const char* const kScreenKey;
  static const char* const kClassKey;
  static const char* const kStackTrace;
  static const char* const kFileName;
  static const char* const kLineNumber;

  // The response status code. Stored outside of |data_| since it is
  // likely to be queried often.
  ErrorCode status_;
  DictionaryValue data_;

  DISALLOW_COPY_AND_ASSIGN(Response);
};

}  // namespace webdriver

#endif  // CHROME_TEST_WEBDRIVER_COMMANDS_RESPONSE_H_

