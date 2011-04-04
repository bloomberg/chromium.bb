// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/keyword_provider.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/test/testing_browser_process.h"
#include "chrome/test/testing_browser_process_test.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

class KeywordProviderTest : public TestingBrowserProcessTest {
 protected:
  template<class ResultType>
  struct test_data {
    const string16 input;
    const size_t num_results;
    const ResultType output[3];
  };

  KeywordProviderTest() : kw_provider_(NULL) { }
  virtual ~KeywordProviderTest() { }

  virtual void SetUp();
  virtual void TearDown();

  template<class ResultType>
  void RunTest(test_data<ResultType>* keyword_cases,
               int num_cases,
               ResultType AutocompleteMatch::* member);

 protected:
  scoped_refptr<KeywordProvider> kw_provider_;
  scoped_ptr<TemplateURLModel> model_;
};

void KeywordProviderTest::SetUp() {
  static const TemplateURLModel::Initializer kTestKeywordData[] = {
    { "aa", "aa.com?foo=%s", "aa" },
    { "aaaa", "http://aaaa/?aaaa=1&b=%s&c", "aaaa" },
    { "aaaaa", "%s", "aaaaa" },
    { "ab", "bogus URL %s", "ab" },
    { "weasel", "weasel%sweasel", "weasel" },
    { "www", " +%2B?=%sfoo ", "www" },
    { "z", "%s=z", "z" },
  };

  model_.reset(new TemplateURLModel(kTestKeywordData,
                                    arraysize(kTestKeywordData)));
  kw_provider_ = new KeywordProvider(NULL, model_.get());
}

void KeywordProviderTest::TearDown() {
  model_.reset();
  kw_provider_ = NULL;
}

template<class ResultType>
void KeywordProviderTest::RunTest(
    test_data<ResultType>* keyword_cases,
    int num_cases,
    ResultType AutocompleteMatch::* member) {
  ACMatches matches;
  for (int i = 0; i < num_cases; ++i) {
    AutocompleteInput input(keyword_cases[i].input, string16(), true,
                            false, true, AutocompleteInput::ALL_MATCHES);
    kw_provider_->Start(input, false);
    EXPECT_TRUE(kw_provider_->done());
    matches = kw_provider_->matches();
    EXPECT_EQ(keyword_cases[i].num_results, matches.size()) <<
                ASCIIToUTF16("Input was: ") + keyword_cases[i].input;
    if (matches.size() == keyword_cases[i].num_results) {
      for (size_t j = 0; j < keyword_cases[i].num_results; ++j) {
        EXPECT_EQ(keyword_cases[i].output[j], matches[j].*member);
      }
    }
  }
}

TEST_F(KeywordProviderTest, Edit) {
  test_data<string16> edit_cases[] = {
    // Searching for a nonexistent prefix should give nothing.
    {ASCIIToUTF16("Not Found"),       0, {}},
    {ASCIIToUTF16("aaaaaNot Found"),  0, {}},

    // Check that tokenization only collapses whitespace between first tokens,
    // no-query-input cases have a space appended, and action is not escaped.
    {ASCIIToUTF16("z foo"),           1, {ASCIIToUTF16("z foo")}},
    {ASCIIToUTF16("z"),               1, {ASCIIToUTF16("z ")}},
    {ASCIIToUTF16("z    \t"),         1, {ASCIIToUTF16("z ")}},
    {ASCIIToUTF16("z   a   b   c++"), 1, {ASCIIToUTF16("z a   b   c++")}},

    // Matches should be limited to three, and sorted in quality order, not
    // alphabetical.
    {ASCIIToUTF16("aaa"),             2, {ASCIIToUTF16("aaaa "),
                                          ASCIIToUTF16("aaaaa ")}},
    {ASCIIToUTF16("a 1 2 3"),         3, {ASCIIToUTF16("aa 1 2 3"),
                                          ASCIIToUTF16("ab 1 2 3"),
                                          ASCIIToUTF16("aaaa 1 2 3")}},
    {ASCIIToUTF16("www.a"),           3, {ASCIIToUTF16("aa "),
                                          ASCIIToUTF16("ab "),
                                          ASCIIToUTF16("aaaa ")}},
    // Exact matches should prevent returning inexact matches.
    {ASCIIToUTF16("aaaa foo"),        1, {ASCIIToUTF16("aaaa foo")}},
    {ASCIIToUTF16("www.aaaa foo"),    1, {ASCIIToUTF16("aaaa foo")}},

    // Clean up keyword input properly.  "http" and "https" are the only
    // allowed schemes.
    {ASCIIToUTF16("www"),             1, {ASCIIToUTF16("www ")}},
    {ASCIIToUTF16("www."),            0, {}},
    {ASCIIToUTF16("www.w w"),         2, {ASCIIToUTF16("www w"),
                                          ASCIIToUTF16("weasel w")}},
    {ASCIIToUTF16("http://www"),      1, {ASCIIToUTF16("www ")}},
    {ASCIIToUTF16("http://www."),     0, {}},
    {ASCIIToUTF16("ftp: blah"),       0, {}},
    {ASCIIToUTF16("mailto:z"),        0, {}},
    {ASCIIToUTF16("ftp://z"),         0, {}},
    {ASCIIToUTF16("https://z"),       1, {ASCIIToUTF16("z ")}},
  };

  RunTest<string16>(edit_cases, arraysize(edit_cases),
                    &AutocompleteMatch::fill_into_edit);
}

