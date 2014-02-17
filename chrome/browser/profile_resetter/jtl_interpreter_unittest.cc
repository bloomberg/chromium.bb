// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profile_resetter/jtl_interpreter.h"

#include <numeric>

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

std::string EncodeUint32(uint32 value) {
  std::string bytecode;
  for (int i = 0; i < 4; ++i) {
    bytecode.push_back(static_cast<char>(value & 0xFFu));
    value >>= 8;
  }
  return bytecode;
}

// escaped_json_param may contain ' characters that are replaced with ". This
// makes the code more readable because we need less escaping.
#define INIT_INTERPRETER(program_param, escaped_json_param) \
    const char* escaped_json = escaped_json_param; \
    std::string json; \
    base::ReplaceChars(escaped_json, "'", "\"", &json); \
    scoped_ptr<base::Value> json_value(ParseJson(json)); \
    JtlInterpreter interpreter( \
        seed, \
        program_param, \
        static_cast<const base::DictionaryValue*>(json_value.get())); \
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

TEST(JtlInterpreter, CompareBool) {
  struct TestCase {
    std::string expected_value;
    const char* json;
    bool expected_success;
  } cases[] = {
    { VALUE_TRUE, "{ 'KEY_HASH_1': true }", true },
    { VALUE_FALSE, "{ 'KEY_HASH_1': false }", true },
    { VALUE_TRUE, "{ 'KEY_HASH_1': false }", false },
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

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    SCOPED_TRACE(testing::Message() << "Negated, Iteration " << i);
    INIT_INTERPRETER(
        OP_NAVIGATE(KEY_HASH_1) +
        OP_COMPARE_NODE_HASH_NOT(cases[i].expected_value) +
        OP_STORE_BOOL(VAR_HASH_1, VALUE_TRUE),
        cases[i].json);
    EXPECT_EQ(JtlInterpreter::OK, interpreter.result());
    if (!cases[i].expected_success) {
      base::ExpectDictBooleanValue(
          true, *interpreter.working_memory(), VAR_HASH_1);
    } else {
      EXPECT_FALSE(interpreter.working_memory()->HasKey(VAR_HASH_1));
    }
  }
}

TEST(JtlInterpreter, StoreNodeBool) {
  struct TestCase {
    bool expected_value;
    const char* json;
    bool expected_success;
  } cases[] = {
    { true, "{ 'KEY_HASH_1': true }", true },
    { false, "{ 'KEY_HASH_1': false }", true },
    { false, "{ 'KEY_HASH_1': 'abc' }", false },
    { false, "{ 'KEY_HASH_1': 1 }", false },
    { false, "{ 'KEY_HASH_1': 1.2 }", false },
    { false, "{ 'KEY_HASH_1': [1] }", false },
    { false, "{ 'KEY_HASH_1': {'a': 'b'} }", false },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    SCOPED_TRACE(testing::Message() << "Iteration " << i);
    INIT_INTERPRETER(
        OP_NAVIGATE(KEY_HASH_1) +
        OP_STORE_NODE_BOOL(VAR_HASH_1) +
        OP_STORE_BOOL(VAR_HASH_2, VALUE_TRUE),
        cases[i].json);
    EXPECT_EQ(JtlInterpreter::OK, interpreter.result());
    if (cases[i].expected_success) {
      base::ExpectDictBooleanValue(
          cases[i].expected_value, *interpreter.working_memory(), VAR_HASH_1);
      base::ExpectDictBooleanValue(
          true, *interpreter.working_memory(), VAR_HASH_2);
    } else {
      EXPECT_FALSE(interpreter.working_memory()->HasKey(VAR_HASH_1));
      EXPECT_FALSE(interpreter.working_memory()->HasKey(VAR_HASH_2));
    }
  }
}

