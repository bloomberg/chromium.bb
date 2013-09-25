// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profile_resetter/jtl_interpreter.h"

#include "base/strings/string_util.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/profile_resetter/jtl_foundation.h"
#include "chrome/browser/profile_resetter/jtl_instructions.h"
#include "crypto/hmac.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char seed[] = "foobar";

#define KEY_HASH_1 GetHash("KEY_HASH_1")
#define KEY_HASH_2 GetHash("KEY_HASH_2")
#define KEY_HASH_3 GetHash("KEY_HASH_3")
#define KEY_HASH_4 GetHash("KEY_HASH_4")

#define VALUE_HASH_1 GetHash("VALUE_HASH_1")
#define VALUE_HASH_2 GetHash("VALUE_HASH_2")

#define VAR_HASH_1 "01234567890123456789012345678901"
#define VAR_HASH_2 "12345678901234567890123456789012"

std::string GetHash(const std::string& input) {
  return jtl_foundation::Hasher(seed).GetHash(input);
}

// escaped_json_param may contain ' characters that are replaced with ". This
// makes the code more readable because we need less escaping.
#define INIT_INTERPRETER(program_param, escaped_json_param) \
    const char* escaped_json = escaped_json_param; \
    std::string json; \
    ReplaceChars(escaped_json, "'", "\"", &json); \
    scoped_ptr<Value> json_value(ParseJson(json)); \
    JtlInterpreter interpreter( \
        seed, \
        program_param, \
        static_cast<const DictionaryValue*>(json_value.get())); \
    interpreter.Execute()

using base::test::ParseJson;

TEST(JtlInterpreter, Store) {
  INIT_INTERPRETER(
      OP_STORE_BOOL(VAR_HASH_1, VALUE_TRUE),
      "{ 'KEY_HASH_1': 'VALUE_HASH_1' }");
  EXPECT_EQ(JtlInterpreter::OK, interpreter.result());
  base::ExpectDictBooleanValue(true, *interpreter.working_memory(), VAR_HASH_1);
}

TEST(JtlInterpreter, NavigateAndStore) {
  INIT_INTERPRETER(
      OP_NAVIGATE(KEY_HASH_1) +
      OP_STORE_BOOL(VAR_HASH_1, VALUE_TRUE),
      "{ 'KEY_HASH_1': 'VALUE_HASH_1' }");
  EXPECT_EQ(JtlInterpreter::OK, interpreter.result());
  base::ExpectDictBooleanValue(true, *interpreter.working_memory(), VAR_HASH_1);
}

TEST(JtlInterpreter, FailNavigate) {
  INIT_INTERPRETER(
      OP_NAVIGATE(KEY_HASH_2) +
      OP_STORE_BOOL(VAR_HASH_1, VALUE_TRUE),
      "{ 'KEY_HASH_1': 'VALUE_HASH_1' }");
  EXPECT_EQ(JtlInterpreter::OK, interpreter.result());
  EXPECT_FALSE(interpreter.working_memory()->HasKey(VAR_HASH_1));
}

TEST(JtlInterpreter, ConsecutiveNavigate) {
  INIT_INTERPRETER(
      OP_NAVIGATE(KEY_HASH_1) +
      OP_NAVIGATE(KEY_HASH_2) +
      OP_STORE_BOOL(VAR_HASH_1, VALUE_TRUE),
      "{ 'KEY_HASH_1': { 'KEY_HASH_2': 'VALUE_HASH_1' } }");
  EXPECT_EQ(JtlInterpreter::OK, interpreter.result());
  base::ExpectDictBooleanValue(true, *interpreter.working_memory(), VAR_HASH_1);
}

TEST(JtlInterpreter, FailConsecutiveNavigate) {
  INIT_INTERPRETER(
      OP_NAVIGATE(KEY_HASH_1) +
      OP_NAVIGATE(KEY_HASH_2) +
      OP_STORE_BOOL(VAR_HASH_1, VALUE_TRUE),
      "{ 'KEY_HASH_1': 'foo' }");
  EXPECT_EQ(JtlInterpreter::OK, interpreter.result());
  EXPECT_FALSE(interpreter.working_memory()->HasKey(VAR_HASH_1));
}

