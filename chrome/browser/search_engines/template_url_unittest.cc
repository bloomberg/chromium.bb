// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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

// Simple implementation of SearchTermsData.
class TestSearchTermsData : public SearchTermsData {
 public:
  explicit TestSearchTermsData(const char* google_base_url)
      : google_base_url_(google_base_url)  {
  }

  virtual std::string GoogleBaseURLValue() const {
    return google_base_url_;
  }

  virtual std::string GetApplicationLocale() const {
    return "yy";
  }

#if defined(OS_WIN) && defined(GOOGLE_CHROME_BUILD)
  // Returns the value for the Chrome Omnibox rlz.
  virtual string16 GetRlzParameterValue() const {
    return string16();
  }
#endif

 private:
  std::string google_base_url_;

  DISALLOW_COPY_AND_ASSIGN(TestSearchTermsData);
};

class TemplateURLTest : public testing::Test {
 public:
  virtual void TearDown() {
    TemplateURLRef::SetGoogleBaseURL(NULL);
  }

  void CheckSuggestBaseURL(const char* base_url,
                           const char* base_suggest_url) const {
    TestSearchTermsData search_terms_data(base_url);
    EXPECT_STREQ(base_suggest_url,
                 search_terms_data.GoogleBaseSuggestURLValue().c_str());
  }
};

TEST_F(TemplateURLTest, Defaults) {
  TemplateURL url;
  ASSERT_FALSE(url.show_in_default_list());
  ASSERT_FALSE(url.safe_for_autoreplace());
  ASSERT_EQ(0, url.prepopulate_id());
}

TEST_F(TemplateURLTest, TestValidWithComplete) {
  TemplateURLRef ref("{searchTerms}", 0, 0);
  ASSERT_TRUE(ref.IsValid());
}

TEST_F(TemplateURLTest, URLRefTestSearchTerms) {
  struct SearchTermsCase {
    const char* url;
    const string16 terms;
    const char* output;
  } search_term_cases[] = {
    { "http://foo{searchTerms}", ASCIIToUTF16("sea rch/bar"),
      "http://foosea%20rch/bar" },
    { "http://foo{searchTerms}?boo=abc", ASCIIToUTF16("sea rch/bar"),
      "http://foosea%20rch/bar?boo=abc" },
    { "http://foo/?boo={searchTerms}", ASCIIToUTF16("sea rch/bar"),
      "http://foo/?boo=sea+rch%2Fbar" },
    { "http://en.wikipedia.org/{searchTerms}", ASCIIToUTF16("wiki/?"),
      "http://en.wikipedia.org/wiki/%3F" }
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(search_term_cases); ++i) {
    const SearchTermsCase& value = search_term_cases[i];
    TemplateURL t_url;
    TemplateURLRef ref(value.url, 0, 0);
    ASSERT_TRUE(ref.IsValid());

    ASSERT_TRUE(ref.SupportsReplacement());
    GURL result = GURL(ref.ReplaceSearchTerms(t_url, value.terms,
        TemplateURLRef::NO_SUGGESTIONS_AVAILABLE, string16()));
    ASSERT_TRUE(result.is_valid());
    ASSERT_EQ(value.output, result.spec());
  }
}

TEST_F(TemplateURLTest, URLRefTestCount) {
  TemplateURL t_url;
  TemplateURLRef ref("http://foo{searchTerms}{count?}", 0, 0);
  ASSERT_TRUE(ref.IsValid());
  ASSERT_TRUE(ref.SupportsReplacement());
  GURL result = GURL(ref.ReplaceSearchTerms(t_url, ASCIIToUTF16("X"),
      TemplateURLRef::NO_SUGGESTIONS_AVAILABLE, string16()));
  ASSERT_TRUE(result.is_valid());
  ASSERT_EQ("http://foox/", result.spec());
}

TEST_F(TemplateURLTest, URLRefTestCount2) {
  TemplateURL t_url;
  TemplateURLRef ref("http://foo{searchTerms}{count}", 0, 0);
  ASSERT_TRUE(ref.IsValid());
  ASSERT_TRUE(ref.SupportsReplacement());
  GURL result = GURL(ref.ReplaceSearchTerms(t_url, ASCIIToUTF16("X"),
      TemplateURLRef::NO_SUGGESTIONS_AVAILABLE, string16()));
  ASSERT_TRUE(result.is_valid());
  ASSERT_EQ("http://foox10/", result.spec());
}

