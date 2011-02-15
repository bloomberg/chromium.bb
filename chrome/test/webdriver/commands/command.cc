// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/commands/command.h"

namespace webdriver {

// Error message taken from:
// http://code.google.com/p/selenium/wiki/JsonWireProtocol#Response_Status_Codes
const char* const Response::kStatusKey = "status";
const char* const Response::kValueKey = "value";
const char* const Response::kMessageKey = "message";
const char* const Response::kScreenKey = "screen";
const char* const Response::kClassKey = "class";
const char* const Response::kStackTrace = "stackTrace";
const char* const Response::kFileName = "fileName";
const char* const Response::kLineNumber = "lineNumber";

Command::Command(const std::vector<std::string>& path_segments,
                 const DictionaryValue* const parameters)
    : path_segments_(path_segments),
      parameters_(parameters) {}

Command::~Command() {}

bool Command::DoesDelete() {
  return false;
}

bool Command::DoesGet() {
  return false;
}

bool Command::DoesPost() {
  return false;
}

bool Command::Init(Response* const response) {
  return true;
}

bool Command::GetStringParameter(const std::string& key,
                                 string16* out) const {
  return parameters_.get() != NULL && parameters_->GetString(key, out);
}

bool Command::GetStringParameter(const std::string& key,
                                 std::string* out) const {
  return parameters_.get() != NULL && parameters_->GetString(key, out);
}

bool Command::GetStringASCIIParameter(const std::string& key,
                                      std::string* out) const {
  return parameters_.get() != NULL && parameters_->GetStringASCII(key, out);
}

bool Command::GetBooleanParameter(const std::string& key,
                                  bool* out) const {
  return parameters_.get() != NULL && parameters_->GetBoolean(key, out);
}

bool Command::GetIntegerParameter(const std::string& key,
                                  int* out) const {
  return parameters_.get() != NULL && parameters_->GetInteger(key, out);
}

bool Command::GetDictionaryParameter(const std::string& key,
                                     DictionaryValue** out) const {
  return parameters_.get() != NULL && parameters_->GetDictionary(key, out);
}

bool Command::GetListParameter(const std::string& key,
                               ListValue** out) const {
  return parameters_.get() != NULL && parameters_->GetList(key, out);
}

}  // namespace webdriver

