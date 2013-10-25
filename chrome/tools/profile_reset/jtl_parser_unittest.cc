// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/tools/profile_reset/jtl_parser.h"

#include "base/json/json_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Helpers -------------------------------------------------------------------

void ExpectNextOperation(JtlParser* parser,
                         const char* expected_name,
                         const char* expected_args_json,
                         bool expected_ends_sentence) {
  std::string actual_name;
  base::ListValue actual_args;
  std::string actual_args_json;
  bool actual_ends_sentence;

  EXPECT_FALSE(parser->HasFinished());
  EXPECT_TRUE(parser->ParseNextOperation(
      &actual_name, &actual_args, &actual_ends_sentence));
  EXPECT_EQ(expected_name, actual_name);
  base::JSONWriter::Write(&actual_args, &actual_args_json);
  EXPECT_EQ(expected_args_json, actual_args_json);
  EXPECT_EQ(expected_ends_sentence, actual_ends_sentence);
}

void ExpectNextOperationToFail(JtlParser* parser,
                               size_t expected_line_number,
                               const char* expected_context_prefix) {
  std::string actual_name;
  base::ListValue actual_args;
  bool actual_ends_sentence;

  EXPECT_FALSE(parser->HasFinished());
  EXPECT_FALSE(parser->ParseNextOperation(
      &actual_name, &actual_args, &actual_ends_sentence));
  EXPECT_THAT(parser->GetLastContext(),
              testing::StartsWith(expected_context_prefix));
  EXPECT_EQ(expected_line_number, parser->GetLastLineNumber());
}

JtlParser* CreateParserFromVerboseText(const std::string& verbose_text) {
  std::string compacted_source_code;
  std::vector<size_t> newline_indices;
  EXPECT_TRUE(JtlParser::RemoveCommentsAndAllWhitespace(
      verbose_text, &compacted_source_code, &newline_indices, NULL));
  return new JtlParser(compacted_source_code, newline_indices);
}

// Tests ---------------------------------------------------------------------

TEST(JtlParser, CompactingEmpty) {
  const char kSourceCode[] = "";
  const char kCompactedSourceCode[] = "";

  scoped_ptr<JtlParser> parser(CreateParserFromVerboseText(kSourceCode));
  EXPECT_EQ(kCompactedSourceCode, parser->compacted_source());
}

TEST(JtlParser, CompactingTrivial) {
  const char kSourceCode[] = "foo";
  const char kCompactedSourceCode[] = "foo";

  scoped_ptr<JtlParser> parser(CreateParserFromVerboseText(kSourceCode));
  EXPECT_EQ(kCompactedSourceCode, parser->compacted_source());
}

TEST(JtlParser, CompactingOneLine) {
  const char kSourceCode[] = "  \r f\to o  ( true )   ";
  const char kCompactedSourceCode[] = "foo(true)";

  scoped_ptr<JtlParser> parser(CreateParserFromVerboseText(kSourceCode));
  EXPECT_EQ(kCompactedSourceCode, parser->compacted_source());
  for (size_t i = 0; i < arraysize(kCompactedSourceCode) - 1; ++i) {
    SCOPED_TRACE(testing::Message("Position ") << i);
    EXPECT_EQ(0u, parser->GetOriginalLineNumber(i));
  }
}

TEST(JtlParser, CompactingMultipleLines) {
  const char kSourceCode[] = "a\nbb\n \nccc  \n\n  d( \n e \n )";
  const char kCompactedSourceCode[] = "abbcccd(e)";
  const size_t kLineNumbers[] = {0u, 1u, 1u, 3u, 3u, 3u, 5u, 5u, 6u, 7u};
  COMPILE_ASSERT(arraysize(kCompactedSourceCode) == arraysize(kLineNumbers) + 1,
                 mismatched_test_data);

  scoped_ptr<JtlParser> parser(CreateParserFromVerboseText(kSourceCode));
  EXPECT_EQ(kCompactedSourceCode, parser->compacted_source());
  for (size_t i = 0; i < arraysize(kLineNumbers); ++i) {
    SCOPED_TRACE(testing::Message("Position ") << i);
    EXPECT_EQ(kLineNumbers[i], parser->GetOriginalLineNumber(i));
  }
}