TEST_F(KeywordProviderTest, URL) {
  test_data<GURL> url_cases[] = {
    // No query input -> empty destination URL.
    {ASCIIToUTF16("z"),               1, {GURL()}},
    {ASCIIToUTF16("z    \t"),         1, {GURL()}},

    // Check that tokenization only collapses whitespace between first tokens
    // and query input, but not rest of URL, is escaped.
    {ASCIIToUTF16("z   a   b   c++"), 1, {GURL("a+++b+++c%2B%2B=z")}},
    {ASCIIToUTF16("www.www www"),     1, {GURL(" +%2B?=wwwfoo ")}},

    // Substitution should work with various locations of the "%s".
    {ASCIIToUTF16("aaa 1a2b"),        2, {GURL("http://aaaa/?aaaa=1&b=1a2b&c"),
                                          GURL("1a2b")}},
    {ASCIIToUTF16("a 1 2 3"),         3, {GURL("aa.com?foo=1+2+3"),
                                          GURL("bogus URL 1+2+3"),
                                        GURL("http://aaaa/?aaaa=1&b=1+2+3&c")}},
    {ASCIIToUTF16("www.w w"),         2, {GURL(" +%2B?=wfoo "),
                                          GURL("weaselwweasel")}},
  };

  RunTest<GURL>(url_cases, arraysize(url_cases),
                &AutocompleteMatch::destination_url);
}

TEST_F(KeywordProviderTest, Contents) {
  test_data<string16> contents_cases[] = {
    // No query input -> substitute "<enter query>" into contents.
    {ASCIIToUTF16("z"),               1,
        {ASCIIToUTF16("Search z for <enter query>")}},
    {ASCIIToUTF16("z    \t"),         1,
        {ASCIIToUTF16("Search z for <enter query>")}},

    // Check that tokenization only collapses whitespace between first tokens
    // and contents are not escaped or unescaped.
    {ASCIIToUTF16("z   a   b   c++"), 1,
        {ASCIIToUTF16("Search z for a   b   c++")}},
    {ASCIIToUTF16("www.www www"),     1, {ASCIIToUTF16("Search www for www")}},

    // Substitution should work with various locations of the "%s".
    {ASCIIToUTF16("aaa"),             2,
        {ASCIIToUTF16("Search aaaa for <enter query>"),
         ASCIIToUTF16("Search aaaaa for <enter query>")}},
    {ASCIIToUTF16("a 1 2 3"),         3, {ASCIIToUTF16("Search aa for 1 2 3"),
                                          ASCIIToUTF16("Search ab for 1 2 3"),
                                        ASCIIToUTF16("Search aaaa for 1 2 3")}},
    {ASCIIToUTF16("www.w w"),         2, {ASCIIToUTF16("Search www for w"),
        ASCIIToUTF16("Search weasel for w")}},
  };

  RunTest<string16>(contents_cases, arraysize(contents_cases),
                        &AutocompleteMatch::contents);
}

TEST_F(KeywordProviderTest, Description) {
  test_data<string16> description_cases[] = {
    // Whole keyword should be returned for both exact and inexact matches.
    {ASCIIToUTF16("z foo"),           1, {ASCIIToUTF16("(Keyword: z)")}},
    {ASCIIToUTF16("a foo"),           3, {ASCIIToUTF16("(Keyword: aa)"),
                                          ASCIIToUTF16("(Keyword: ab)"),
                                          ASCIIToUTF16("(Keyword: aaaa)")}},
    {ASCIIToUTF16("ftp://www.www w"), 0, {}},
    {ASCIIToUTF16("http://www.ab w"), 1, {ASCIIToUTF16("(Keyword: ab)")}},

    // Keyword should be returned regardless of query input.
    {ASCIIToUTF16("z"),               1, {ASCIIToUTF16("(Keyword: z)")}},
    {ASCIIToUTF16("z    \t"),         1, {ASCIIToUTF16("(Keyword: z)")}},
    {ASCIIToUTF16("z   a   b   c++"), 1, {ASCIIToUTF16("(Keyword: z)")}},
  };

  RunTest<string16>(description_cases, arraysize(description_cases),
                        &AutocompleteMatch::description);
}

TEST_F(KeywordProviderTest, AddKeyword) {
  TemplateURL* template_url = new TemplateURL();
  string16 keyword(ASCIIToUTF16("foo"));
  std::string url("http://www.google.com/foo?q={searchTerms}");
  template_url->SetURL(url, 0, 0);
  template_url->set_keyword(keyword);
  template_url->set_short_name(ASCIIToUTF16("Test"));
  model_->Add(template_url);
  ASSERT_TRUE(template_url == model_->GetTemplateURLForKeyword(keyword));
}

TEST_F(KeywordProviderTest, RemoveKeyword) {
  string16 url(ASCIIToUTF16("http://aaaa/?aaaa=1&b={searchTerms}&c"));
  model_->Remove(model_->GetTemplateURLForKeyword(ASCIIToUTF16("aaaa")));
  ASSERT_TRUE(model_->GetTemplateURLForKeyword(ASCIIToUTF16("aaaa")) == NULL);
}