TEST(JtlInterpreter, CompareNodeToStoredBool) {
  struct TestCase {
    std::string stored_value;
    const char* json;
    bool expected_success;
  } cases[] = {
    { VALUE_TRUE, "{ 'KEY_HASH_1': true }", true },
    { VALUE_FALSE, "{ 'KEY_HASH_1': false }", true },
    { VALUE_FALSE, "{ 'KEY_HASH_1': true }", false },
    { std::string(), "{ 'KEY_HASH_1': true }", false },

    { VALUE_HASH_1, "{ 'KEY_HASH_1': 'VALUE_HASH_1' }", false },
    { GetHash("1"), "{ 'KEY_HASH_1': 1 }", false },
    { GetHash("1.2"), "{ 'KEY_HASH_1': 1.2 }", false },

    { VALUE_HASH_1, "{ 'KEY_HASH_1': true }", false },
    { GetHash("1"), "{ 'KEY_HASH_1': true }", false },
    { GetHash("1.2"), "{ 'KEY_HASH_1': true }", false },

    { VALUE_TRUE, "{ 'KEY_HASH_1': 'VALUE_HASH_1' }", false },
    { VALUE_TRUE, "{ 'KEY_HASH_1': 1 }", false },
    { VALUE_TRUE, "{ 'KEY_HASH_1': 1.2 }", false },
    { VALUE_TRUE, "{ 'KEY_HASH_1': [1] }", false },
    { VALUE_TRUE, "{ 'KEY_HASH_1': {'a': 'b'} }", false },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    SCOPED_TRACE(testing::Message() << "Iteration " << i);
    std::string store_op;
    if (cases[i].stored_value == VALUE_TRUE ||
        cases[i].stored_value == VALUE_FALSE)
      store_op = OP_STORE_BOOL(VAR_HASH_1, cases[i].stored_value);
    else if (!cases[i].stored_value.empty())
      store_op = OP_STORE_HASH(VAR_HASH_1, cases[i].stored_value);
    INIT_INTERPRETER(
        store_op +
        OP_NAVIGATE(KEY_HASH_1) +
        OP_COMPARE_NODE_TO_STORED_BOOL(VAR_HASH_1) +
        OP_STORE_BOOL(VAR_HASH_2, VALUE_TRUE),
        cases[i].json);
    EXPECT_EQ(JtlInterpreter::OK, interpreter.result());
    if (cases[i].expected_success) {
      base::ExpectDictBooleanValue(
          true, *interpreter.working_memory(), VAR_HASH_2);
    } else {
      EXPECT_FALSE(interpreter.working_memory()->HasKey(VAR_HASH_2));
    }
  }
}

TEST(JtlInterpreter, StoreNodeHash) {
  struct TestCase {
    std::string expected_value;
    const char* json;
    bool expected_success;
  } cases[] = {
    { VALUE_HASH_1, "{ 'KEY_HASH_1': 'VALUE_HASH_1' }", true },
    { VALUE_HASH_2, "{ 'KEY_HASH_1': 'VALUE_HASH_2' }", true },
    { GetHash("1"), "{ 'KEY_HASH_1': 1 }", true },
    { GetHash("1.2"), "{ 'KEY_HASH_1': 1.2 }", true },
    { std::string(), "{ 'KEY_HASH_1': true }", false },
    { std::string(), "{ 'KEY_HASH_1': [1] }", false },
    { std::string(), "{ 'KEY_HASH_1': {'a': 'b'} }", false },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    SCOPED_TRACE(testing::Message() << "Iteration " << i);
    INIT_INTERPRETER(
        OP_NAVIGATE(KEY_HASH_1) +
        OP_STORE_NODE_HASH(VAR_HASH_1) +
        OP_STORE_BOOL(VAR_HASH_2, VALUE_TRUE),
        cases[i].json);
    EXPECT_EQ(JtlInterpreter::OK, interpreter.result());
    if (cases[i].expected_success) {
      base::ExpectDictStringValue(
          cases[i].expected_value, *interpreter.working_memory(), VAR_HASH_1);
      base::ExpectDictBooleanValue(
          true, *interpreter.working_memory(), VAR_HASH_2);
    } else {
      EXPECT_FALSE(interpreter.working_memory()->HasKey(VAR_HASH_1));
      EXPECT_FALSE(interpreter.working_memory()->HasKey(VAR_HASH_2));
    }
  }
}

