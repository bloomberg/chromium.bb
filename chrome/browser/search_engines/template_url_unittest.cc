// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/rlz/rlz.h"
#include "chrome/browser/search_engines/search_terms_data.h"
#include "chrome/browser/search_engines/template_url.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(ENABLE_RLZ)
#include "chrome/browser/google/google_util.h"
#endif

// TestSearchTermsData --------------------------------------------------------

// Simple implementation of SearchTermsData.
class TestSearchTermsData : public SearchTermsData {
 public:
  explicit TestSearchTermsData(const std::string& google_base_url);

  virtual std::string GoogleBaseURLValue() const OVERRIDE;

 private:
  std::string google_base_url_;

  DISALLOW_COPY_AND_ASSIGN(TestSearchTermsData);
};

TestSearchTermsData::TestSearchTermsData(const std::string& google_base_url)
    : google_base_url_(google_base_url)  {
}

std::string TestSearchTermsData::GoogleBaseURLValue() const {
  return google_base_url_;
}


// TemplateURLTest ------------------------------------------------------------

class TemplateURLTest : public testing::Test {
 public:
  void CheckSuggestBaseURL(const std::string& base_url,
                           const std::string& base_suggest_url) const;
};

void TemplateURLTest::CheckSuggestBaseURL(
    const std::string& base_url,
    const std::string& base_suggest_url) const {
  TestSearchTermsData search_terms_data(base_url);
  EXPECT_EQ(base_suggest_url, search_terms_data.GoogleBaseSuggestURLValue());
}


// Actual tests ---------------------------------------------------------------

TEST_F(TemplateURLTest, Defaults) {
  TemplateURL url;
  EXPECT_FALSE(url.show_in_default_list());
  EXPECT_FALSE(url.safe_for_autoreplace());
  EXPECT_EQ(0, url.prepopulate_id());
}

TEST_F(TemplateURLTest, TestValidWithComplete) {
  TemplateURL t_url;
  TemplateURLRef ref(&t_url, "{searchTerms}");
  EXPECT_TRUE(ref.IsValid());
}

TEST_F(TemplateURLTest, URLRefTestSearchTerms) {
  struct SearchTermsCase {
    const char* url;
    const string16 terms;
    const std::string output;
    bool valid_url;
  } search_term_cases[] = {
    { "http://foo{searchTerms}", ASCIIToUTF16("sea rch/bar"),
      "http://foosea%20rch%2Fbar", false },
    { "http://foo{searchTerms}?boo=abc", ASCIIToUTF16("sea rch/bar"),
      "http://foosea%20rch%2Fbar?boo=abc", false },
    { "http://foo/?boo={searchTerms}", ASCIIToUTF16("sea rch/bar"),
      "http://foo/?boo=sea+rch%2Fbar", true },
    { "http://en.wikipedia.org/{searchTerms}", ASCIIToUTF16("wiki/?"),
      "http://en.wikipedia.org/wiki%2F%3F", true }
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(search_term_cases); ++i) {
    const SearchTermsCase& value = search_term_cases[i];
    TemplateURL t_url;
    TemplateURLRef ref(&t_url, value.url);
    ASSERT_TRUE(ref.IsValid());

    ASSERT_TRUE(ref.SupportsReplacement());
    std::string result = ref.ReplaceSearchTerms(value.terms,
        TemplateURLRef::NO_SUGGESTIONS_AVAILABLE, string16());
    ASSERT_EQ(value.output, result);
    GURL result_url(result);
    ASSERT_EQ(value.valid_url, result_url.is_valid());
  }
}

TEST_F(TemplateURLTest, URLRefTestCount) {
  TemplateURL t_url;
  TemplateURLRef ref(&t_url, "http://foo{searchTerms}{count?}");
  ASSERT_TRUE(ref.IsValid());
  ASSERT_TRUE(ref.SupportsReplacement());
  GURL result(ref.ReplaceSearchTerms(ASCIIToUTF16("X"),
      TemplateURLRef::NO_SUGGESTIONS_AVAILABLE, string16()));
  ASSERT_TRUE(result.is_valid());
  EXPECT_EQ("http://foox/", result.spec());
}

