// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/commands/command.h"

namespace webdriver {

const char* const Response::kStatusKey = "status";
const char* const Response::kValueKey = "value";

bool Command::GetStringASCIIParameter(const std::string& key,
                                      std::string* out) const {
  return parameters_.get() != NULL && parameters_->GetStringASCII(key, out);
}

}  // namespace webdriver