TEST(JtlInterpreter, NavigateAnyInDictionary) {
  INIT_INTERPRETER(
      OP_NAVIGATE(KEY_HASH_1) +
      OP_NAVIGATE_ANY +
      OP_NAVIGATE(KEY_HASH_4) +
      OP_STORE_BOOL(VAR_HASH_1, VALUE_TRUE),
          "{ 'KEY_HASH_1':"
          "  { 'KEY_HASH_2': {'KEY_HASH_3': 'VALUE_HASH_1' },"
          "    'KEY_HASH_3': {'KEY_HASH_4': 'VALUE_HASH_1' }"
          "  } }");
  EXPECT_EQ(JtlInterpreter::OK, interpreter.result());
  base::ExpectDictBooleanValue(true, *interpreter.working_memory(), VAR_HASH_1);
}

TEST(JtlInterpreter, NavigateAnyInList) {
  INIT_INTERPRETER(
      OP_NAVIGATE(KEY_HASH_1) +
      OP_NAVIGATE_ANY +
      OP_NAVIGATE(KEY_HASH_4) +
      OP_STORE_BOOL(VAR_HASH_1, VALUE_TRUE),
          "{ 'KEY_HASH_1':"
          "  [ {'KEY_HASH_3': 'VALUE_HASH_1' },"
          "    {'KEY_HASH_4': 'VALUE_HASH_1' }"
          "  ] }");
  EXPECT_EQ(JtlInterpreter::OK, interpreter.result());
  base::ExpectDictBooleanValue(true, *interpreter.working_memory(), VAR_HASH_1);
}

TEST(JtlInterpreter, NavigateBack) {
  INIT_INTERPRETER(
      OP_NAVIGATE(KEY_HASH_1) +
      OP_NAVIGATE(KEY_HASH_2) +
      OP_NAVIGATE_BACK +
      OP_NAVIGATE(KEY_HASH_3) +
      OP_NAVIGATE(KEY_HASH_4) +
      OP_STORE_BOOL(VAR_HASH_1, VALUE_TRUE),
          "{ 'KEY_HASH_1':"
          "  { 'KEY_HASH_2': {'KEY_HASH_3': 'VALUE_HASH_1' },"
          "    'KEY_HASH_3': {'KEY_HASH_4': 'VALUE_HASH_1' }"
          "  } }");
  EXPECT_EQ(JtlInterpreter::OK, interpreter.result());
  base::ExpectDictBooleanValue(true, *interpreter.working_memory(), VAR_HASH_1);
}

TEST(JtlInterpreter, StoreTwoValues) {
  INIT_INTERPRETER(
      OP_NAVIGATE(KEY_HASH_1) +
      OP_NAVIGATE(KEY_HASH_2) +
      OP_STORE_BOOL(VAR_HASH_1, VALUE_TRUE) +
      OP_STORE_HASH(VAR_HASH_2, VALUE_HASH_1),
      "{ 'KEY_HASH_1': { 'KEY_HASH_2': 'VALUE_HASH_1' } }");
  EXPECT_EQ(JtlInterpreter::OK, interpreter.result());
  base::ExpectDictBooleanValue(true, *interpreter.working_memory(), VAR_HASH_1);
  base::ExpectDictStringValue(VALUE_HASH_1, *interpreter.working_memory(),
                              VAR_HASH_2);
}

TEST(JtlInterpreter, CompareStoredMatch) {
  INIT_INTERPRETER(
      OP_NAVIGATE(KEY_HASH_1) +
      OP_STORE_BOOL(VAR_HASH_1, VALUE_TRUE) +
      OP_NAVIGATE(KEY_HASH_2) +
      OP_COMPARE_STORED_BOOL(VAR_HASH_1, VALUE_TRUE, VALUE_FALSE) +
      OP_STORE_BOOL(VAR_HASH_2, VALUE_TRUE),
      "{ 'KEY_HASH_1': { 'KEY_HASH_2': 'VALUE_HASH_1' } }");
  EXPECT_EQ(JtlInterpreter::OK, interpreter.result());
  base::ExpectDictBooleanValue(true, *interpreter.working_memory(), VAR_HASH_1);
  base::ExpectDictBooleanValue(true, *interpreter.working_memory(), VAR_HASH_2);
}

