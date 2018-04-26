// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/tools/ruleset_converter/rule_stream.h"

#include <algorithm>
#include <fstream>
#include <ios>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/subresource_filter/tools/rule_parser/rule_parser.h"
#include "components/url_pattern_index/proto/rules.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace subresource_filter {

namespace {

// Stores lists of the rules that a test ruleset consists of.
struct TestRulesetContents {
  std::vector<url_pattern_index::proto::UrlRule> url_rules;
  std::vector<url_pattern_index::proto::CssRule> css_rules;
};

// Stores identification information about a temporary File with a ruleset.
// Deletes the file automatically on destruction.
class ScopedTempRulesetFile {
 public:
  // Creates a temporary file of the specified |format|.
  explicit ScopedTempRulesetFile(RulesetFormat format) : format_(format) {
    // Cannot ASSERT due to returning in constructor.
    CHECK(scoped_dir_.CreateUniqueTempDir());
    CHECK(
        base::CreateTemporaryFileInDir(scoped_dir_.GetPath(), &ruleset_path_));
  }

  ~ScopedTempRulesetFile() {}

  const base::FilePath& ruleset_path() const { return ruleset_path_; }
  RulesetFormat format() const { return format_; }

 private:
  base::ScopedTempDir scoped_dir_;
  base::FilePath ruleset_path_;
  const RulesetFormat format_;  // The format of the |file|.
};

bool IsEqualRules(const url_pattern_index::proto::UrlRule& first,
                  const url_pattern_index::proto::UrlRule& second) {
  return first.SerializeAsString() == second.SerializeAsString();
}

bool IsEqualRules(const url_pattern_index::proto::CssRule& first,
                  const url_pattern_index::proto::CssRule& second) {
  return first.SerializeAsString() == second.SerializeAsString();
}

void AssertEqualContents(const TestRulesetContents& lhs,
                         const TestRulesetContents& rhs) {
  ASSERT_EQ(lhs.url_rules.size(), rhs.url_rules.size());
  for (size_t i = 0, size = lhs.url_rules.size(); i != size; ++i) {
    ASSERT_TRUE(IsEqualRules(lhs.url_rules[i], rhs.url_rules[i]))
        << i << ' ' << ToString(lhs.url_rules[i]) << ' '
        << ToString(rhs.url_rules[i]);
  }

  ASSERT_EQ(lhs.css_rules.size(), rhs.css_rules.size());
  for (size_t i = 0, size = lhs.css_rules.size(); i != size; ++i) {
    ASSERT_TRUE(IsEqualRules(lhs.css_rules[i], rhs.css_rules[i]))
        << i << ' ' << ToString(lhs.css_rules[i]) << ' '
        << ToString(rhs.css_rules[i]);
  }
}

// Opens the |ruleset_file| and creates an empty rule output stream to this
// file. Returns the stream or nullptr if it failed to be created.
std::unique_ptr<RuleOutputStream> OpenNewRuleset(
    const ScopedTempRulesetFile& ruleset_file) {
  return RuleOutputStream::Create(
      std::make_unique<std::ofstream>(
          ruleset_file.ruleset_path().MaybeAsASCII(),
          std::ios::binary | std::ios::out),
      ruleset_file.format());
}

// Opens the |ruleset_file| with already existing ruleset and returns the
// corresponding input stream, or nullptr if it failed to be created.
std::unique_ptr<RuleInputStream> OpenRuleset(
    const ScopedTempRulesetFile& ruleset_file) {
  return RuleInputStream::Create(std::make_unique<std::ifstream>(
                                     ruleset_file.ruleset_path().MaybeAsASCII(),
                                     std::ios::binary | std::ios::in),
                                 ruleset_file.format());
}

// Returns a small number of predefined rules in text format.
std::vector<std::string> GetSomeRules() {
  return std::vector<std::string>{
      "example.com",
      "||ex.com$image",
      "|http://example.com/?key=value$~third-party,domain=ex.com",
      "&key1=value1&key2=value2|$script,image,font",
      "domain1.com,domain1.com###id",
      "@@whitelisted.com$document,domain=example.com|~sub.example.com",
      "###absolute_evil_id",
      "@@whitelisted.com$match-case,document,domain=another.example.com",
      "domain.com,~sub.domain.com,sub.sub.domain.com#@#id",
      "#@#absolute_good_id",
      "host$websocket",
  };
}

// Returns some rules which Chrome doesn't support fully or partially, mixed
// with a couple of supported rules, or otherwise weird ones.
std::vector<std::string> GetSomeChromeUnfriendlyRules() {
  return std::vector<std::string>{
      "/a[0-9].com/$image",
      "a.com$image,popup"
      "a.com$popup",
      "a.com$~image",
      "a.com$~popup",
      "a.com$~image,~popup",
      "@@a.com$subdocument,document",
      "@@a.com$document,generichide",
      "@@a.com$document",
      "@@a.com$genericblock",
      "@@a.com$elemhide",
      "@@a.com$generichide",
      "@@a.com$elemhide,generichide",
      "@@a.com$image,elemhide,generichide",
      "a.com$image,~image",
  };
}

// Generates and returns many rules in text format.
std::vector<std::string> GetManyRules() {
  constexpr size_t kNumberOfUrlRules = 10123;
  constexpr size_t kNumberOfCssRules = 5321;

  std::vector<std::string> text_rules;

  for (size_t i = 0; i != kNumberOfUrlRules; ++i) {
    std::string text_rule;
    if (!(i & 3))
      text_rule += "@@";
    if (i & 1)
      text_rule += "sub.";
    text_rule += "example" + base::IntToString(i) + ".com";
    text_rule += '$';
    text_rule += (i & 7) ? "script" : "image";
    if (i & 1)
      text_rule += ",domain=example.com|~but_not.example.com";
    text_rules.push_back(text_rule);
  }

  for (size_t i = 0; i != kNumberOfCssRules; ++i) {
    std::string text_rule = "domain.com";
    if (i & 1)
      text_rule += ",~but_not.domain.com";
    text_rule += (i & 3) ? "##" : "#@#";
    text_rule += "#id" + base::IntToString(i);
    text_rules.push_back(text_rule);
  }

  return text_rules;
}

// Parses |text_rules| and appends them to the |ruleset|.
void PrepareRuleset(TestRulesetContents* ruleset,
                    const std::vector<std::string>& text_rules,
                    bool allow_errors = false) {
  RuleParser parser;
  for (const std::string& text_rule : text_rules) {
    url_pattern_index::proto::RuleType rule_type = parser.Parse(text_rule);
    switch (rule_type) {
      case url_pattern_index::proto::RULE_TYPE_URL:
        ruleset->url_rules.push_back(parser.url_rule().ToProtobuf());
        break;
      case url_pattern_index::proto::RULE_TYPE_CSS:
        ruleset->css_rules.push_back(parser.css_rule().ToProtobuf());
        break;
      case url_pattern_index::proto::RULE_TYPE_UNSPECIFIED:
        ASSERT_TRUE(allow_errors);
        break;
      default:
        ASSERT_TRUE(false);
    }
  }
}

// Opens the |ruleset_file|, and writes the test ruleset |contents| to it in the
// corresponding format.
void WriteTestRuleset(const ScopedTempRulesetFile& ruleset_file,
                      const TestRulesetContents& contents) {
  std::unique_ptr<RuleOutputStream> output = OpenNewRuleset(ruleset_file);
  ASSERT_NE(nullptr, output);

  for (const auto& rule : contents.url_rules)
    EXPECT_TRUE(output->PutUrlRule(rule));
  for (const auto& rule : contents.css_rules)
    EXPECT_TRUE(output->PutCssRule(rule));
  EXPECT_TRUE(output->Finish());
}

// Reads the provided |ruleset_file| back, and EXPECTs that it contains exactly
// all the rules from |expected_contents| in the same order.
void ReadTestRulesetAndExpectContents(
    const ScopedTempRulesetFile& ruleset_file,
    const TestRulesetContents& expected_contents) {
  std::unique_ptr<RuleInputStream> input = OpenRuleset(ruleset_file);

  TestRulesetContents contents;
  url_pattern_index::proto::RuleType rule_type =
      url_pattern_index::proto::RULE_TYPE_UNSPECIFIED;
  while ((rule_type = input->FetchNextRule()) !=
         url_pattern_index::proto::RULE_TYPE_UNSPECIFIED) {
    if (rule_type == url_pattern_index::proto::RULE_TYPE_URL) {
      contents.url_rules.push_back(input->GetUrlRule());
    } else {
      ASSERT_EQ(url_pattern_index::proto::RULE_TYPE_CSS, rule_type);
      contents.css_rules.push_back(input->GetCssRule());
    }
  }

  AssertEqualContents(contents, expected_contents);
}

// Reads the provided |ruleset_file| skipping every second rule (independently
// for URL and CSS rules), and EXPECTs that it contains exactly all the rules
// from |expected_contents| in the same order.
void ReadHalfRulesOfTestRulesetAndExpectContents(
    const ScopedTempRulesetFile& ruleset_file,
    const TestRulesetContents& expected_contents) {
  std::unique_ptr<RuleInputStream> input = OpenRuleset(ruleset_file);

  TestRulesetContents contents;

  bool take_url_rule = true;
  bool take_css_rule = true;
  url_pattern_index::proto::RuleType rule_type =
      url_pattern_index::proto::RULE_TYPE_UNSPECIFIED;
  while ((rule_type = input->FetchNextRule()) !=
         url_pattern_index::proto::RULE_TYPE_UNSPECIFIED) {
    if (rule_type == url_pattern_index::proto::RULE_TYPE_URL) {
      if (take_url_rule)
        contents.url_rules.push_back(input->GetUrlRule());
      take_url_rule = !take_url_rule;
    } else {
      ASSERT_EQ(url_pattern_index::proto::RULE_TYPE_CSS, rule_type);
      if (take_css_rule)
        contents.css_rules.push_back(input->GetCssRule());
      take_css_rule = !take_css_rule;
    }
  }

  AssertEqualContents(contents, expected_contents);
}

}  // namespace

