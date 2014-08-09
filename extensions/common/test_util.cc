// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/test_util.h"

#include "base/json/json_reader.h"
#include "base/values.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace test_util {

ExtensionBuilder& BuildExtension(ExtensionBuilder& builder) {
  return builder
         .SetManifest(DictionaryBuilder()
                      .Set("name", "Test extension")
                      .Set("version", "1.0")
                      .Set("manifest_version", 2));
}

scoped_refptr<Extension> CreateEmptyExtension() {
  scoped_refptr<Extension> empty_extension(
      ExtensionBuilder()
          .SetManifest(
               DictionaryBuilder().Set("name", "Test").Set("version", "1.0"))
          .Build());
  return empty_extension;
}

scoped_refptr<Extension> CreateExtensionWithID(const std::string& id) {
  return ExtensionBuilder()
      .SetManifest(
           DictionaryBuilder().Set("name", "test").Set("version", "0.1"))
      .SetID(id)
      .Build();
}

scoped_ptr<base::DictionaryValue> ParseJsonDictionaryWithSingleQuotes(
    std::string json) {
  std::replace(json.begin(), json.end(), '\'', '"');
  std::string error_msg;
  scoped_ptr<base::Value> result(base::JSONReader::ReadAndReturnError(
      json, base::JSON_ALLOW_TRAILING_COMMAS, NULL, &error_msg));
  scoped_ptr<base::DictionaryValue> result_dict;
  if (result && result->IsType(base::Value::TYPE_DICTIONARY)) {
    result_dict.reset(static_cast<base::DictionaryValue*>(result.release()));
  } else {
    ADD_FAILURE() << "Failed to parse \"" << json << "\": " << error_msg;
    result_dict.reset(new base::DictionaryValue());
  }
  return result_dict.Pass();
}

}  // namespace test_util
}  // namespace extensions