TEST(JtlInterpreter, CompareStoredMismatch) {
  INIT_INTERPRETER(
      OP_NAVIGATE(KEY_HASH_1) +
      OP_STORE_BOOL(VAR_HASH_1, VALUE_TRUE) +
      OP_NAVIGATE(KEY_HASH_2) +
      OP_COMPARE_STORED_BOOL(VAR_HASH_1, VALUE_FALSE, VALUE_TRUE) +
      OP_STORE_BOOL(VAR_HASH_2, VALUE_TRUE),
      "{ 'KEY_HASH_1': { 'KEY_HASH_2': 'VALUE_HASH_1' } }");
  EXPECT_EQ(JtlInterpreter::OK, interpreter.result());
  base::ExpectDictBooleanValue(true, *interpreter.working_memory(), VAR_HASH_1);
  EXPECT_FALSE(interpreter.working_memory()->HasKey(VAR_HASH_2));
}

TEST(JtlInterpreter, CompareStoredNoValueMatchingDefault) {
  INIT_INTERPRETER(
      OP_NAVIGATE(KEY_HASH_1) +
      OP_NAVIGATE(KEY_HASH_2) +
      OP_COMPARE_STORED_BOOL(VAR_HASH_1, VALUE_TRUE, VALUE_TRUE) +
      OP_STORE_BOOL(VAR_HASH_2, VALUE_TRUE),
      "{ 'KEY_HASH_1': { 'KEY_HASH_2': 'VALUE_HASH_1' } }");
  EXPECT_EQ(JtlInterpreter::OK, interpreter.result());
  base::ExpectDictBooleanValue(true, *interpreter.working_memory(), VAR_HASH_2);
}

TEST(JtlInterpreter, CompareStoredNoValueMismatchingDefault) {
  INIT_INTERPRETER(
      OP_NAVIGATE(KEY_HASH_1) +
      OP_NAVIGATE(KEY_HASH_2) +
      OP_COMPARE_STORED_BOOL(VAR_HASH_1, VALUE_TRUE, VALUE_FALSE) +
      OP_STORE_BOOL(VAR_HASH_2, VALUE_TRUE),
      "{ 'KEY_HASH_1': { 'KEY_HASH_2': 'VALUE_HASH_1' } }");
  EXPECT_EQ(JtlInterpreter::OK, interpreter.result());
  EXPECT_FALSE(interpreter.working_memory()->HasKey(VAR_HASH_2));
}

TEST(JtlInterpreter, CompareBoolMatch) {
  struct TestCase {
    std::string expected_value;
    const char* json;
    bool expected_success;
  } cases[] = {
    { VALUE_TRUE, "{ 'KEY_HASH_1': true }", true },
    { VALUE_FALSE, "{ 'KEY_HASH_1': false }", true },
    { VALUE_TRUE, "{ 'KEY_HASH_1': 'abc' }", false },
    { VALUE_TRUE, "{ 'KEY_HASH_1': 1 }", false },
    { VALUE_TRUE, "{ 'KEY_HASH_1': 1.2 }", false },
    { VALUE_TRUE, "{ 'KEY_HASH_1': [1] }", false },
    { VALUE_TRUE, "{ 'KEY_HASH_1': {'a': 'b'} }", false },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    SCOPED_TRACE(testing::Message() << "Iteration " << i);
    INIT_INTERPRETER(
        OP_NAVIGATE(KEY_HASH_1) +
        OP_COMPARE_NODE_BOOL(cases[i].expected_value) +
        OP_STORE_BOOL(VAR_HASH_1, VALUE_TRUE),
        cases[i].json);
    EXPECT_EQ(JtlInterpreter::OK, interpreter.result());
    if (cases[i].expected_success) {
      base::ExpectDictBooleanValue(
          true, *interpreter.working_memory(), VAR_HASH_1);
    } else {
      EXPECT_FALSE(interpreter.working_memory()->HasKey(VAR_HASH_1));
    }
  }
}

