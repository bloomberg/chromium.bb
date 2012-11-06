// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/network_settings/onc_test_utils.h"

#include "base/file_path.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/values.h"
#include "chrome/common/chrome_paths.h"

namespace chromeos {
namespace onc {
namespace test_utils {

scoped_ptr<base::DictionaryValue> ReadTestDictionary(
    const std::string& filename) {
  FilePath path;
  PathService::Get(chrome::DIR_TEST_DATA, &path);
  path = path.AppendASCII("chromeos").AppendASCII("network_settings").
      Append(filename);
  JSONFileValueSerializer serializer(path);
  serializer.set_allow_trailing_comma(true);

  std::string error_message;
  base::Value* content = serializer.Deserialize(NULL, &error_message);
  CHECK(content != NULL) << "Couldn't json-deserialize file '"
                         << filename << "': " << error_message;

  base::DictionaryValue* dict = NULL;
  CHECK(content->GetAsDictionary(&dict))
      << "File '" << filename
      << "' does not contain a dictionary as expected, but type "
      << content->GetType();
  return make_scoped_ptr(dict);
}

::testing::AssertionResult Equals(const base::DictionaryValue* expected,
                                  const base::DictionaryValue* actual) {
  CHECK(expected != NULL);
  if (actual == NULL)
    return ::testing::AssertionFailure() << "Actual dictionary pointer is NULL";

  if (expected->Equals(actual))
    return ::testing::AssertionSuccess() << "Dictionaries are equal";

  return ::testing::AssertionFailure() << "Dictionaries are unequal.\n"
                                       << "Expected dictionary:\n" << *expected
                                       << "Actual dictionary:\n" << *actual;
}

}  // namespace test_utils
}  // namespace onc
}  // namespace chromeos