TEST(JtlInterpreter, CompareNodeToStoredHash) {
  struct TestCase {
    std::string stored_value;
    const char* json;
    bool expected_success;
  } cases[] = {
    { VALUE_HASH_1, "{ 'KEY_HASH_1': 'VALUE_HASH_1' }", true },
    { VALUE_HASH_2, "{ 'KEY_HASH_1': 'VALUE_HASH_2' }", true },
    { std::string(), "{ 'KEY_HASH_1': 'VALUE_HASH_2' }", false },
    { VALUE_HASH_1, "{ 'KEY_HASH_1': 'VALUE_HASH_2' }", false },
    { VALUE_HASH_1, "{ 'KEY_HASH_1': true }", false },
    { VALUE_HASH_1, "{ 'KEY_HASH_1': 1 }", false },
    { VALUE_HASH_1, "{ 'KEY_HASH_1': 1.1 }", false },
    { VALUE_HASH_1, "{ 'KEY_HASH_1': [1] }", false },
    { VALUE_HASH_1, "{ 'KEY_HASH_1': {'a': 'b'} }", false },

    { GetHash("1.2"), "{ 'KEY_HASH_1': 1.2 }", true },
    { GetHash("1.3"), "{ 'KEY_HASH_1': 1.3 }", true },
    { std::string(), "{ 'KEY_HASH_1': 1.2 }", false },
    { GetHash("1.2"), "{ 'KEY_HASH_1': 'VALUE_HASH_1' }", false },
    { GetHash("1.2"), "{ 'KEY_HASH_1': true }", false },
    { GetHash("1.2"), "{ 'KEY_HASH_1': 1 }", false },
    { GetHash("1.2"), "{ 'KEY_HASH_1': 1.3 }", false },
    { GetHash("1.2"), "{ 'KEY_HASH_1': [1] }", false },
    { GetHash("1.2"), "{ 'KEY_HASH_1': {'a': 'b'} }", false },

    { GetHash("1"), "{ 'KEY_HASH_1': 1 }", true },
    { GetHash("2"), "{ 'KEY_HASH_1': 2 }", true },
    { std::string(), "{ 'KEY_HASH_1': 2 }", false },
    { GetHash("1"), "{ 'KEY_HASH_1': 'VALUE_HASH_1' }", false },
    { GetHash("1"), "{ 'KEY_HASH_1': true }", false },
    { GetHash("1"), "{ 'KEY_HASH_1': 2 }", false },
    { GetHash("1"), "{ 'KEY_HASH_1': 1.1 }", false },
    { GetHash("1"), "{ 'KEY_HASH_1': [1] }", false },
    { GetHash("1"), "{ 'KEY_HASH_1': {'a': 'b'} }", false },

    { VALUE_TRUE, "{ 'KEY_HASH_1': 'VALUE_HASH_1' }", false },
    { VALUE_TRUE, "{ 'KEY_HASH_1': 1 }", false },
    { VALUE_TRUE, "{ 'KEY_HASH_1': 1.3 }", false },
    { VALUE_TRUE, "{ 'KEY_HASH_1': [1] }", false },
    { VALUE_TRUE, "{ 'KEY_HASH_1': {'a': 'b'} }", false },

    { VALUE_TRUE, "{ 'KEY_HASH_1': true }", false },
    { VALUE_FALSE, "{ 'KEY_HASH_1': false }", false },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    SCOPED_TRACE(testing::Message() << "Iteration " << i);
    std::string store_op;
    if (cases[i].stored_value == VALUE_TRUE ||
        cases[i].stored_value == VALUE_FALSE)
      store_op = OP_STORE_BOOL(VAR_HASH_1, cases[i].stored_value);
    else if (!cases[i].stored_value.empty())
      store_op = OP_STORE_HASH(VAR_HASH_1, cases[i].stored_value);
    INIT_INTERPRETER(
        store_op +
        OP_NAVIGATE(KEY_HASH_1) +
        OP_COMPARE_NODE_TO_STORED_HASH(VAR_HASH_1) +
        OP_STORE_BOOL(VAR_HASH_2, VALUE_TRUE),
        cases[i].json);
    EXPECT_EQ(JtlInterpreter::OK, interpreter.result());
    if (cases[i].expected_success) {
      base::ExpectDictBooleanValue(
          true, *interpreter.working_memory(), VAR_HASH_2);
    } else {
      EXPECT_FALSE(interpreter.working_memory()->HasKey(VAR_HASH_2));
    }
  }
}

