// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_JSON_SCHEMA_VALIDATOR_UNITTEST_BASE_H_
#define CHROME_COMMON_JSON_SCHEMA_VALIDATOR_UNITTEST_BASE_H_

#include "testing/gtest/include/gtest/gtest.h"

class DictionaryValue;
class ListValue;
class Value;

// Base class for unit tests for JSONSchemaValidator. There is currently only
// one implementation, JSONSchemaValidatorCPPTest.
//
// TODO(aa): Refactor chrome/test/data/json_schema_test.js into
// JSONSchemaValidatorJSTest that inherits from this.
class JSONSchemaValidatorTestBase : public testing::Test {
 public:
  enum ValidatorType {
    CPP = 1,
    JS = 2
  };

  explicit JSONSchemaValidatorTestBase(ValidatorType type);

  void RunTests();

 protected:
  virtual void ExpectValid(const std::string& test_source,
                           Value* instance, DictionaryValue* schema,
                           ListValue* types) = 0;

  virtual void ExpectNotValid(const std::string& test_source,
                              Value* instance, DictionaryValue* schema,
                              ListValue* types,
                              const std::string& expected_error_path,
                              const std::string& expected_error_message) = 0;

 private:
  void TestComplex();
  void TestStringPattern();
  void TestEnum();
  void TestChoices();
  void TestExtends();
  void TestObject();
  void TestTypeReference();
  void TestArrayTuple();
  void TestArrayNonTuple();
  void TestString();
  void TestNumber();
  void TestTypeClassifier();
  void TestTypes();

  ValidatorType type_;
};

#endif  // CHROME_COMMON_JSON_SCHEMA_VALIDATOR_UNITTEST_BASE_H_