TEST(RuleStreamTest, WriteAndReadRuleset) {
  for (int small_or_big = 0; small_or_big < 2; ++small_or_big) {
    TestRulesetContents contents;
    if (small_or_big)
      PrepareRuleset(&contents, GetManyRules());
    else
      PrepareRuleset(&contents, GetSomeRules());

    TestRulesetContents only_url_rules;
    only_url_rules.url_rules = contents.url_rules;

    for (auto format : {RulesetFormat::kFilterList, RulesetFormat::kProto,
                        RulesetFormat::kUnindexedRuleset}) {
      ScopedTempRulesetFile ruleset_file(format);
      WriteTestRuleset(ruleset_file, contents);
      // Note: kUnindexedRuleset discards CSS rules, test it differently.
      ReadTestRulesetAndExpectContents(
          ruleset_file, format == RulesetFormat::kUnindexedRuleset
                            ? only_url_rules
                            : contents);
    }
  }
}

TEST(RuleStreamTest, WriteAndReadHalfRuleset) {
  TestRulesetContents contents;
  PrepareRuleset(&contents, GetManyRules());

  TestRulesetContents half_contents;
  for (size_t i = 0, size = contents.url_rules.size(); i < size; i += 2)
    half_contents.url_rules.push_back(contents.url_rules[i]);
  for (size_t i = 0, size = contents.css_rules.size(); i < size; i += 2)
    half_contents.css_rules.push_back(contents.css_rules[i]);

  TestRulesetContents half_url_rules;
  half_url_rules.url_rules = half_contents.url_rules;

  for (auto format : {RulesetFormat::kFilterList, RulesetFormat::kProto,
                      RulesetFormat::kUnindexedRuleset}) {
    ScopedTempRulesetFile ruleset_file(format);
    WriteTestRuleset(ruleset_file, contents);
    // Note: kUnindexedRuleset discards CSS rules, test it differently.
    ReadHalfRulesOfTestRulesetAndExpectContents(
        ruleset_file, format == RulesetFormat::kUnindexedRuleset
                          ? half_url_rules
                          : half_contents);
  }
}