TEST(JtlParser, CompactingMultipleLinesWithComments) {
  const char kSourceCode[] =
      "a/ /b//Comment \n"
      "//\n"
      "// Full line comment\n"
      " cd //";
  const char kCompactedSourceCode[] = "a//bcd";
  const size_t kLineNumbers[] = {0u, 0u, 0u, 0u, 3u, 3u};
  COMPILE_ASSERT(arraysize(kCompactedSourceCode) == arraysize(kLineNumbers) + 1,
                 mismatched_test_data);

  scoped_ptr<JtlParser> parser(CreateParserFromVerboseText(kSourceCode));
  EXPECT_EQ(kCompactedSourceCode, parser->compacted_source());
  for (size_t i = 0; i < arraysize(kLineNumbers); ++i) {
    SCOPED_TRACE(testing::Message("Position ") << i);
    EXPECT_EQ(kLineNumbers[i], parser->GetOriginalLineNumber(i));
  }
}

TEST(JtlParser, HandlingCommentsAndStringLiterals) {
  struct TestCase {
    const char* source_code;
    const char* compacted_source_code;
  } cases[] = {
      {"//", ""},
      {"//comment", ""},
      {"foo // comment", "foo"},
      {"foo // // comment", "foo"},
      {"foo //", "foo"},
      {"\"literal\"", "\"literal\""},
      {"\"literal with space\"", "\"literal with space\""},
      {"\"\"", "\"\""},
      {"\"\"\"\"", "\"\"\"\""},
      {"\"\" // comment", "\"\""},
      {"\"literal\" // comment", "\"literal\""},
      {"\"literal\" \"literal\" // comment", "\"literal\"\"literal\""},
      {"foo // \"not a literal\"", "foo"},
      {"foo // \"not even matched", "foo"},
      {"foo // \"not a literal\" \"not even matched", "foo"},
      {"\"literal\" // \"not a literal\"", "\"literal\""},
      {"\"literal\" // \"not even matched", "\"literal\""},
      {"\"//not a comment//\"", "\"//not a comment//\""},
      {"\"//not a comment//\" // comment", "\"//not a comment//\""},
      {"// \"//not a literal//\" // comment", ""},
      {"\"literal\" // \"//not a literal//\" // comment", "\"literal\""},
      {"\"//not a comment//\" // \"//not a literal//\" // comment",
       "\"//not a comment//\""},
      {"\"literal // \"not a literal nor a comment",
       "\"literal // \"notaliteralnoracomment"},
      {"\"literal // \"not a literal nor a comment//\"",
       "\"literal // \"notaliteralnoracomment"}
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    SCOPED_TRACE(cases[i].source_code);
    scoped_ptr<JtlParser> parser(
        CreateParserFromVerboseText(cases[i].source_code));
    EXPECT_EQ(cases[i].compacted_source_code, parser->compacted_source());
  }
}