TEST_F(TemplateURLTest, URLRefTestIndices) {
  TemplateURL t_url;
  TemplateURLRef ref("http://foo{searchTerms}x{startIndex?}y{startPage?}",
                     1, 2);
  ASSERT_TRUE(ref.IsValid());
  ASSERT_TRUE(ref.SupportsReplacement());
  GURL result = GURL(ref.ReplaceSearchTerms(t_url, ASCIIToUTF16("X"),
      TemplateURLRef::NO_SUGGESTIONS_AVAILABLE, string16()));
  ASSERT_TRUE(result.is_valid());
  ASSERT_EQ("http://fooxxy/", result.spec());
}

TEST_F(TemplateURLTest, URLRefTestIndices2) {
  TemplateURL t_url;
  TemplateURLRef ref("http://foo{searchTerms}x{startIndex}y{startPage}", 1, 2);
  ASSERT_TRUE(ref.IsValid());
  ASSERT_TRUE(ref.SupportsReplacement());
  GURL result = GURL(ref.ReplaceSearchTerms(t_url, ASCIIToUTF16("X"),
      TemplateURLRef::NO_SUGGESTIONS_AVAILABLE, string16()));
  ASSERT_TRUE(result.is_valid());
  ASSERT_EQ("http://fooxx1y2/", result.spec());
}

TEST_F(TemplateURLTest, URLRefTestEncoding) {
  TemplateURL t_url;
  TemplateURLRef ref(
      "http://foo{searchTerms}x{inputEncoding?}y{outputEncoding?}a", 1, 2);
  ASSERT_TRUE(ref.IsValid());
  ASSERT_TRUE(ref.SupportsReplacement());
  GURL result = GURL(ref.ReplaceSearchTerms(t_url, ASCIIToUTF16("X"),
      TemplateURLRef::NO_SUGGESTIONS_AVAILABLE, string16()));
  ASSERT_TRUE(result.is_valid());
  ASSERT_EQ("http://fooxxutf-8ya/", result.spec());
}

TEST_F(TemplateURLTest, InputEncodingBeforeSearchTerm) {
  TemplateURL t_url;
  TemplateURLRef ref(
      "http://foox{inputEncoding?}a{searchTerms}y{outputEncoding?}b", 1, 2);
  ASSERT_TRUE(ref.IsValid());
  ASSERT_TRUE(ref.SupportsReplacement());
  GURL result = GURL(ref.ReplaceSearchTerms(t_url, ASCIIToUTF16("X"),
      TemplateURLRef::NO_SUGGESTIONS_AVAILABLE, string16()));
  ASSERT_TRUE(result.is_valid());
  ASSERT_EQ("http://fooxutf-8axyb/", result.spec());
}

TEST_F(TemplateURLTest, URLRefTestEncoding2) {
  TemplateURL t_url;
  TemplateURLRef ref(
      "http://foo{searchTerms}x{inputEncoding}y{outputEncoding}a", 1, 2);
  ASSERT_TRUE(ref.IsValid());
  ASSERT_TRUE(ref.SupportsReplacement());
  GURL result = GURL(ref.ReplaceSearchTerms(t_url, ASCIIToUTF16("X"),
      TemplateURLRef::NO_SUGGESTIONS_AVAILABLE, string16()));
  ASSERT_TRUE(result.is_valid());
  ASSERT_EQ("http://fooxxutf-8yutf-8a/", result.spec());
}

TEST_F(TemplateURLTest, URLRefTestSearchTermsUsingTermsData) {
  struct SearchTermsCase {
    const char* url;
    const string16 terms;
    const char* output;
  } search_term_cases[] = {
    { "{google:baseURL}{language}{searchTerms}", string16(),
      "http://example.com/e/yy" },
    { "{google:baseSuggestURL}{searchTerms}", string16(),
      "http://clients1.example.com/complete/" }
  };

  TestSearchTermsData search_terms_data("http://example.com/e/");
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(search_term_cases); ++i) {
    const SearchTermsCase& value = search_term_cases[i];
    TemplateURL t_url;
    TemplateURLRef ref(value.url, 0, 0);
    ASSERT_TRUE(ref.IsValid());

    ASSERT_TRUE(ref.SupportsReplacement());
    GURL result = GURL(ref.ReplaceSearchTermsUsingTermsData(
        t_url, value.terms,
        TemplateURLRef::NO_SUGGESTIONS_AVAILABLE, string16(),
        search_terms_data));
    ASSERT_TRUE(result.is_valid());
    ASSERT_EQ(value.output, result.spec());
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

  TemplateURL t_url;

  // Set one input encoding: big-5. This is so we can test fallback to UTF-8.
  std::vector<std::string> encodings;
  encodings.push_back("big-5");
  t_url.set_input_encodings(encodings);

  TemplateURLRef ref("http://foo?q={searchTerms}", 1, 2);
  ASSERT_TRUE(ref.IsValid());
  ASSERT_TRUE(ref.SupportsReplacement());

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(to_wide_cases); i++) {
    string16 result = ref.SearchTermToString16(t_url,
        to_wide_cases[i].encoded_search_term);

    EXPECT_EQ(to_wide_cases[i].expected_decoded_term, result);
  }
}