TEST(RuleStreamTest, TransferAllRulesToSameStream) {
  TestRulesetContents contents;
  PrepareRuleset(&contents, GetManyRules());

  ScopedTempRulesetFile source_ruleset(RulesetFormat::kFilterList);
  ScopedTempRulesetFile target_ruleset(RulesetFormat::kFilterList);
  WriteTestRuleset(source_ruleset, contents);

  std::unique_ptr<RuleInputStream> input = OpenRuleset(source_ruleset);
  std::unique_ptr<RuleOutputStream> output = OpenNewRuleset(target_ruleset);
  TransferRules(input.get(), output.get(), output.get());
  EXPECT_TRUE(output->Finish());
  input.reset();
  output.reset();

  ReadTestRulesetAndExpectContents(target_ruleset, contents);
}

TEST(RuleStreamTest, TransferUrlRulesToOneStream) {
  TestRulesetContents contents;
  PrepareRuleset(&contents, GetManyRules());

  ScopedTempRulesetFile source_ruleset(RulesetFormat::kFilterList);
  ScopedTempRulesetFile target_ruleset(RulesetFormat::kFilterList);
  WriteTestRuleset(source_ruleset, contents);

  std::unique_ptr<RuleInputStream> input = OpenRuleset(source_ruleset);
  std::unique_ptr<RuleOutputStream> output = OpenNewRuleset(target_ruleset);
  TransferRules(input.get(), output.get(), nullptr);
  EXPECT_TRUE(output->Finish());
  input.reset();
  output.reset();

  contents.css_rules.clear();
  ReadTestRulesetAndExpectContents(target_ruleset, contents);
}