TEST(JtlInterpreter, CompareHashString) {
  struct TestCase {
    std::string expected_value;
    const char* json;
    bool expected_success;
  } cases[] = {
    { VALUE_HASH_1, "{ 'KEY_HASH_1': 'VALUE_HASH_1' }", true },
    { VALUE_HASH_1, "{ 'KEY_HASH_1': 'VALUE_HASH_2' }", false },
    { VALUE_HASH_1, "{ 'KEY_HASH_1': true }", false },
    { VALUE_HASH_1, "{ 'KEY_HASH_1': 1 }", false },
    { VALUE_HASH_1, "{ 'KEY_HASH_1': 1.1 }", false },
    { VALUE_HASH_1, "{ 'KEY_HASH_1': [1] }", false },
    { VALUE_HASH_1, "{ 'KEY_HASH_1': {'a': 'b'} }", false },

    { GetHash("1.2"), "{ 'KEY_HASH_1': 1.2 }", true },
    { GetHash("1.2"), "{ 'KEY_HASH_1': 'VALUE_HASH_1' }", false },
    { GetHash("1.2"), "{ 'KEY_HASH_1': true }", false },
    { GetHash("1.2"), "{ 'KEY_HASH_1': 1 }", false },
    { GetHash("1.2"), "{ 'KEY_HASH_1': 1.3 }", false },
    { GetHash("1.2"), "{ 'KEY_HASH_1': [1] }", false },
    { GetHash("1.2"), "{ 'KEY_HASH_1': {'a': 'b'} }", false },

    { GetHash("1"), "{ 'KEY_HASH_1': 1 }", true },
    { GetHash("1"), "{ 'KEY_HASH_1': 'VALUE_HASH_1' }", false },
    { GetHash("1"), "{ 'KEY_HASH_1': true }", false },
    { GetHash("1"), "{ 'KEY_HASH_1': 2 }", false },
    { GetHash("1"), "{ 'KEY_HASH_1': 1.1 }", false },
    { GetHash("1"), "{ 'KEY_HASH_1': [1] }", false },
    { GetHash("1"), "{ 'KEY_HASH_1': {'a': 'b'} }", false },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    SCOPED_TRACE(testing::Message() << "Iteration " << i);
    INIT_INTERPRETER(
        OP_NAVIGATE(KEY_HASH_1) +
        OP_COMPARE_NODE_HASH(cases[i].expected_value) +
        OP_STORE_BOOL(VAR_HASH_1, VALUE_TRUE),
        cases[i].json);
    EXPECT_EQ(JtlInterpreter::OK, interpreter.result());
    if (cases[i].expected_success) {
      base::ExpectDictBooleanValue(
          true, *interpreter.working_memory(), VAR_HASH_1);
    } else {
      EXPECT_FALSE(interpreter.working_memory()->HasKey(VAR_HASH_1));
    }
  }
}

TEST(JtlInterpreter, Stop) {
  INIT_INTERPRETER(
      OP_NAVIGATE(KEY_HASH_1) +
      OP_NAVIGATE(KEY_HASH_2) +
      OP_STORE_BOOL(VAR_HASH_1, VALUE_TRUE) +
      OP_STOP_EXECUTING_SENTENCE +
      OP_STORE_BOOL(VAR_HASH_2, VALUE_TRUE),
      "{ 'KEY_HASH_1': { 'KEY_HASH_2': 'VALUE_HASH_1' } }");
  EXPECT_EQ(JtlInterpreter::OK, interpreter.result());
  base::ExpectDictBooleanValue(true, *interpreter.working_memory(), VAR_HASH_1);
  EXPECT_FALSE(interpreter.working_memory()->HasKey(VAR_HASH_2));
}