TEST_F(TemplateURLTest, URLRefTestCount2) {
  TemplateURL t_url;
  TemplateURLRef ref(&t_url, "http://foo{searchTerms}{count}");
  ASSERT_TRUE(ref.IsValid());
  ASSERT_TRUE(ref.SupportsReplacement());
  GURL result(ref.ReplaceSearchTerms(ASCIIToUTF16("X"),
      TemplateURLRef::NO_SUGGESTIONS_AVAILABLE, string16()));
  ASSERT_TRUE(result.is_valid());
  EXPECT_EQ("http://foox10/", result.spec());
}

TEST_F(TemplateURLTest, URLRefTestIndices) {
  TemplateURL t_url;
  TemplateURLRef ref(&t_url,
                     "http://foo{searchTerms}x{startIndex?}y{startPage?}");
  ASSERT_TRUE(ref.IsValid());
  ASSERT_TRUE(ref.SupportsReplacement());
  GURL result(ref.ReplaceSearchTerms(ASCIIToUTF16("X"),
      TemplateURLRef::NO_SUGGESTIONS_AVAILABLE, string16()));
  ASSERT_TRUE(result.is_valid());
  EXPECT_EQ("http://fooxxy/", result.spec());
}

TEST_F(TemplateURLTest, URLRefTestIndices2) {
  TemplateURL t_url;
  TemplateURLRef ref(&t_url,
                     "http://foo{searchTerms}x{startIndex}y{startPage}");
  ASSERT_TRUE(ref.IsValid());
  ASSERT_TRUE(ref.SupportsReplacement());
  GURL result(ref.ReplaceSearchTerms(ASCIIToUTF16("X"),
      TemplateURLRef::NO_SUGGESTIONS_AVAILABLE, string16()));
  ASSERT_TRUE(result.is_valid());
  EXPECT_EQ("http://fooxx1y1/", result.spec());
}

TEST_F(TemplateURLTest, URLRefTestEncoding) {
  TemplateURL t_url;
  TemplateURLRef ref(&t_url,
      "http://foo{searchTerms}x{inputEncoding?}y{outputEncoding?}a");
  ASSERT_TRUE(ref.IsValid());
  ASSERT_TRUE(ref.SupportsReplacement());
  GURL result(ref.ReplaceSearchTerms(ASCIIToUTF16("X"),
      TemplateURLRef::NO_SUGGESTIONS_AVAILABLE, string16()));
  ASSERT_TRUE(result.is_valid());
  EXPECT_EQ("http://fooxxutf-8ya/", result.spec());
}

// Test that setting the prepopulate ID from TemplateURL causes the stored
// TemplateURLRef to handle parsing the URL parameters differently.
TEST_F(TemplateURLTest, SetPrepopulatedAndParse) {
  TemplateURL t_url;
  t_url.SetURL("http://foo{fhqwhgads}");
  TemplateURLRef::Replacements replacements;
  bool valid = false;
  EXPECT_EQ("http://foo{fhqwhgads}",
      t_url.url()->ParseURL("http://foo{fhqwhgads}", &replacements, &valid));
  EXPECT_TRUE(replacements.empty());
  EXPECT_TRUE(valid);

  t_url.SetPrepopulateId(123);
  EXPECT_EQ("http://foo",
      t_url.url()->ParseURL("http://foo{fhqwhgads}", &replacements, &valid));
  EXPECT_TRUE(replacements.empty());
  EXPECT_TRUE(valid);
}

TEST_F(TemplateURLTest, InputEncodingBeforeSearchTerm) {
  TemplateURL t_url;
  TemplateURLRef ref(&t_url,
      "http://foox{inputEncoding?}a{searchTerms}y{outputEncoding?}b");
  ASSERT_TRUE(ref.IsValid());
  ASSERT_TRUE(ref.SupportsReplacement());
  GURL result(ref.ReplaceSearchTerms(ASCIIToUTF16("X"),
      TemplateURLRef::NO_SUGGESTIONS_AVAILABLE, string16()));
  ASSERT_TRUE(result.is_valid());
  EXPECT_EQ("http://fooxutf-8axyb/", result.spec());
}

TEST_F(TemplateURLTest, URLRefTestEncoding2) {
  TemplateURL t_url;
  TemplateURLRef ref(&t_url,
      "http://foo{searchTerms}x{inputEncoding}y{outputEncoding}a");
  ASSERT_TRUE(ref.IsValid());
  ASSERT_TRUE(ref.SupportsReplacement());
  GURL result(ref.ReplaceSearchTerms(ASCIIToUTF16("X"),
      TemplateURLRef::NO_SUGGESTIONS_AVAILABLE, string16()));
  ASSERT_TRUE(result.is_valid());
  EXPECT_EQ("http://fooxxutf-8yutf-8a/", result.spec());
}