TEST_F(TemplateURLTest, SetFavicon) {
  TemplateURL url;
  GURL favicon_url("http://favicon.url");
  url.SetFaviconURL(favicon_url);
  ASSERT_EQ(1U, url.image_refs().size());
  ASSERT_TRUE(favicon_url == url.GetFavIconURL());

  GURL favicon_url2("http://favicon2.url");
  url.SetFaviconURL(favicon_url2);
  ASSERT_EQ(1U, url.image_refs().size());
  ASSERT_TRUE(favicon_url2 == url.GetFavIconURL());
}

TEST_F(TemplateURLTest, DisplayURLToURLRef) {
  struct TestData {
    const std::string url;
    const string16 expected_result;
  } data[] = {
    { "http://foo{searchTerms}x{inputEncoding}y{outputEncoding}a",
      ASCIIToUTF16("http://foo%sx{inputEncoding}y{outputEncoding}a") },
    { "http://X",
      ASCIIToUTF16("http://X") },
    { "http://foo{searchTerms",
      ASCIIToUTF16("http://foo{searchTerms") },
    { "http://foo{searchTerms}{language}",
      ASCIIToUTF16("http://foo%s{language}") },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(data); ++i) {
    TemplateURLRef ref(data[i].url, 1, 2);
    EXPECT_EQ(data[i].expected_result, ref.DisplayURL());
    EXPECT_EQ(data[i].url,
              TemplateURLRef::DisplayURLToURLRef(ref.DisplayURL()));
  }
}

TEST_F(TemplateURLTest, ReplaceSearchTerms) {
  struct TestData {
    const std::string url;
    const std::string expected_result;
  } data[] = {
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
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(data); ++i) {
    TemplateURLRef ref(data[i].url, 1, 2);
    EXPECT_TRUE(ref.IsValid());
    EXPECT_TRUE(ref.SupportsReplacement());
    std::string expected_result = data[i].expected_result;
    ReplaceSubstringsAfterOffset(&expected_result, 0, "{language}",
        g_browser_process->GetApplicationLocale());
    GURL result = GURL(ref.ReplaceSearchTerms(turl, ASCIIToUTF16("X"),
        TemplateURLRef::NO_SUGGESTIONS_AVAILABLE, string16()));
    EXPECT_TRUE(result.is_valid());
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
  } data[] = {
    { "BIG5",  WideToUTF16(L"\x60BD"),
      "http://foo/?{searchTerms}{inputEncoding}",
      "http://foo/?%B1~BIG5" },
    { "UTF-8", ASCIIToUTF16("blah"),
      "http://foo/?{searchTerms}{inputEncoding}",
      "http://foo/?blahUTF-8" },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(data); ++i) {
    TemplateURL turl;
    turl.add_input_encoding(data[i].encoding);
    TemplateURLRef ref(data[i].url, 1, 2);
    GURL result = GURL(ref.ReplaceSearchTerms(turl,
        data[i].search_term, TemplateURLRef::NO_SUGGESTIONS_AVAILABLE,
        string16()));
    EXPECT_TRUE(result.is_valid());
    EXPECT_EQ(data[i].expected_result, result.spec());
  }
}

TEST_F(TemplateURLTest, Suggestions) {
  struct TestData {
    const int accepted_suggestion;
    const string16 original_query_for_suggestion;
    const std::string expected_result;
  } data[] = {
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
  TemplateURLRef ref("http://bar/foo?{google:acceptedSuggestion}"
      "{google:originalQueryForSuggestion}q={searchTerms}", 1, 2);
  ASSERT_TRUE(ref.IsValid());
  ASSERT_TRUE(ref.SupportsReplacement());
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(data); ++i) {
    GURL result = GURL(ref.ReplaceSearchTerms(turl, ASCIIToUTF16("foobar"),
        data[i].accepted_suggestion, data[i].original_query_for_suggestion));
    EXPECT_TRUE(result.is_valid());
    EXPECT_EQ(data[i].expected_result, result.spec());
  }
}