TEST(RuleStreamTest, TransferCssRulesToOneStream) {
  TestRulesetContents contents;
  PrepareRuleset(&contents, GetManyRules());

  ScopedTempRulesetFile source_ruleset(RulesetFormat::kFilterList);
  ScopedTempRulesetFile target_ruleset(RulesetFormat::kFilterList);
  WriteTestRuleset(source_ruleset, contents);

  std::unique_ptr<RuleInputStream> input = OpenRuleset(source_ruleset);
  std::unique_ptr<RuleOutputStream> output = OpenNewRuleset(target_ruleset);
  TransferRules(input.get(), nullptr, output.get());
  EXPECT_TRUE(output->Finish());
  input.reset();
  output.reset();

  contents.url_rules.clear();
  ReadTestRulesetAndExpectContents(target_ruleset, contents);
}

TEST(RuleStreamTest, TransferAllRulesToDifferentStreams) {
  TestRulesetContents contents;
  PrepareRuleset(&contents, GetManyRules());

  ScopedTempRulesetFile source_ruleset(RulesetFormat::kFilterList);
  ScopedTempRulesetFile target_ruleset_url(RulesetFormat::kFilterList);
  ScopedTempRulesetFile target_ruleset_css(RulesetFormat::kFilterList);
  WriteTestRuleset(source_ruleset, contents);

  std::unique_ptr<RuleInputStream> input = OpenRuleset(source_ruleset);
  std::unique_ptr<RuleOutputStream> output_url =
      OpenNewRuleset(target_ruleset_url);
  std::unique_ptr<RuleOutputStream> output_css =
      OpenNewRuleset(target_ruleset_css);
  TransferRules(input.get(), output_url.get(), output_css.get());
  EXPECT_TRUE(output_url->Finish());
  EXPECT_TRUE(output_css->Finish());
  input.reset();
  output_url.reset();
  output_css.reset();

  TestRulesetContents only_url_rules;
  only_url_rules.url_rules = contents.url_rules;
  ReadTestRulesetAndExpectContents(target_ruleset_url, only_url_rules);

  contents.url_rules.clear();
  ReadTestRulesetAndExpectContents(target_ruleset_css, contents);
}

