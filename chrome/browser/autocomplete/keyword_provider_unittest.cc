// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/autocomplete/keyword_provider.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/autocomplete/autocomplete_match.h"
#include "components/metrics/proto/omnibox_event.pb.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using base::ASCIIToUTF16;

class KeywordProviderTest : public testing::Test {
 protected:
  template<class ResultType>
  struct MatchType {
    const ResultType member;
    bool allowed_to_be_default_match;
  };

  template<class ResultType>
  struct TestData {
    const base::string16 input;
    const size_t num_results;
    const MatchType<ResultType> output[3];
  };

  KeywordProviderTest() : kw_provider_(NULL) { }
  virtual ~KeywordProviderTest() { }

  virtual void SetUp();
  virtual void TearDown();

  template<class ResultType>
  void RunTest(TestData<ResultType>* keyword_cases,
               int num_cases,
               ResultType AutocompleteMatch::* member);

 protected:
  static const TemplateURLService::Initializer kTestData[];

  scoped_refptr<KeywordProvider> kw_provider_;
  scoped_ptr<TemplateURLService> model_;
};

// static
const TemplateURLService::Initializer KeywordProviderTest::kTestData[] = {
  { "aa", "aa.com?foo={searchTerms}", "aa" },
  { "aaaa", "http://aaaa/?aaaa=1&b={searchTerms}&c", "aaaa" },
  { "aaaaa", "{searchTerms}", "aaaaa" },
  { "ab", "bogus URL {searchTerms}", "ab" },
  { "weasel", "weasel{searchTerms}weasel", "weasel" },
  { "www", " +%2B?={searchTerms}foo ", "www" },
  { "nonsub", "http://nonsubstituting-keyword.com/", "nonsub" },
  { "z", "{searchTerms}=z", "z" },
};

void KeywordProviderTest::SetUp() {
  model_.reset(new TemplateURLService(kTestData, arraysize(kTestData)));
  kw_provider_ = new KeywordProvider(NULL, model_.get());
}

void KeywordProviderTest::TearDown() {
  model_.reset();
  kw_provider_ = NULL;
}

template<class ResultType>
void KeywordProviderTest::RunTest(
    TestData<ResultType>* keyword_cases,
    int num_cases,
    ResultType AutocompleteMatch::* member) {
  ACMatches matches;
  for (int i = 0; i < num_cases; ++i) {
    SCOPED_TRACE(keyword_cases[i].input);
    AutocompleteInput input(keyword_cases[i].input, base::string16::npos,
                            base::string16(), GURL(),
                            metrics::OmniboxEventProto::INVALID_SPEC, true,
                            false, true, true,
                            ChromeAutocompleteSchemeClassifier(NULL));
    kw_provider_->Start(input, false);
    EXPECT_TRUE(kw_provider_->done());
    matches = kw_provider_->matches();
    ASSERT_EQ(keyword_cases[i].num_results, matches.size());
    for (size_t j = 0; j < matches.size(); ++j) {
      EXPECT_EQ(keyword_cases[i].output[j].member, matches[j].*member);
      EXPECT_EQ(keyword_cases[i].output[j].allowed_to_be_default_match,
                matches[j].allowed_to_be_default_match);
    }
  }
}

