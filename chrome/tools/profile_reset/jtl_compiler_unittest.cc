// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/tools/profile_reset/jtl_compiler.h"

#include <string>

#include "base/values.h"
#include "chrome/browser/profile_resetter/jtl_foundation.h"
#include "chrome/browser/profile_resetter/jtl_instructions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTestHashSeed[] = "test-hash-seed";

// Helpers -------------------------------------------------------------------

std::string GetHash(const std::string& input) {
  return jtl_foundation::Hasher(kTestHashSeed).GetHash(input);
}

static std::string EncodeUint32(uint32 value) {
  std::string bytecode;
  for (int i = 0; i < 4; ++i) {
    bytecode.push_back(static_cast<char>(value & 0xFFu));
    value >>= 8;
  }
  return bytecode;
}

// Tests ---------------------------------------------------------------------

// Note: Parsing and parsing-related errors are unit-tested separately in more
// detail in "jtl_parser_unittest.cc". Here, most of  the time, we assume that
// creating the parse tree works.

TEST(JtlCompiler, CompileSingleInstructions) {
  struct TestCase {
    std::string source_code;
    std::string expected_bytecode;
  } cases[] = {
        {"go(\"foo\").", OP_NAVIGATE(GetHash("foo"))},
        {"go(\"has whitespace\t\").", OP_NAVIGATE(GetHash("has whitespace\t"))},
        {"any.", OP_NAVIGATE_ANY},
        {"back.", OP_NAVIGATE_BACK},
        {"store_bool(\"name\", true).",
         OP_STORE_BOOL(GetHash("name"), VALUE_TRUE)},
        {"compare_stored_bool(\"name\", true, false).",
         OP_COMPARE_STORED_BOOL(GetHash("name"), VALUE_TRUE, VALUE_FALSE)},
        {"store_hash(\"name\", \"" + GetHash("value") + "\").",
         OP_STORE_HASH(GetHash("name"), GetHash("value"))},
        {"store_hashed(\"name\", \"value\").",
         OP_STORE_HASH(GetHash("name"), GetHash("value"))},
        {"store_node_bool(\"name\").", OP_STORE_NODE_BOOL(GetHash("name"))},
        {"store_node_hash(\"name\").", OP_STORE_NODE_HASH(GetHash("name"))},
        {"store_node_registerable_domain_hash(\"name\").",
         OP_STORE_NODE_REGISTERABLE_DOMAIN_HASH(GetHash("name"))},
        {"compare_stored_hashed(\"name\", \"value\", \"default\").",
         OP_COMPARE_STORED_HASH(
             GetHash("name"), GetHash("value"), GetHash("default"))},
        {"compare_bool(false).", OP_COMPARE_NODE_BOOL(VALUE_FALSE)},
        {"compare_bool(true).", OP_COMPARE_NODE_BOOL(VALUE_TRUE)},
        {"compare_hashed(\"foo\").", OP_COMPARE_NODE_HASH(GetHash("foo"))},
        {"compare_hashed_not(\"foo\").",
         OP_COMPARE_NODE_HASH_NOT(GetHash("foo"))},
        {"compare_to_stored_bool(\"name\").",
         OP_COMPARE_NODE_TO_STORED_BOOL(GetHash("name"))},
        {"compare_to_stored_hash(\"name\").",
         OP_COMPARE_NODE_TO_STORED_HASH(GetHash("name"))},
        {"compare_substring_hashed(\"pattern\").",
         OP_COMPARE_NODE_SUBSTRING(
             GetHash("pattern"), EncodeUint32(7), EncodeUint32(766))},
        {"break.", OP_STOP_EXECUTING_SENTENCE},
        {"break;", OP_STOP_EXECUTING_SENTENCE + OP_END_OF_SENTENCE}};

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    SCOPED_TRACE(cases[i].source_code);
    std::string bytecode;
    EXPECT_TRUE(JtlCompiler::Compile(
        cases[i].source_code, kTestHashSeed, &bytecode, NULL));
    EXPECT_EQ(cases[i].expected_bytecode, bytecode);
  }
}

TEST(JtlCompiler, CompileEntireProgram) {
  const char kSourceCode[] =
      "// Store \"x\"=true if path is found.\n"
      "go(\"foo\").go(\"bar\").store_bool(\"x\", true);\n"
      "// ...\n"
      "// Store \"y\"=\"1\" if above value is set.\n"
      "compare_stored_bool(\"x\", true, false).store_hashed(\"y\", \"1\");\n";

  std::string expected_bytecode =
      OP_NAVIGATE(GetHash("foo")) +
      OP_NAVIGATE(GetHash("bar")) +
      OP_STORE_BOOL(GetHash("x"), VALUE_TRUE) + OP_END_OF_SENTENCE +
      OP_COMPARE_STORED_BOOL(GetHash("x"), VALUE_TRUE, VALUE_FALSE) +
      OP_STORE_HASH(GetHash("y"), GetHash("1")) + OP_END_OF_SENTENCE;

  std::string bytecode;
  EXPECT_TRUE(
      JtlCompiler::Compile(kSourceCode, kTestHashSeed, &bytecode, NULL));
  EXPECT_EQ(expected_bytecode, bytecode);
}