#if defined(OS_WIN)
TEST_F(TemplateURLTest, RLZ) {
  string16 rlz_string;
#if defined(GOOGLE_CHROME_BUILD)
  RLZTracker::GetAccessPointRlz(rlz_lib::CHROME_OMNIBOX, &rlz_string);
#endif

  TemplateURL t_url;
  TemplateURLRef ref("http://bar/?{google:RLZ}{searchTerms}", 1, 2);
  ASSERT_TRUE(ref.IsValid());
  ASSERT_TRUE(ref.SupportsReplacement());
  GURL result(ref.ReplaceSearchTerms(t_url, L"x",
      TemplateURLRef::NO_SUGGESTIONS_AVAILABLE, string16()));
  ASSERT_TRUE(result.is_valid());
  std::string expected_url = "http://bar/?";
  if (!rlz_string.empty()) {
    expected_url += "rlz=" + WideToUTF8(rlz_string) + "&";
  }
  expected_url += "x";
  ASSERT_EQ(expected_url, result.spec());
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

  TemplateURL t_url;
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(data); ++i) {
    t_url.SetURL(data[i].url, 0, 0);
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
    { "http://google.com/", "http://clients1.google.com/complete/", },
    { "http://www.google.com/", "http://clients1.google.com/complete/", },
    { "http://www.google.co.uk/", "http://clients1.google.co.uk/complete/", },
    { "http://www.google.com.by/",
      "http://clients1.google.com.by/complete/", },
    { "http://google.com/intl/xx/", "http://clients1.google.com/complete/", },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(data); ++i)
    CheckSuggestBaseURL(data[i].base_url, data[i].base_suggest_url);
}

TEST_F(TemplateURLTest, Keyword) {
  TemplateURL t_url;
  t_url.SetURL("http://www.google.com/search", 0, 0);
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
  TemplateURLRef url_ref(parsed_url, 0, 0);
  TemplateURLRef::Replacements replacements;
  EXPECT_TRUE(url_ref.ParseParameter(0, 12, &parsed_url, &replacements));
  EXPECT_EQ(std::string(), parsed_url);
  ASSERT_EQ(1U, replacements.size());
  EXPECT_EQ(static_cast<size_t>(0), replacements[0].index);
  EXPECT_EQ(TemplateURLRef::SEARCH_TERMS, replacements[0].type);
}

TEST_F(TemplateURLTest, ParseParameterUnknown) {
  std::string parsed_url("{}");
  TemplateURLRef url_ref(parsed_url, 0, 0);
  TemplateURLRef::Replacements replacements;
  EXPECT_FALSE(url_ref.ParseParameter(0, 1, &parsed_url, &replacements));
  EXPECT_EQ("{}", parsed_url);
  EXPECT_TRUE(replacements.empty());
}

TEST_F(TemplateURLTest, ParseURLEmpty) {
  TemplateURLRef url_ref("", 0, 0);
  TemplateURLRef::Replacements replacements;
  bool valid = false;
  EXPECT_EQ(std::string(), url_ref.ParseURL("", &replacements, &valid));
  EXPECT_TRUE(replacements.empty());
  EXPECT_TRUE(valid);
}

TEST_F(TemplateURLTest, ParseURLNoTemplateEnd) {
  TemplateURLRef url_ref("{", 0, 0);
  TemplateURLRef::Replacements replacements;
  bool valid = false;
  EXPECT_EQ(std::string(), url_ref.ParseURL("{", &replacements, &valid));
  EXPECT_TRUE(replacements.empty());
  EXPECT_FALSE(valid);
}

TEST_F(TemplateURLTest, ParseURLNoKnownParameters) {
  TemplateURLRef url_ref("{}", 0, 0);
  TemplateURLRef::Replacements replacements;
  bool valid = false;
  EXPECT_EQ("{}", url_ref.ParseURL("{}", &replacements, &valid));
  EXPECT_TRUE(replacements.empty());
  EXPECT_TRUE(valid);
}

TEST_F(TemplateURLTest, ParseURLTwoParameters) {
  TemplateURLRef url_ref("{}{{%s}}", 0, 0);
  TemplateURLRef::Replacements replacements;
  bool valid = false;
  EXPECT_EQ("{}{}",
            url_ref.ParseURL("{}{{searchTerms}}", &replacements, &valid));
  ASSERT_EQ(1U, replacements.size());
  EXPECT_EQ(static_cast<size_t>(3), replacements[0].index);
  EXPECT_EQ(TemplateURLRef::SEARCH_TERMS, replacements[0].type);
  EXPECT_TRUE(valid);
}

TEST_F(TemplateURLTest, ParseURLNestedParameter) {
  TemplateURLRef url_ref("{%s", 0, 0);
  TemplateURLRef::Replacements replacements;
  bool valid = false;
  EXPECT_EQ("{", url_ref.ParseURL("{{searchTerms}", &replacements, &valid));
  ASSERT_EQ(1U, replacements.size());
  EXPECT_EQ(static_cast<size_t>(1), replacements[0].index);
  EXPECT_EQ(TemplateURLRef::SEARCH_TERMS, replacements[0].type);
  EXPECT_TRUE(valid);
}