TEST_F(KeywordProviderTest, Edit) {
  const MatchType<base::string16> kEmptyMatch = { base::string16(), false };
  TestData<base::string16> edit_cases[] = {
    // Searching for a nonexistent prefix should give nothing.
    { ASCIIToUTF16("Not Found"), 0,
      { kEmptyMatch, kEmptyMatch, kEmptyMatch } },
    { ASCIIToUTF16("aaaaaNot Found"), 0,
      { kEmptyMatch, kEmptyMatch, kEmptyMatch } },

    // Check that tokenization only collapses whitespace between first tokens,
    // no-query-input cases have a space appended, and action is not escaped.
    { ASCIIToUTF16("z"), 1,
      { { ASCIIToUTF16("z "), true }, kEmptyMatch, kEmptyMatch } },
    { ASCIIToUTF16("z    \t"), 1,
      { { ASCIIToUTF16("z "), true }, kEmptyMatch, kEmptyMatch } },

    // Check that exact, substituting keywords with a verbatim search term
    // don't generate a result.  (These are handled by SearchProvider.)
    { ASCIIToUTF16("z foo"), 0,
      { kEmptyMatch, kEmptyMatch, kEmptyMatch } },
    { ASCIIToUTF16("z   a   b   c++"), 0,
      { kEmptyMatch, kEmptyMatch, kEmptyMatch } },

    // Matches should be limited to three, and sorted in quality order, not
    // alphabetical.
    { ASCIIToUTF16("aaa"), 2,
      { { ASCIIToUTF16("aaaa "), false },
        { ASCIIToUTF16("aaaaa "), false },
        kEmptyMatch } },
    { ASCIIToUTF16("a 1 2 3"), 3,
     { { ASCIIToUTF16("aa 1 2 3"), false },
       { ASCIIToUTF16("ab 1 2 3"), false },
       { ASCIIToUTF16("aaaa 1 2 3"), false } } },
    { ASCIIToUTF16("www.a"), 3,
      { { ASCIIToUTF16("aa "), false },
        { ASCIIToUTF16("ab "), false },
        { ASCIIToUTF16("aaaa "), false } } },
    // Exact matches should prevent returning inexact matches.  Also, the
    // verbatim query for this keyword match should not be returned.  (It's
    // returned by SearchProvider.)
    { ASCIIToUTF16("aaaa foo"), 0,
      { kEmptyMatch, kEmptyMatch, kEmptyMatch } },
    { ASCIIToUTF16("www.aaaa foo"), 0,
      { kEmptyMatch, kEmptyMatch, kEmptyMatch } },

    // Clean up keyword input properly.  "http" and "https" are the only
    // allowed schemes.
    { ASCIIToUTF16("www"), 1,
      { { ASCIIToUTF16("www "), true }, kEmptyMatch, kEmptyMatch }},
    { ASCIIToUTF16("www."), 0,
      { kEmptyMatch, kEmptyMatch, kEmptyMatch } },
    { ASCIIToUTF16("www.w w"), 2,
      { { ASCIIToUTF16("www w"), false },
        { ASCIIToUTF16("weasel w"), false },
        kEmptyMatch } },
    { ASCIIToUTF16("http://www"), 1,
      { { ASCIIToUTF16("www "), true }, kEmptyMatch, kEmptyMatch } },
    { ASCIIToUTF16("http://www."), 0,
      { kEmptyMatch, kEmptyMatch, kEmptyMatch } },
    { ASCIIToUTF16("ftp: blah"), 0,
      { kEmptyMatch, kEmptyMatch, kEmptyMatch } },
    { ASCIIToUTF16("mailto:z"), 0,
      { kEmptyMatch, kEmptyMatch, kEmptyMatch } },
    { ASCIIToUTF16("ftp://z"), 0,
      { kEmptyMatch, kEmptyMatch, kEmptyMatch } },
    { ASCIIToUTF16("https://z"), 1,
      { { ASCIIToUTF16("z "), true }, kEmptyMatch, kEmptyMatch } },

    // Non-substituting keywords, whether typed fully or not
    // should not add a space.
    { ASCIIToUTF16("nonsu"), 1,
      { { ASCIIToUTF16("nonsub"), false }, kEmptyMatch, kEmptyMatch } },
    { ASCIIToUTF16("nonsub"), 1,
      { { ASCIIToUTF16("nonsub"), true }, kEmptyMatch, kEmptyMatch } },
  };

  RunTest<base::string16>(edit_cases, arraysize(edit_cases),
                    &AutocompleteMatch::fill_into_edit);
}