TEST_F(TemplateURLTest, URLRefTestSearchTermsUsingTermsData) {
  struct SearchTermsCase {
    const char* url;
    const string16 terms;
    const char* output;
  } search_term_cases[] = {
    { "{google:baseURL}{language}{searchTerms}", string16(),
      "http://example.com/e/en" },
    { "{google:baseSuggestURL}{searchTerms}", string16(),
      "http://example.com/complete/" }
  };

  TestSearchTermsData search_terms_data("http://example.com/e/");
  TemplateURL t_url;
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(search_term_cases); ++i) {
    const SearchTermsCase& value = search_term_cases[i];
    TemplateURLRef ref(&t_url, value.url);
    ASSERT_TRUE(ref.IsValid());

    ASSERT_TRUE(ref.SupportsReplacement());
    GURL result(ref.ReplaceSearchTermsUsingTermsData(value.terms,
        TemplateURLRef::NO_SUGGESTIONS_AVAILABLE, string16(),
        search_terms_data));
    ASSERT_TRUE(result.is_valid());
    EXPECT_EQ(value.output, result.spec());
  }
}

TEST_F(TemplateURLTest, URLRefTermToWide) {
  struct ToWideCase {
    const char* encoded_search_term;
    const string16 expected_decoded_term;
  } to_wide_cases[] = {
    {"hello+world", ASCIIToUTF16("hello world")},
    // Test some big-5 input.
    {"%a7A%A6%6e+to+you", WideToUTF16(L"\x4f60\x597d to you")},
    // Test some UTF-8 input. We should fall back to this when the encoding
    // doesn't look like big-5. We have a '5' in the middle, which is an invalid
    // Big-5 trailing byte.
    {"%e4%bd%a05%e5%a5%bd+to+you", WideToUTF16(L"\x4f60\x35\x597d to you")},
    // Undecodable input should stay escaped.
    {"%91%01+abcd", WideToUTF16(L"%91%01 abcd")},
    // Make sure we convert %2B to +.
    {"C%2B%2B", ASCIIToUTF16("C++")},
    // C%2B is escaped as C%252B, make sure we unescape it properly.
    {"C%252B", ASCIIToUTF16("C%2B")},
  };

  // Set one input encoding: big-5. This is so we can test fallback to UTF-8.
  TemplateURL t_url;
  std::vector<std::string> encodings;
  encodings.push_back("big-5");
  t_url.set_input_encodings(encodings);

  TemplateURLRef ref(&t_url, "http://foo?q={searchTerms}");
  ASSERT_TRUE(ref.IsValid());
  ASSERT_TRUE(ref.SupportsReplacement());

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(to_wide_cases); i++) {
    EXPECT_EQ(to_wide_cases[i].expected_decoded_term,
              ref.SearchTermToString16(to_wide_cases[i].encoded_search_term));
  }
}

TEST_F(TemplateURLTest, DisplayURLToURLRef) {
  struct TestData {
    const std::string url;
    const string16 expected_result;
  } test_data[] = {
    { "http://foo{searchTerms}x{inputEncoding}y{outputEncoding}a",
      ASCIIToUTF16("http://foo%sx{inputEncoding}y{outputEncoding}a") },
    { "http://X",
      ASCIIToUTF16("http://X") },
    { "http://foo{searchTerms",
      ASCIIToUTF16("http://foo{searchTerms") },
    { "http://foo{searchTerms}{language}",
      ASCIIToUTF16("http://foo%s{language}") },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_data); ++i) {
    TemplateURL t_url;
    TemplateURLRef ref(&t_url, test_data[i].url);
    EXPECT_EQ(test_data[i].expected_result, ref.DisplayURL());
    EXPECT_EQ(test_data[i].url,
              TemplateURLRef::DisplayURLToURLRef(ref.DisplayURL()));
  }
}

