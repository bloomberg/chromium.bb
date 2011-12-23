// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/commands/keys_command.h"

#include "base/values.h"
#include "chrome/test/webdriver/commands/response.h"
#include "chrome/test/webdriver/webdriver_error.h"
#include "chrome/test/webdriver/webdriver_session.h"
#include "chrome/test/webdriver/webdriver_util.h"

namespace webdriver {

KeysCommand::KeysCommand(const std::vector<std::string>& path_segments,
                         const DictionaryValue* const parameters)
    : WebDriverCommand(path_segments, parameters) {}

KeysCommand::~KeysCommand() {}

bool KeysCommand::DoesPost() {
  return true;
}

void KeysCommand::ExecutePost(Response* const response) {
  ListValue* key_list;
  if (!GetListParameter("value", &key_list)) {
    response->SetError(new Error(
        kBadRequest, "Missing or invalid 'value' parameter"));
    return;
  }

  // Flatten the given array of strings into one.
  string16 keys;
  Error* error = FlattenStringArray(key_list, &keys);
  if (error) {
    response->SetError(error);
    return;
  }

  error = session_->SendKeys(keys);
  if (error) {
    response->SetError(error);
    return;
  }
}

}  // namespace webdriver
