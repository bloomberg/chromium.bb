// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/json_parser/test_json_parser.h"

#include "base/json/json_reader.h"
#include "base/values.h"
#include "chrome/chrome_cleaner/json_parser/json_parser_impl.h"

namespace chrome_cleaner {

void TestJsonParser::Parse(const std::string& json,
                           ParseDoneCallback callback) {
  int error_code;
  std::string error;
  std::unique_ptr<base::Value> value = base::JSONReader::ReadAndReturnError(
      json,
      base::JSON_ALLOW_TRAILING_COMMAS | base::JSON_REPLACE_INVALID_CHARACTERS,
      &error_code, &error);
  if (value) {
    std::move(callback).Run(base::make_optional(std::move(*value)),
                            base::nullopt);
  } else {
    std::move(callback).Run(base::nullopt,
                            base::make_optional(std::move(error)));
  }
}

}  // namespace chrome_cleaner