TEST(JtlCompiler, InvalidOperationName) {
  const char kSourceCode[] = "any()\n.\nnon_existent_instruction\n(\n)\n;\n";

  std::string bytecode;
  JtlCompiler::CompileError error;
  EXPECT_FALSE(
      JtlCompiler::Compile(kSourceCode, kTestHashSeed, &bytecode, &error));
  EXPECT_THAT(error.context, testing::StartsWith("non_existent_instruction"));
  EXPECT_EQ(2u, error.line_number);
  EXPECT_EQ(JtlCompiler::CompileError::INVALID_OPERATION_NAME,
            error.error_code);
}

TEST(JtlCompiler, InvalidArgumentsCount) {
  const char* kSourceCodes[] = {
      "any().\nstore_bool(\"name\", true, \"superfluous argument\");\n",
      "any().\nstore_bool(\"name\");"};  // missing argument

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kSourceCodes); ++i) {
    SCOPED_TRACE(kSourceCodes[i]);
    std::string bytecode;
    JtlCompiler::CompileError error;
    EXPECT_FALSE(JtlCompiler::Compile(
        kSourceCodes[i], kTestHashSeed, &bytecode, &error));
    EXPECT_THAT(error.context, testing::StartsWith("store_bool"));
    EXPECT_EQ(1u, error.line_number);
    EXPECT_EQ(JtlCompiler::CompileError::INVALID_ARGUMENT_COUNT,
              error.error_code);
  }
}

TEST(JtlCompiler, InvalidArgumentType) {
  struct TestCase {
    std::string expected_context_prefix;
    std::string source_code;
  } cases[] = {
        {"compare_bool", "any()\n.\ncompare_bool(\"foo\");"},
        {"compare_bool",
         "any()\n.\ncompare_bool(\"01234567890123456789012345678901\");"},
        {"compare_hashed", "any()\n.\ncompare_hashed(false);"},
        {"store_hash", "any()\n.\nstore_hash(\"name\", false);"},
        {"store_hash", "any()\n.\nstore_hash(\"name\", \"foo\");"},
        {"compare_stored_bool",
         "any()\n.\ncompare_stored_bool(\"name\", \"need a bool\", false);"},
        {"compare_stored_bool",
         "any()\n.\ncompare_stored_bool(\"name\", false, \"need a bool\");"},
        {"compare_substring_hashed",
         "any()\n.\ncompare_substring_hashed(true);"}};

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    SCOPED_TRACE(cases[i].source_code);
    std::string bytecode;
    JtlCompiler::CompileError error;
    EXPECT_FALSE(JtlCompiler::Compile(
        cases[i].source_code, kTestHashSeed, &bytecode, &error));
    EXPECT_THAT(error.context,
                testing::StartsWith(cases[i].expected_context_prefix));
    EXPECT_EQ(2u, error.line_number);
    EXPECT_EQ(JtlCompiler::CompileError::INVALID_ARGUMENT_TYPE,
              error.error_code);
  }
}

TEST(JtlCompiler, InvalidArgumentValue) {
  struct TestCase {
    std::string expected_context_prefix;
    std::string source_code;
  } cases[] = {
        {"compare_substring_hashed", "compare_substring_hashed(\"\");"}};

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    SCOPED_TRACE(cases[i].source_code);
    std::string bytecode;
    JtlCompiler::CompileError error;
    EXPECT_FALSE(JtlCompiler::Compile(
        cases[i].source_code, kTestHashSeed, &bytecode, &error));
    EXPECT_THAT(error.context,
                testing::StartsWith(cases[i].expected_context_prefix));
    EXPECT_EQ(0u, error.line_number);
    EXPECT_EQ(JtlCompiler::CompileError::INVALID_ARGUMENT_VALUE,
              error.error_code);
  }
}

TEST(JtlCompiler, MistmatchedDoubleQuotes) {
  const char kSourceCode[] = "any().\ngo(\"ok\", \"stray quote).break();";

  std::string bytecode;
  JtlCompiler::CompileError error;
  EXPECT_FALSE(
      JtlCompiler::Compile(kSourceCode, kTestHashSeed, &bytecode, &error));
  EXPECT_EQ(1u, error.line_number);
  EXPECT_EQ(JtlCompiler::CompileError::MISMATCHED_DOUBLE_QUOTES,
            error.error_code);
}

TEST(JtlCompiler, ParsingError) {
  const char kSourceCode[] = "any().\ngo()missing_separator();";

  std::string bytecode;
  JtlCompiler::CompileError error;
  EXPECT_FALSE(
      JtlCompiler::Compile(kSourceCode, kTestHashSeed, &bytecode, &error));
  EXPECT_THAT(error.context, testing::StartsWith("go"));
  EXPECT_EQ(1u, error.line_number);
  EXPECT_EQ(JtlCompiler::CompileError::PARSING_ERROR, error.error_code);
}

}  // namespace