TEST_F(KeywordProviderTest, URL) {
  const MatchType<GURL> kEmptyMatch = { GURL(), false };
  TestData<GURL> url_cases[] = {
    // No query input -> empty destination URL.
    { ASCIIToUTF16("z"), 1,
      { { GURL(), true }, kEmptyMatch, kEmptyMatch } },
    { ASCIIToUTF16("z    \t"), 1,
      { { GURL(), true }, kEmptyMatch, kEmptyMatch } },

    // Check that tokenization only collapses whitespace between first tokens
    // and query input, but not rest of URL, is escaped.
    { ASCIIToUTF16("w  bar +baz"), 2,
      { { GURL(" +%2B?=bar+%2Bbazfoo "), false },
        { GURL("bar+%2Bbaz=z"), false },
        kEmptyMatch } },

    // Substitution should work with various locations of the "%s".
    { ASCIIToUTF16("aaa 1a2b"), 2,
      { { GURL("http://aaaa/?aaaa=1&b=1a2b&c"), false },
        { GURL("1a2b"), false },
        kEmptyMatch } },
    { ASCIIToUTF16("a 1 2 3"), 3,
      { { GURL("aa.com?foo=1+2+3"), false },
        { GURL("bogus URL 1+2+3"), false },
        { GURL("http://aaaa/?aaaa=1&b=1+2+3&c"), false } } },
    { ASCIIToUTF16("www.w w"), 2,
      { { GURL(" +%2B?=wfoo "), false },
        { GURL("weaselwweasel"), false },
        kEmptyMatch } },
  };

  RunTest<GURL>(url_cases, arraysize(url_cases),
                &AutocompleteMatch::destination_url);
}

TEST_F(KeywordProviderTest, Contents) {
  const MatchType<base::string16> kEmptyMatch = { base::string16(), false };
  TestData<base::string16> contents_cases[] = {
    // No query input -> substitute "<enter query>" into contents.
    { ASCIIToUTF16("z"), 1,
      { { ASCIIToUTF16("Search z for <enter query>"), true },
        kEmptyMatch, kEmptyMatch } },
    { ASCIIToUTF16("z    \t"), 1,
      { { ASCIIToUTF16("Search z for <enter query>"), true },
        kEmptyMatch, kEmptyMatch } },

    // Exact keyword matches with remaining text should return nothing.
    { ASCIIToUTF16("www.www www"), 0,
      { kEmptyMatch, kEmptyMatch, kEmptyMatch } },
    { ASCIIToUTF16("z   a   b   c++"), 0,
      { kEmptyMatch, kEmptyMatch, kEmptyMatch } },

    // Exact keyword matches with remaining text when the keyword is an
    // extension keyword should return something.  This is tested in
    // chrome/browser/extensions/api/omnibox/omnibox_apitest.cc's
    // in OmniboxApiTest's Basic test.

    // Substitution should work with various locations of the "%s".
    { ASCIIToUTF16("aaa"), 2,
      { { ASCIIToUTF16("Search aaaa for <enter query>"), false },
        { ASCIIToUTF16("Search aaaaa for <enter query>"), false },
        kEmptyMatch} },
    { ASCIIToUTF16("www.w w"), 2,
      { { ASCIIToUTF16("Search www for w"), false },
        { ASCIIToUTF16("Search weasel for w"), false },
        kEmptyMatch } },
    // Also, check that tokenization only collapses whitespace between first
    // tokens and contents are not escaped or unescaped.
    { ASCIIToUTF16("a   1 2+ 3"), 3,
      { { ASCIIToUTF16("Search aa for 1 2+ 3"), false },
        { ASCIIToUTF16("Search ab for 1 2+ 3"), false },
        { ASCIIToUTF16("Search aaaa for 1 2+ 3"), false } } },
  };

  RunTest<base::string16>(contents_cases, arraysize(contents_cases),
                    &AutocompleteMatch::contents);
}

TEST_F(KeywordProviderTest, AddKeyword) {
  TemplateURLData data;
  data.short_name = ASCIIToUTF16("Test");
  base::string16 keyword(ASCIIToUTF16("foo"));
  data.SetKeyword(keyword);
  data.SetURL("http://www.google.com/foo?q={searchTerms}");
  TemplateURL* template_url = new TemplateURL(data);
  model_->Add(template_url);
  ASSERT_TRUE(template_url == model_->GetTemplateURLForKeyword(keyword));
}

TEST_F(KeywordProviderTest, RemoveKeyword) {
  base::string16 url(ASCIIToUTF16("http://aaaa/?aaaa=1&b={searchTerms}&c"));
  model_->Remove(model_->GetTemplateURLForKeyword(ASCIIToUTF16("aaaa")));
  ASSERT_TRUE(model_->GetTemplateURLForKeyword(ASCIIToUTF16("aaaa")) == NULL);
}