TEST(JtlInterpreter, CompareSubstring) {
  struct TestCase {
    std::string pattern;
    const char* json;
    bool expected_success;
  } cases[] = {
    { "abc", "{ 'KEY_HASH_1': 'abcdefghijklmnopqrstuvwxyz' }", true },
    { "xyz", "{ 'KEY_HASH_1': 'abcdefghijklmnopqrstuvwxyz' }", true },
    { "m", "{ 'KEY_HASH_1': 'abcdefghijklmnopqrstuvwxyz' }", true },
    { "abc", "{ 'KEY_HASH_1': 'abc' }", true },
    { "cba", "{ 'KEY_HASH_1': 'abcdefghijklmnopqrstuvwxyz' }", false },
    { "acd", "{ 'KEY_HASH_1': 'abcdefghijklmnopqrstuvwxyz' }", false },
    { "waaaaaaay_too_long", "{ 'KEY_HASH_1': 'abc' }", false },

    { VALUE_HASH_1, "{ 'KEY_HASH_1': true }", false },
    { VALUE_HASH_1, "{ 'KEY_HASH_1': 1 }", false },
    { VALUE_HASH_1, "{ 'KEY_HASH_1': 1.1 }", false },
    { VALUE_HASH_1, "{ 'KEY_HASH_1': [1] }", false },
    { VALUE_HASH_1, "{ 'KEY_HASH_1': {'a': 'b'} }", false },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    SCOPED_TRACE(testing::Message() << "Iteration " << i);
    std::string pattern = cases[i].pattern;
    uint32 pattern_sum = std::accumulate(
        pattern.begin(), pattern.end(), static_cast<uint32>(0u));
    INIT_INTERPRETER(
        OP_NAVIGATE(KEY_HASH_1) +
        OP_COMPARE_NODE_SUBSTRING(GetHash(pattern),
                                  EncodeUint32(pattern.size()),
                                  EncodeUint32(pattern_sum)) +
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

TEST(JtlInterpreter, StoreNodeRegisterableDomainHash) {
  struct TestCase {
    std::string expected_value;
    const char* json;
    bool expected_success;
  } cases[] = {
    { GetHash("google"), "{ 'KEY_HASH_1': 'http://google.com/path' }", true },
    { GetHash("google"), "{ 'KEY_HASH_1': 'http://mail.google.com/' }", true },
    { GetHash("google"), "{ 'KEY_HASH_1': 'http://google.co.uk/' }", true },
    { GetHash("google"), "{ 'KEY_HASH_1': 'http://google.com./' }", true },
    { GetHash("google"), "{ 'KEY_HASH_1': 'http://..google.com/' }", true },

    { GetHash("foo"), "{ 'KEY_HASH_1': 'http://foo.bar/path' }", true },
    { GetHash("foo"), "{ 'KEY_HASH_1': 'http://sub.foo.bar' }", true },
    { GetHash("foo"), "{ 'KEY_HASH_1': 'http://foo.appspot.com/' }", true },
    { GetHash("foo"), "{ 'KEY_HASH_1': 'http://sub.foo.appspot.com' }", true },

    { std::string(), "{ 'KEY_HASH_1': 'http://google.com../' }", false },

    { std::string(), "{ 'KEY_HASH_1': 'http://bar/path' }", false },
    { std::string(), "{ 'KEY_HASH_1': 'http://co.uk/path' }", false },
    { std::string(), "{ 'KEY_HASH_1': 'http://appspot.com/path' }", false },
    { std::string(), "{ 'KEY_HASH_1': 'http://127.0.0.1/path' }", false },
    { std::string(), "{ 'KEY_HASH_1': 'file:///C:/bar.html' }", false },

    { std::string(), "{ 'KEY_HASH_1': 1 }", false },
    { std::string(), "{ 'KEY_HASH_1': 1.2 }", false },
    { std::string(), "{ 'KEY_HASH_1': true }", false },
    { std::string(), "{ 'KEY_HASH_1': [1] }", false },
    { std::string(), "{ 'KEY_HASH_1': {'a': 'b'} }", false },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    SCOPED_TRACE(testing::Message() << "Iteration " << i);
    INIT_INTERPRETER(
        OP_NAVIGATE(KEY_HASH_1) +
        OP_STORE_NODE_REGISTERABLE_DOMAIN_HASH(VAR_HASH_1) +
        OP_STORE_BOOL(VAR_HASH_2, VALUE_TRUE),
        cases[i].json);
    EXPECT_EQ(JtlInterpreter::OK, interpreter.result());
    if (cases[i].expected_success) {
      base::ExpectDictStringValue(
          cases[i].expected_value, *interpreter.working_memory(), VAR_HASH_1);
      base::ExpectDictBooleanValue(
          true, *interpreter.working_memory(), VAR_HASH_2);
    } else {
      EXPECT_FALSE(interpreter.working_memory()->HasKey(VAR_HASH_1));
      EXPECT_FALSE(interpreter.working_memory()->HasKey(VAR_HASH_2));
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
    OP_STORE_NODE_BOOL(missing_hash),
    OP_STORE_NODE_BOOL(invalid_hash),
    OP_STORE_NODE_HASH(missing_hash),
    OP_STORE_NODE_HASH(invalid_hash),
    OP_COMPARE_NODE_BOOL(missing_bool),
    OP_COMPARE_NODE_BOOL(invalid_bool),
    OP_COMPARE_NODE_HASH(missing_hash),
    OP_COMPARE_NODE_HASH(invalid_hash),
    OP_COMPARE_NODE_TO_STORED_BOOL(missing_hash),
    OP_COMPARE_NODE_TO_STORED_BOOL(invalid_hash),
    OP_COMPARE_NODE_TO_STORED_HASH(missing_hash),
    OP_COMPARE_NODE_TO_STORED_HASH(invalid_hash),
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

TEST(JtlInterpreter, CalculateProgramChecksum) {
  const char kTestSeed[] = "Irrelevant seed value.";
  const char kTestProgram[] = "The quick brown fox jumps over the lazy dog.";
  // This program is invalid, but we are not actually executing it.
  base::DictionaryValue input;
  JtlInterpreter interpreter(kTestSeed, kTestProgram, &input);
  EXPECT_EQ(0xef537f, interpreter.CalculateProgramChecksum());
}

}  // namespace