TEST(JtlParser, MismatchedDoubleQuotesBeforeEndOfLine) {
  struct TestCase {
    const char* source_code;
    size_t error_line_number;
  } cases[] = {
      {"\"", 0},
      {"\"mismatched literal", 0},
      {"\n\"", 1},
      {"\"\n\"", 0},
      {"\"\"\"", 0},
      {"\"\"\n\"", 1},
      {"\"\"\n\"\n\"", 1},
      {"\" // not a comment", 0},
      {"\" // not a comment\n\"", 0},
      {"\"\" // comment\n\"", 1},
      {"\"\"\" // not a comment\n\"", 0},
      {"\"\"\" // \" neither a literal nor a comment\"\n\"", 0},
      {"\"\" // comment\n\"// not a comment", 1},
      {"\" // not a comment\"\n\"// not a comment", 1},
      {"foo(\"bar\");\nfoo(\"mismatched);", 1},
      {"foo(\n\"bar\", \"mismatched);", 1},
      {"foo(\n\"bar\", \"mismatched); //comment", 1},
      {"foo(\n\"bar\", \"mismatched);\ngood(\"bar\")", 1}
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    SCOPED_TRACE(cases[i].source_code);
    std::string compacted_source_code;
    std::vector<size_t> newline_indices;
    size_t error_line_number;
    EXPECT_FALSE(JtlParser::RemoveCommentsAndAllWhitespace(
        cases[i].source_code,
        &compacted_source_code,
        &newline_indices,
        &error_line_number));
    EXPECT_EQ(cases[i].error_line_number, error_line_number);
  }
}

TEST(JtlParser, ParsingEmpty) {
  const char kSourceCode[] = "";

  scoped_ptr<JtlParser> parser(CreateParserFromVerboseText(kSourceCode));
  EXPECT_TRUE(parser->HasFinished());
}

TEST(JtlParser, ParsingOneWellFormedOperation) {
  struct TestCase {
    const char* source_code;
    const char* expected_name;
    const char* expected_args;
    const bool expected_ends_sentence;
  } cases[] = {
      {"foo1;", "foo1", "[]", true},
      {"foo2().", "foo2", "[]", false},
      {"foo3(true);", "foo3", "[true]", true},
      {"foo4(false).", "foo4", "[false]", false},
      {"foo5(\"bar\").", "foo5", "[\"bar\"]", false},
      {"foo6(\" b a r \").", "foo6", "[\" b a r \"]", false},
      {"foo7(true, \"bar\").", "foo7", "[true,\"bar\"]", false},
      {"foo8(\"bar\", false, true);", "foo8", "[\"bar\",false,true]", true},
      {"foo9(\"bar\", \" b a r \");", "foo9", "[\"bar\",\" b a r \"]", true}
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    SCOPED_TRACE(cases[i].expected_name);
    scoped_ptr<JtlParser> parser(
        CreateParserFromVerboseText(cases[i].source_code));
    ExpectNextOperation(parser.get(),
                        cases[i].expected_name,
                        cases[i].expected_args,
                        cases[i].expected_ends_sentence);
    EXPECT_TRUE(parser->HasFinished());
  }
}

TEST(JtlParser, ParsingMultipleWellFormedOperations) {
  const char kSourceCode[] =
      "foo1(true).foo2.foo3(\"bar\");"
      "foo4(\"bar\", false);";

  scoped_ptr<JtlParser> parser(CreateParserFromVerboseText(kSourceCode));
  ExpectNextOperation(parser.get(), "foo1", "[true]", false);
  ExpectNextOperation(parser.get(), "foo2", "[]", false);
  ExpectNextOperation(parser.get(), "foo3", "[\"bar\"]", true);
  ExpectNextOperation(parser.get(), "foo4", "[\"bar\",false]", true);
  EXPECT_TRUE(parser->HasFinished());
}

