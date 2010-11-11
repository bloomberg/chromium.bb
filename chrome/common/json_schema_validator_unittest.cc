// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "chrome/common/json_schema_validator.h"
#include "chrome/common/json_schema_validator_unittest_base.h"
#include "testing/gtest/include/gtest/gtest.h"

class JSONSchemaValidatorCPPTest : public JSONSchemaValidatorTestBase {
 public:
  JSONSchemaValidatorCPPTest()
      : JSONSchemaValidatorTestBase(JSONSchemaValidatorTestBase::CPP) {
  }

 protected:
  virtual void ExpectValid(const std::string& test_source,
                           Value* instance, DictionaryValue* schema,
                           ListValue* types) {
    JSONSchemaValidator validator(schema, types);
    if (validator.Validate(instance))
      return;

    for (size_t i = 0; i < validator.errors().size(); ++i) {
      ADD_FAILURE() << test_source << ": "
                    << validator.errors()[i].path << ": "
                    << validator.errors()[i].message;
    }
  }

  virtual void ExpectNotValid(const std::string& test_source,
                              Value* instance, DictionaryValue* schema,
                              ListValue* types,
                              const std::string& expected_error_path,
                              const std::string& expected_error_message) {
    JSONSchemaValidator validator(schema, types);
    if (validator.Validate(instance)) {
      ADD_FAILURE() << test_source;
      return;
    }

    ASSERT_EQ(1u, validator.errors().size()) << test_source;
    EXPECT_EQ(expected_error_path, validator.errors()[0].path) << test_source;
    EXPECT_EQ(expected_error_message, validator.errors()[0].message)
        << test_source;
  }
};

TEST_F(JSONSchemaValidatorCPPTest, Test) {
  RunTests();
}