TEST_F(TemplateURLTest, ReplaceSearchTerms) {
  struct TestData {
    const std::string url;
    const std::string expected_result;
  } test_data[] = {
    { "http://foo/{language}{searchTerms}{inputEncoding}",
      "http://foo/{language}XUTF-8" },
    { "http://foo/{language}{inputEncoding}{searchTerms}",
      "http://foo/{language}UTF-8X" },
    { "http://foo/{searchTerms}{language}{inputEncoding}",
      "http://foo/X{language}UTF-8" },
    { "http://foo/{searchTerms}{inputEncoding}{language}",
      "http://foo/XUTF-8{language}" },
    { "http://foo/{inputEncoding}{searchTerms}{language}",
      "http://foo/UTF-8X{language}" },
    { "http://foo/{inputEncoding}{language}{searchTerms}",
      "http://foo/UTF-8{language}X" },
    { "http://foo/{language}a{searchTerms}a{inputEncoding}a",
      "http://foo/{language}aXaUTF-8a" },
    { "http://foo/{language}a{inputEncoding}a{searchTerms}a",
      "http://foo/{language}aUTF-8aXa" },
    { "http://foo/{searchTerms}a{language}a{inputEncoding}a",
      "http://foo/Xa{language}aUTF-8a" },
    { "http://foo/{searchTerms}a{inputEncoding}a{language}a",
      "http://foo/XaUTF-8a{language}a" },
    { "http://foo/{inputEncoding}a{searchTerms}a{language}a",
      "http://foo/UTF-8aXa{language}a" },
    { "http://foo/{inputEncoding}a{language}a{searchTerms}a",
      "http://foo/UTF-8a{language}aXa" },
  };
  TemplateURL turl;
  turl.add_input_encoding("UTF-8");
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_data); ++i) {
    TemplateURLRef ref(&turl, test_data[i].url);
    EXPECT_TRUE(ref.IsValid());
    EXPECT_TRUE(ref.SupportsReplacement());
    std::string expected_result = test_data[i].expected_result;
    ReplaceSubstringsAfterOffset(&expected_result, 0, "{language}",
        g_browser_process->GetApplicationLocale());
    GURL result(ref.ReplaceSearchTerms(ASCIIToUTF16("X"),
        TemplateURLRef::NO_SUGGESTIONS_AVAILABLE, string16()));
    ASSERT_TRUE(result.is_valid());
    EXPECT_EQ(expected_result, result.spec());
  }
}


// Tests replacing search terms in various encodings and making sure the
// generated URL matches the expected value.
TEST_F(TemplateURLTest, ReplaceArbitrarySearchTerms) {
  struct TestData {
    const std::string encoding;
    const string16 search_term;
    const std::string url;
    const std::string expected_result;
  } test_data[] = {
    { "BIG5",  WideToUTF16(L"\x60BD"),
      "http://foo/?{searchTerms}{inputEncoding}",
      "http://foo/?%B1~BIG5" },
    { "UTF-8", ASCIIToUTF16("blah"),
      "http://foo/?{searchTerms}{inputEncoding}",
      "http://foo/?blahUTF-8" },
    { "Shift_JIS", UTF8ToUTF16("\xe3\x81\x82"),
      "http://foo/{searchTerms}/bar",
      "http://foo/%82%A0/bar"},
    { "Shift_JIS", UTF8ToUTF16("\xe3\x81\x82 \xe3\x81\x84"),
      "http://foo/{searchTerms}/bar",
      "http://foo/%82%A0%20%82%A2/bar"},
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_data); ++i) {
    TemplateURL turl;
    turl.add_input_encoding(test_data[i].encoding);
    TemplateURLRef ref(&turl, test_data[i].url);
    GURL result(ref.ReplaceSearchTerms(test_data[i].search_term,
        TemplateURLRef::NO_SUGGESTIONS_AVAILABLE, string16()));
    ASSERT_TRUE(result.is_valid());
    EXPECT_EQ(test_data[i].expected_result, result.spec());
  }
}

TEST_F(TemplateURLTest, Suggestions) {
  struct TestData {
    const int accepted_suggestion;
    const string16 original_query_for_suggestion;
    const std::string expected_result;
  } test_data[] = {
    { TemplateURLRef::NO_SUGGESTIONS_AVAILABLE, string16(),
      "http://bar/foo?q=foobar" },
    { TemplateURLRef::NO_SUGGESTIONS_AVAILABLE, ASCIIToUTF16("foo"),
      "http://bar/foo?q=foobar" },
    { TemplateURLRef::NO_SUGGESTION_CHOSEN, string16(),
      "http://bar/foo?aq=f&q=foobar" },
    { TemplateURLRef::NO_SUGGESTION_CHOSEN, ASCIIToUTF16("foo"),
      "http://bar/foo?aq=f&q=foobar" },
    { 0, string16(), "http://bar/foo?aq=0&oq=&q=foobar" },
    { 1, ASCIIToUTF16("foo"), "http://bar/foo?aq=1&oq=foo&q=foobar" },
  };
  TemplateURL turl;
  turl.add_input_encoding("UTF-8");
  TemplateURLRef ref(&turl, "http://bar/foo?{google:acceptedSuggestion}"
      "{google:originalQueryForSuggestion}q={searchTerms}");
  ASSERT_TRUE(ref.IsValid());
  ASSERT_TRUE(ref.SupportsReplacement());
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_data); ++i) {
    GURL result(ref.ReplaceSearchTerms(ASCIIToUTF16("foobar"),
        test_data[i].accepted_suggestion,
        test_data[i].original_query_for_suggestion));
    ASSERT_TRUE(result.is_valid());
    EXPECT_EQ(test_data[i].expected_result, result.spec());
  }
}