TEST_F(KeywordProviderTest, GetKeywordForInput) {
  EXPECT_EQ(ASCIIToUTF16("aa"),
      kw_provider_->GetKeywordForText(ASCIIToUTF16("aa")));
  EXPECT_EQ(base::string16(),
      kw_provider_->GetKeywordForText(ASCIIToUTF16("aafoo")));
  EXPECT_EQ(base::string16(),
      kw_provider_->GetKeywordForText(ASCIIToUTF16("aa foo")));
}

TEST_F(KeywordProviderTest, GetSubstitutingTemplateURLForInput) {
  struct {
    const std::string text;
    const size_t cursor_position;
    const bool allow_exact_keyword_match;
    const std::string expected_url;
    const std::string updated_text;
    const size_t updated_cursor_position;
  } cases[] = {
    { "foo", base::string16::npos, true, "", "foo", base::string16::npos },
    { "aa foo", base::string16::npos, true, "aa.com?foo={searchTerms}", "foo",
      base::string16::npos },

    // Cursor adjustment.
    { "aa foo", base::string16::npos, true, "aa.com?foo={searchTerms}", "foo",
      base::string16::npos },
    { "aa foo", 4u, true, "aa.com?foo={searchTerms}", "foo", 1u },
    // Cursor at the end.
    { "aa foo", 6u, true, "aa.com?foo={searchTerms}", "foo", 3u },
    // Cursor before the first character of the remaining text.
    { "aa foo", 3u, true, "aa.com?foo={searchTerms}", "foo", 0u },

    // Trailing space.
    { "aa foo ", 7u, true, "aa.com?foo={searchTerms}", "foo ", 4u },
    // Trailing space without remaining text, cursor in the middle.
    { "aa  ", 3u, true, "aa.com?foo={searchTerms}", "", base::string16::npos },
    // Trailing space without remaining text, cursor at the end.
    { "aa  ", 4u, true, "aa.com?foo={searchTerms}", "", base::string16::npos },
    // Extra space after keyword, cursor at the end.
    { "aa  foo ", 8u, true, "aa.com?foo={searchTerms}", "foo ", 4u },
    // Extra space after keyword, cursor in the middle.
    { "aa  foo ", 3u, true, "aa.com?foo={searchTerms}", "foo ", 0 },
    // Extra space after keyword, no trailing space, cursor at the end.
    { "aa  foo", 7u, true, "aa.com?foo={searchTerms}", "foo", 3u },
    // Extra space after keyword, no trailing space, cursor in the middle.
    { "aa  foo", 5u, true, "aa.com?foo={searchTerms}", "foo", 1u },

    // Disallow exact keyword match.
    { "aa foo", base::string16::npos, false, "", "aa foo",
      base::string16::npos },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); i++) {
    AutocompleteInput input(
        ASCIIToUTF16(cases[i].text), cases[i].cursor_position, base::string16(),
        GURL(), metrics::OmniboxEventProto::INVALID_SPEC, false, false,
        cases[i].allow_exact_keyword_match, true,
        ChromeAutocompleteSchemeClassifier(NULL));
    const TemplateURL* url =
        KeywordProvider::GetSubstitutingTemplateURLForInput(model_.get(),
                                                            &input);
    if (cases[i].expected_url.empty())
      EXPECT_FALSE(url);
    else
      EXPECT_EQ(cases[i].expected_url, url->url());
    EXPECT_EQ(ASCIIToUTF16(cases[i].updated_text), input.text());
    EXPECT_EQ(cases[i].updated_cursor_position, input.cursor_position());
  }
}

// If extra query params are specified on the command line, they should be
// reflected (only) in the default search provider's destination URL.
TEST_F(KeywordProviderTest, ExtraQueryParams) {
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kExtraSearchQueryParams, "a=b");

  TestData<GURL> url_cases[] = {
    { ASCIIToUTF16("a 1 2 3"), 3,
      { { GURL("aa.com?a=b&foo=1+2+3"), false },
        { GURL("bogus URL 1+2+3"), false },
        { GURL("http://aaaa/?aaaa=1&b=1+2+3&c"), false } } },
  };

  RunTest<GURL>(url_cases, arraysize(url_cases),
                &AutocompleteMatch::destination_url);
}