TEST(JtlInterpreter, EndOfSentence) {
  INIT_INTERPRETER(
      OP_NAVIGATE(KEY_HASH_1) +
      OP_NAVIGATE(KEY_HASH_2) +
      OP_STORE_BOOL(VAR_HASH_1, VALUE_TRUE) +
      OP_END_OF_SENTENCE +
      OP_STORE_BOOL(VAR_HASH_2, VALUE_TRUE),
      "{ 'KEY_HASH_1': { 'KEY_HASH_2': 'VALUE_HASH_1' } }");
  EXPECT_EQ(JtlInterpreter::OK, interpreter.result());
  base::ExpectDictBooleanValue(true, *interpreter.working_memory(), VAR_HASH_1);
  base::ExpectDictBooleanValue(true, *interpreter.working_memory(), VAR_HASH_2);
}

TEST(JtlInterpreter, InvalidBack) {
  INIT_INTERPRETER(
      OP_NAVIGATE(KEY_HASH_1) +
      OP_NAVIGATE_BACK +
      OP_NAVIGATE_BACK,
      "{ 'KEY_HASH_1': { 'KEY_HASH_2': 'VALUE_HASH_1' } }");
  EXPECT_EQ(JtlInterpreter::RUNTIME_ERROR, interpreter.result());
}

TEST(JtlInterpreter, IncorrectPrograms) {
  std::string missing_hash;
  std::string missing_bool;
  std::string invalid_hash("123");
  std::string invalid_bool("\x02", 1);
  std::string invalid_operation("\x99", 1);
  std::string programs[] = {
    OP_NAVIGATE(missing_hash),
    OP_NAVIGATE(invalid_hash),
    OP_STORE_BOOL(VAR_HASH_1, invalid_bool),
    OP_STORE_BOOL(missing_hash, VALUE_TRUE),
    OP_STORE_BOOL(invalid_hash, VALUE_TRUE),
    OP_COMPARE_STORED_BOOL(invalid_hash, VALUE_TRUE, VALUE_TRUE),
    OP_COMPARE_STORED_BOOL(VAR_HASH_1, invalid_bool, VALUE_TRUE),
    OP_COMPARE_STORED_BOOL(VAR_HASH_1, VALUE_TRUE, invalid_bool),
    OP_COMPARE_STORED_BOOL(VAR_HASH_1, VALUE_TRUE, missing_bool),
    OP_COMPARE_NODE_BOOL(missing_bool),
    OP_COMPARE_NODE_BOOL(invalid_bool),
    OP_COMPARE_NODE_HASH(missing_hash),
    OP_COMPARE_NODE_HASH(invalid_hash),
    invalid_operation,
  };
  for (size_t i = 0; i < arraysize(programs); ++i) {
    SCOPED_TRACE(testing::Message() << "Iteration " << i);
    INIT_INTERPRETER(programs[i],
                     "{ 'KEY_HASH_1': { 'KEY_HASH_2': 'VALUE_HASH_1' } }");
    EXPECT_EQ(JtlInterpreter::PARSE_ERROR, interpreter.result());
  }
}

TEST(JtlInterpreter, GetOutput) {
  INIT_INTERPRETER(
      OP_STORE_BOOL(GetHash("output1"), VALUE_TRUE) +
      OP_STORE_HASH(GetHash("output2"), VALUE_HASH_1),
      "{}");
  bool output1 = false;
  std::string output2;
  EXPECT_TRUE(interpreter.GetOutputBoolean("output1", &output1));
  EXPECT_EQ(true, output1);
  EXPECT_TRUE(interpreter.GetOutputString("output2", &output2));
  EXPECT_EQ(VALUE_HASH_1, output2);
  EXPECT_FALSE(interpreter.GetOutputBoolean("outputxx", &output1));
  EXPECT_FALSE(interpreter.GetOutputString("outputxx", &output2));
}

}  // namespace