#if defined(OS_WIN) || defined(OS_MACOSX)
TEST_F(TemplateURLTest, RLZ) {
  string16 rlz_string;
#if defined(ENABLE_RLZ)
  std::string brand;
  if (google_util::GetBrand(&brand) && !brand.empty() &&
      !google_util::IsOrganic(brand)) {
    RLZTracker::GetAccessPointRlz(rlz_lib::CHROME_OMNIBOX, &rlz_string);
  }
#endif

  TemplateURL t_url;
  TemplateURLRef ref(&t_url, "http://bar/?{google:RLZ}{searchTerms}");
  ASSERT_TRUE(ref.IsValid());
  ASSERT_TRUE(ref.SupportsReplacement());
  GURL result(ref.ReplaceSearchTerms(ASCIIToUTF16("x"),
      TemplateURLRef::NO_SUGGESTIONS_AVAILABLE, string16()));
  ASSERT_TRUE(result.is_valid());
  std::string expected_url = "http://bar/?";
  if (!rlz_string.empty())
    expected_url += "rlz=" + UTF16ToUTF8(rlz_string) + "&";
  expected_url += "x";
  EXPECT_EQ(expected_url, result.spec());
}
#endif

TEST_F(TemplateURLTest, HostAndSearchTermKey) {
  struct TestData {
    const std::string url;
    const std::string host;
    const std::string path;
    const std::string search_term_key;
  } data[] = {
    { "http://blah/?foo=bar&q={searchTerms}&b=x", "blah", "/", "q"},

    // No query key should result in empty values.
    { "http://blah/{searchTerms}", "", "", ""},

    // No term should result in empty values.
    { "http://blah/", "", "", ""},

    // Multiple terms should result in empty values.
    { "http://blah/?q={searchTerms}&x={searchTerms}", "", "", ""},

    // Term in the host shouldn't match.
    { "http://{searchTerms}", "", "", ""},

    { "http://blah/?q={searchTerms}", "blah", "/", "q"},

    // Single term with extra chars in value should match.
    { "http://blah/?q=stock:{searchTerms}", "blah", "/", "q"},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(data); ++i) {
    TemplateURL t_url;
    t_url.SetURL(data[i].url);
    EXPECT_EQ(data[i].host, t_url.url()->GetHost());
    EXPECT_EQ(data[i].path, t_url.url()->GetPath());
    EXPECT_EQ(data[i].search_term_key, t_url.url()->GetSearchTermKey());
  }
}

TEST_F(TemplateURLTest, GoogleBaseSuggestURL) {
  static const struct {
    const char* const base_url;
    const char* const base_suggest_url;
  } data[] = {
    { "http://google.com/", "http://google.com/complete/", },
    { "http://www.google.com/", "http://www.google.com/complete/", },
    { "http://www.google.co.uk/", "http://www.google.co.uk/complete/", },
    { "http://www.google.com.by/", "http://www.google.com.by/complete/", },
    { "http://google.com/intl/xx/", "http://google.com/complete/", },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(data); ++i)
    CheckSuggestBaseURL(data[i].base_url, data[i].base_suggest_url);
}

TEST_F(TemplateURLTest, Keyword) {
  TemplateURL t_url;
  t_url.SetURL("http://www.google.com/search");
  EXPECT_FALSE(t_url.autogenerate_keyword());
  t_url.set_keyword(ASCIIToUTF16("foo"));
  EXPECT_EQ(ASCIIToUTF16("foo"), t_url.keyword());
  t_url.set_autogenerate_keyword(true);
  EXPECT_TRUE(t_url.autogenerate_keyword());
  EXPECT_EQ(ASCIIToUTF16("google.com"), t_url.keyword());
  t_url.set_keyword(ASCIIToUTF16("foo"));
  EXPECT_FALSE(t_url.autogenerate_keyword());
  EXPECT_EQ(ASCIIToUTF16("foo"), t_url.keyword());
}