TEST(RuleStreamTest, TransferRulesAndDiscardRegexpRules) {
  TestRulesetContents contents;
  PrepareRuleset(&contents, GetManyRules());

  ScopedTempRulesetFile source_ruleset(RulesetFormat::kFilterList);
  ScopedTempRulesetFile target_ruleset(RulesetFormat::kFilterList);
  WriteTestRuleset(source_ruleset, contents);

  std::unique_ptr<RuleInputStream> input = OpenRuleset(source_ruleset);
  std::unique_ptr<RuleOutputStream> output = OpenNewRuleset(target_ruleset);
  TransferRules(input.get(), output.get(), nullptr, 54 /* chrome_version */);
  EXPECT_TRUE(output->Finish());
  input.reset();
  output.reset();

  contents.url_rules.erase(
      std::remove_if(contents.url_rules.begin(), contents.url_rules.end(),
                     [](const url_pattern_index::proto::UrlRule& rule) {
                       return rule.url_pattern_type() ==
                              url_pattern_index::proto::URL_PATTERN_TYPE_REGEXP;
                     }),
      contents.url_rules.end());
  contents.css_rules.clear();
  ReadTestRulesetAndExpectContents(target_ruleset, contents);
}

TEST(RuleStreamTest, TransferRulesChromeVersion) {
  TestRulesetContents contents;
  PrepareRuleset(&contents, GetSomeChromeUnfriendlyRules());
  PrepareRuleset(&contents, GetManyRules());

  ScopedTempRulesetFile source_ruleset(RulesetFormat::kFilterList);
  WriteTestRuleset(source_ruleset, contents);

  for (int chrome_version : {0, 54, 59}) {
    TestRulesetContents expected_contents;
    for (url_pattern_index::proto::UrlRule url_rule : contents.url_rules) {
      if (DeleteUrlRuleOrAmend(&url_rule, chrome_version))
        continue;
      expected_contents.url_rules.push_back(url_rule);
    }

    ScopedTempRulesetFile target_ruleset(RulesetFormat::kFilterList);
    std::unique_ptr<RuleOutputStream> output = OpenNewRuleset(target_ruleset);
    std::unique_ptr<RuleInputStream> input = OpenRuleset(source_ruleset);
    TransferRules(input.get(), output.get(), nullptr, chrome_version);
    EXPECT_TRUE(output->Finish());
    input.reset();
    output.reset();

    ReadTestRulesetAndExpectContents(target_ruleset, expected_contents);
  }
}