TEST(JtlParser, ParsingTrickyStringLiterals) {
  struct TestCase {
    const char* source_code;
    const char* expected_name;
    const char* expected_args;
    const bool expected_ends_sentence;
  } cases[] = {
      {"prev().foo1(\"\");next(true);", "foo1", "[\"\"]", true},
      {"prev().foo2(\" \");next(true);", "foo2", "[\" \"]", true},
      {"prev().foo3(\",\",true);next(true);", "foo3", "[\",\",true]", true},
      {"prev().foo4(\")\",true);next(true);", "foo4", "[\")\",true]", true},
      {"prev().foo5(\";\",true);next(true);", "foo5", "[\";\",true]", true},
      {"prev().foo6(\"/\",true).next(true);", "foo6", "[\"/\",true]", false},
      {"prev().foo7(\"//\",true).next(true);", "foo7", "[\"//\",true]", false},
      {"prev().foo8(\".\",true).next(true);", "foo8", "[\".\",true]", false},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    SCOPED_TRACE(cases[i].expected_name);
    scoped_ptr<JtlParser> parser(
        CreateParserFromVerboseText(cases[i].source_code));
    ExpectNextOperation(parser.get(), "prev", "[]", false);
    ExpectNextOperation(parser.get(),
                        cases[i].expected_name,
                        cases[i].expected_args,
                        cases[i].expected_ends_sentence);
    ExpectNextOperation(parser.get(), "next", "[true]", true);
    EXPECT_TRUE(parser->HasFinished());
  }
}

TEST(JtlParser, FirstOperationIsIllFormed) {
  struct TestCase {
    const char* source_code;
    const char* operation_name;
  } cases[] = {
      {";;", ";"},
      {"bad_args1(not a boolean value);", "bad_args1"},
      {"bad_args2(,);", "bad_args2"},
      {"bad_args3(...);", "bad_args3"},
      {"bad_args4(1);", "bad_args4"},
      {"bad_args5(1.2);", "bad_args5"},
      {"bad_args6([\"bar\"]);", "bad_args6"},
      {"bad_args7(False);", "bad_args7"},
      {"bad_args8(True);", "bad_args8"},
      {"bad_quotes1(missing both, true).good();", "bad_quotes1"},
      {"bad_quotes2(true, \"missing one).good(); //\"", "bad_quotes2"},
      {"bad_quotes3(\"too\" \"much\", true).good();", "bad_quotes3"},
      {"bad_missing_separator1", "bad_missing_separator1"},
      {"bad_missing_separator2()good();", "bad_missing_separator2"},
      {"bad_parenthesis1(true.good();", "bad_parenthesis1"},
      {"bad_parenthesis2(true.good());", "bad_parenthesis2"},
      {"bad_parenthesis3).good();", "bad_parenthesis3"}
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    SCOPED_TRACE(cases[i].operation_name);
    scoped_ptr<JtlParser> parser(
        CreateParserFromVerboseText(cases[i].source_code));
    ExpectNextOperationToFail(parser.get(), 0, cases[i].operation_name);
  }
}

TEST(JtlParser, SecondOperationIsIllFormed) {
  struct TestCase {
    const char* source_code;
    const char* bad_operation_name;
  } cases[] = {
      {"\ngood(true,false)\n.bad_args(,);", "bad_args"},
      {"\ngood(true,false)\n.bad_quotes1(missing both, true).good();",
       "bad_quotes1"},
      {"\ngood(true,false)\n.bad_quotes2(\"missing one, true).good(); //\"",
       "bad_quotes2"},
      {"\ngood(true,false)\n.bad_quotes3(\"too\" \"many\", true).good();",
       "bad_quotes3"},
      {"\ngood(true,false)\n.bad_separator1()/good();", "bad_separator1"},
      {"\ngood(true,false)\n.missing_separator1", "missing_separator1"},
      {"\ngood(true,false)\n.missing_separator2()good();",
       "missing_separator2"},
      {"\ngood(true,false)\n.bad_parens1(true.good();", "bad_parens1"},
      {"\ngood(true,false)\n.bad_parens2(true.good());", "bad_parens2"},
      {"\ngood(true,false)\n.bad_parens3).good();", "bad_parens3"}
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    SCOPED_TRACE(cases[i].bad_operation_name);
    scoped_ptr<JtlParser> parser(
        CreateParserFromVerboseText(cases[i].source_code));
    ExpectNextOperation(parser.get(), "good", "[true,false]", false);
    ExpectNextOperationToFail(parser.get(), 2, cases[i].bad_operation_name);
  }
}

}  // namespace
