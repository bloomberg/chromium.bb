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

// Tests ---------------------------------------------------------------------

// Note: Parsing and parsing-related errors are unit-tested separately in more
// detail in "jtl_parser_unittest.cc". Here, most of  the time, we assume that
// creating the parse tree works.

TEST(JtlCompiler, CompileSingleInstructions) {
  struct TestCase {
    std::string source_code;
    std::string expected_bytecode;
  } cases[] = {
        {"node(\"foo\")/", OP_NAVIGATE(GetHash("foo"))},
        {"node(\"has whitespace\t\")/",
         OP_NAVIGATE(GetHash("has whitespace\t"))},
        {"any/", OP_NAVIGATE_ANY},
        {"back/", OP_NAVIGATE_BACK},
        {"store_bool(\"name\", true)/",
         OP_STORE_BOOL(GetHash("name"), VALUE_TRUE)},
        {"compare_stored_bool(\"name\", true, false)/",
         OP_COMPARE_STORED_BOOL(GetHash("name"), VALUE_TRUE, VALUE_FALSE)},
        {"store_hash(\"name\", \"value\")/",
         OP_STORE_HASH(GetHash("name"), GetHash("value"))},
        {"compare_stored_hash(\"name\", \"value\", \"default\")/",
         OP_COMPARE_STORED_HASH(
             GetHash("name"), GetHash("value"), GetHash("default"))},
        {"compare_bool(false)/", OP_COMPARE_NODE_BOOL(VALUE_FALSE)},
        {"compare_bool(true)/", OP_COMPARE_NODE_BOOL(VALUE_TRUE)},
        {"compare_hash(\"foo\")/", OP_COMPARE_NODE_HASH(GetHash("foo"))},
        {"compare_hash(\"bar\")/", OP_COMPARE_NODE_HASH(GetHash("bar"))},
        {"break/", OP_STOP_EXECUTING_SENTENCE},
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
      "node(\"foo\")/node(\"bar\")/store_bool(\"x\", true);\n"
      "// ...\n"
      "// Store \"y\"=\"1\" if above value is set.\n"
      "compare_stored_bool(\"x\", true, false)/store_hash(\"y\", \"1\");\n";

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
  const char kSourceCode[] = "any()\n/\nnon_existent_instruction\n(\n)\n;\n";

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
      "any()/\nstore_bool(\"name\", true, \"superfluous argument\");\n",
      "any()/\nstore_bool(\"name\");"};  // missing argument

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
  const char* kSourceCodes[] = {
      "any()\n/\ncompare_stored_bool(true, false, false);",  // Arg#1 should be
                                                             // a hash.
      "any()\n/\ncompare_stored_bool(\"name\", \"should be a bool\", false);",
      "any()\n/\ncompare_stored_bool(\"name\", false, \"should be a bool\");"};

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kSourceCodes); ++i) {
    SCOPED_TRACE(kSourceCodes[i]);
    std::string bytecode;
    JtlCompiler::CompileError error;
    EXPECT_FALSE(JtlCompiler::Compile(
        kSourceCodes[i], kTestHashSeed, &bytecode, &error));
    EXPECT_THAT(error.context, testing::StartsWith("compare_stored_bool"));
    EXPECT_EQ(2u, error.line_number);
    EXPECT_EQ(JtlCompiler::CompileError::INVALID_ARGUMENT_TYPE,
              error.error_code);
  }
}

TEST(JtlCompiler, MistmatchedDoubleQuotes) {
  const char kSourceCode[] = "any()/\nnode(\"ok\", \"stray quote)/break();";

  std::string bytecode;
  JtlCompiler::CompileError error;
  EXPECT_FALSE(
      JtlCompiler::Compile(kSourceCode, kTestHashSeed, &bytecode, &error));
  EXPECT_EQ(1u, error.line_number);
  EXPECT_EQ(JtlCompiler::CompileError::MISMATCHED_DOUBLE_QUOTES,
            error.error_code);
}

TEST(JtlCompiler, ParsingError) {
  const char kSourceCode[] = "any()/\nnode()missing_separator();";

  std::string bytecode;
  JtlCompiler::CompileError error;
  EXPECT_FALSE(
      JtlCompiler::Compile(kSourceCode, kTestHashSeed, &bytecode, &error));
  EXPECT_THAT(error.context, testing::StartsWith("node"));
  EXPECT_EQ(1u, error.line_number);
  EXPECT_EQ(JtlCompiler::CompileError::PARSING_ERROR, error.error_code);
}

}  // namespace