TEST_F(TemplateURLTest, ParseParameterKnown) {
  std::string parsed_url("{searchTerms}");
  TemplateURL t_url;
  TemplateURLRef url_ref(&t_url, parsed_url);
  TemplateURLRef::Replacements replacements;
  EXPECT_TRUE(url_ref.ParseParameter(0, 12, &parsed_url, &replacements));
  EXPECT_EQ(std::string(), parsed_url);
  ASSERT_EQ(1U, replacements.size());
  EXPECT_EQ(0U, replacements[0].index);
  EXPECT_EQ(TemplateURLRef::SEARCH_TERMS, replacements[0].type);
}

TEST_F(TemplateURLTest, ParseParameterUnknown) {
  std::string parsed_url("{fhqwhgads}");
  TemplateURL t_url;
  TemplateURLRef url_ref(&t_url, parsed_url);
  TemplateURLRef::Replacements replacements;

  // By default, TemplateURLRef should not consider itself prepopulated.
  // Therefore we should not replace the unknown parameter.
  EXPECT_FALSE(url_ref.ParseParameter(0, 10, &parsed_url, &replacements));
  EXPECT_EQ("{fhqwhgads}", parsed_url);
  EXPECT_TRUE(replacements.empty());

  // If the TemplateURLRef is prepopulated, we should remove unknown parameters.
  parsed_url = "{fhqwhgads}";
  url_ref.prepopulated_ = true;
  EXPECT_FALSE(url_ref.ParseParameter(0, 10, &parsed_url, &replacements));
  EXPECT_EQ(std::string(), parsed_url);
  EXPECT_TRUE(replacements.empty());
}

TEST_F(TemplateURLTest, ParseURLEmpty) {
  TemplateURL t_url;
  TemplateURLRef url_ref(&t_url);
  TemplateURLRef::Replacements replacements;
  bool valid = false;
  EXPECT_EQ(std::string(),
            url_ref.ParseURL(std::string(), &replacements, &valid));
  EXPECT_TRUE(replacements.empty());
  EXPECT_TRUE(valid);
}

TEST_F(TemplateURLTest, ParseURLNoTemplateEnd) {
  TemplateURL t_url;
  TemplateURLRef url_ref(&t_url, "{");
  TemplateURLRef::Replacements replacements;
  bool valid = false;
  EXPECT_EQ(std::string(), url_ref.ParseURL("{", &replacements, &valid));
  EXPECT_TRUE(replacements.empty());
  EXPECT_FALSE(valid);
}

TEST_F(TemplateURLTest, ParseURLNoKnownParameters) {
  TemplateURL t_url;
  TemplateURLRef url_ref(&t_url, "{}");
  TemplateURLRef::Replacements replacements;
  bool valid = false;
  EXPECT_EQ("{}", url_ref.ParseURL("{}", &replacements, &valid));
  EXPECT_TRUE(replacements.empty());
  EXPECT_TRUE(valid);
}

TEST_F(TemplateURLTest, ParseURLTwoParameters) {
  TemplateURL t_url;
  TemplateURLRef url_ref(&t_url, "{}{{%s}}");
  TemplateURLRef::Replacements replacements;
  bool valid = false;
  EXPECT_EQ("{}{}",
            url_ref.ParseURL("{}{{searchTerms}}", &replacements, &valid));
  ASSERT_EQ(1U, replacements.size());
  EXPECT_EQ(3U, replacements[0].index);
  EXPECT_EQ(TemplateURLRef::SEARCH_TERMS, replacements[0].type);
  EXPECT_TRUE(valid);
}

TEST_F(TemplateURLTest, ParseURLNestedParameter) {
  TemplateURL t_url;
  TemplateURLRef url_ref(&t_url, "{%s");
  TemplateURLRef::Replacements replacements;
  bool valid = false;
  EXPECT_EQ("{", url_ref.ParseURL("{{searchTerms}", &replacements, &valid));
  ASSERT_EQ(1U, replacements.size());
  EXPECT_EQ(1U, replacements[0].index);
  EXPECT_EQ(TemplateURLRef::SEARCH_TERMS, replacements[0].type);
  EXPECT_TRUE(valid);
}