TEST(RuleStreamTest, TransferRulesFromFilterListWithUnsupportedOptions) {
  std::vector<std::string> text_rules = GetSomeRules();
  const size_t number_of_correct_rules = text_rules.size();

  // Insert several rules with non-critical parse errors.
  text_rules.insert(text_rules.begin(), "host1$donottrack");
  text_rules.push_back("");
  text_rules.insert(text_rules.begin() + text_rules.size() / 2,
                    "host3$collapse");

  ScopedTempRulesetFile source_ruleset(RulesetFormat::kFilterList);
  ScopedTempRulesetFile target_ruleset(RulesetFormat::kFilterList);

  // Output all the rules to the |source_ruleset| file.
  std::string joined_rules = base::JoinString(text_rules, "\n");
  base::WriteFile(source_ruleset.ruleset_path(), joined_rules.data(),
                  joined_rules.size());

  // Filter out the rules with parse errors, and save the rest to |contents|.
  TestRulesetContents contents;
  PrepareRuleset(&contents, text_rules, true /* allow_errors */);

  // Make sure all the rules with no errors were transferred.
  {
    std::unique_ptr<RuleInputStream> input = OpenRuleset(source_ruleset);
    std::unique_ptr<RuleOutputStream> output = OpenNewRuleset(target_ruleset);
    TransferRules(input.get(), output.get(), output.get());
    EXPECT_TRUE(output->Finish());
  }

  EXPECT_EQ(number_of_correct_rules,
            contents.url_rules.size() + contents.css_rules.size());
  ReadTestRulesetAndExpectContents(target_ruleset, contents);
}

TEST(RuleStreamTest, DeleteUrlRuleOrAmend) {
  const struct TestCase {
    const char* rule;
    const char* chrome_54_rule;
    const char* chrome_59_rule;
  } kTestCases[] = {
      {"/a[0-9].com/$image", nullptr, nullptr},
      {"a.com$image,popup", "a.com$image,~popup", "#54"},
      {"a.com$popup", nullptr, nullptr},
      {"a.com$~image", "a.com$~image,~popup,~websocket", "#0"},
      {"a.com$~popup", "a.com$~popup,~websocket", "a.com"},
      {"a.com$~image,~popup", "a.com$~image,~popup,~websocket", "#0"},
      {"@@a.com$subdocument,document", "#0", "#0"},
      {"@@a.com$document,generichide", "@@a.com$document", "#54"},
      {"@@a.com$document", "#0", "#0"},
      {"@@a.com$genericblock", "#0", "#0"},
      {"@@a.com$elemhide", nullptr, nullptr},
      {"@@a.com$generichide", nullptr, nullptr},
      {"@@a.com$elemhide,generichide", nullptr, nullptr},
      {"@@a.com$image,elemhide,generichide", "@@a.com$image", "#54"},
      {"a.com$image,~image", nullptr, nullptr},
  };

  auto get_target_rule = [](const TestCase& test, std::string target_rule) {
    RuleParser parser;
    if (target_rule == "#0")
      target_rule = test.rule;
    else if (target_rule == "#54")
      target_rule = test.chrome_54_rule;
    EXPECT_EQ(url_pattern_index::proto::RULE_TYPE_URL,
              parser.Parse(target_rule));
    return parser.url_rule().ToProtobuf();
  };

  RuleParser parser;
  for (const auto& test : kTestCases) {
    SCOPED_TRACE(test.rule);
    ASSERT_EQ(url_pattern_index::proto::RULE_TYPE_URL, parser.Parse(test.rule));
    const url_pattern_index::proto::UrlRule current_rule =
        parser.url_rule().ToProtobuf();

    url_pattern_index::proto::UrlRule modified_rule = current_rule;
    EXPECT_FALSE(DeleteUrlRuleOrAmend(&modified_rule, 0));
    EXPECT_TRUE(IsEqualRules(modified_rule, current_rule));

    modified_rule = current_rule;
    EXPECT_EQ(!test.chrome_54_rule, DeleteUrlRuleOrAmend(&modified_rule, 54));
    if (test.chrome_54_rule) {
      EXPECT_TRUE(IsEqualRules(modified_rule,
                               get_target_rule(test, test.chrome_54_rule)));
    }

    modified_rule = current_rule;
    EXPECT_EQ(!test.chrome_59_rule, DeleteUrlRuleOrAmend(&modified_rule, 59));
    if (test.chrome_59_rule) {
      EXPECT_TRUE(IsEqualRules(modified_rule,
                               get_target_rule(test, test.chrome_59_rule)));
    }
  }
}

}  // namespace subresource_filter
