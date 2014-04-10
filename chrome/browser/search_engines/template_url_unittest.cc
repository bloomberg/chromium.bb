// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/rlz/rlz.h"
#include "chrome/browser/search_engines/search_terms_data.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(ENABLE_RLZ)
#include "chrome/browser/google/google_util.h"
#endif

#if defined(OS_ANDROID)
#include "chrome/browser/search_engines/search_terms_data_android.h"
#endif

using base::ASCIIToUTF16;

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
    : google_base_url_(google_base_url) {
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
  TemplateURLData data;
  EXPECT_FALSE(data.show_in_default_list);
  EXPECT_FALSE(data.safe_for_autoreplace);
  EXPECT_EQ(0, data.prepopulate_id);
}

TEST_F(TemplateURLTest, TestValidWithComplete) {
  TemplateURLData data;
  data.SetURL("{searchTerms}");
  TemplateURL url(NULL, data);
  EXPECT_TRUE(url.url_ref().IsValid());
}

TEST_F(TemplateURLTest, URLRefTestSearchTerms) {
  struct SearchTermsCase {
    const char* url;
    const base::string16 terms;
    const std::string output;
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
    TemplateURLData data;
    data.SetURL(value.url);
    TemplateURL url(NULL, data);
    EXPECT_TRUE(url.url_ref().IsValid());
    ASSERT_TRUE(url.url_ref().SupportsReplacement());
    GURL result(url.url_ref().ReplaceSearchTerms(
        TemplateURLRef::SearchTermsArgs(value.terms)));
    ASSERT_TRUE(result.is_valid());
    EXPECT_EQ(value.output, result.spec());
  }
}

TEST_F(TemplateURLTest, URLRefTestCount) {
  TemplateURLData data;
  data.SetURL("http://foo{searchTerms}{count?}");
  TemplateURL url(NULL, data);
  EXPECT_TRUE(url.url_ref().IsValid());
  ASSERT_TRUE(url.url_ref().SupportsReplacement());
  GURL result(url.url_ref().ReplaceSearchTerms(
      TemplateURLRef::SearchTermsArgs(ASCIIToUTF16("X"))));
  ASSERT_TRUE(result.is_valid());
  EXPECT_EQ("http://foox/", result.spec());
}

TEST_F(TemplateURLTest, URLRefTestCount2) {
  TemplateURLData data;
  data.SetURL("http://foo{searchTerms}{count}");
  TemplateURL url(NULL, data);
  EXPECT_TRUE(url.url_ref().IsValid());
  ASSERT_TRUE(url.url_ref().SupportsReplacement());
  GURL result(url.url_ref().ReplaceSearchTerms(
      TemplateURLRef::SearchTermsArgs(ASCIIToUTF16("X"))));
  ASSERT_TRUE(result.is_valid());
  EXPECT_EQ("http://foox10/", result.spec());
}

TEST_F(TemplateURLTest, URLRefTestIndices) {
  TemplateURLData data;
  data.SetURL("http://foo{searchTerms}x{startIndex?}y{startPage?}");
  TemplateURL url(NULL, data);
  EXPECT_TRUE(url.url_ref().IsValid());
  ASSERT_TRUE(url.url_ref().SupportsReplacement());
  GURL result(url.url_ref().ReplaceSearchTerms(
      TemplateURLRef::SearchTermsArgs(ASCIIToUTF16("X"))));
  ASSERT_TRUE(result.is_valid());
  EXPECT_EQ("http://fooxxy/", result.spec());
}

TEST_F(TemplateURLTest, URLRefTestIndices2) {
  TemplateURLData data;
  data.SetURL("http://foo{searchTerms}x{startIndex}y{startPage}");
  TemplateURL url(NULL, data);
  EXPECT_TRUE(url.url_ref().IsValid());
  ASSERT_TRUE(url.url_ref().SupportsReplacement());
  GURL result(url.url_ref().ReplaceSearchTerms(
      TemplateURLRef::SearchTermsArgs(ASCIIToUTF16("X"))));
  ASSERT_TRUE(result.is_valid());
  EXPECT_EQ("http://fooxx1y1/", result.spec());
}

TEST_F(TemplateURLTest, URLRefTestEncoding) {
  TemplateURLData data;
  data.SetURL("http://foo{searchTerms}x{inputEncoding?}y{outputEncoding?}a");
  TemplateURL url(NULL, data);
  EXPECT_TRUE(url.url_ref().IsValid());
  ASSERT_TRUE(url.url_ref().SupportsReplacement());
  GURL result(url.url_ref().ReplaceSearchTerms(
      TemplateURLRef::SearchTermsArgs(ASCIIToUTF16("X"))));
  ASSERT_TRUE(result.is_valid());
  EXPECT_EQ("http://fooxxutf-8ya/", result.spec());
}

TEST_F(TemplateURLTest, URLRefTestImageURLWithPOST) {
  const char kInvalidPostParamsString[] =
      "unknown_template={UnknownTemplate},bad_value=bad{value},"
      "{google:sbiSource}";
  // List all accpectable parameter format in valid_post_params_string. it is
  // expected like: "name0=,name1=value1,name2={template1}"
  const char kValidPostParamsString[] =
      "image_content={google:imageThumbnail},image_url={google:imageURL},"
      "sbisrc={google:imageSearchSource},language={language},empty_param=,"
      "constant_param=constant,width={google:imageOriginalWidth}";
  const char KImageSearchURL[] = "http://foo.com/sbi";

  TemplateURLData data;
  data.image_url = KImageSearchURL;

  // Try to parse invalid post parameters.
  data.image_url_post_params = kInvalidPostParamsString;
  TemplateURL url_bad(NULL, data);
  ASSERT_FALSE(url_bad.image_url_ref().IsValid());
  const TemplateURLRef::PostParams& bad_post_params =
      url_bad.image_url_ref().post_params_;
  ASSERT_EQ(2U, bad_post_params.size());
  EXPECT_EQ("unknown_template", bad_post_params[0].first);
  EXPECT_EQ("{UnknownTemplate}", bad_post_params[0].second);
  EXPECT_EQ("bad_value", bad_post_params[1].first);
  EXPECT_EQ("bad{value}", bad_post_params[1].second);

  // Try to parse valid post parameters.
  data.image_url_post_params = kValidPostParamsString;
  TemplateURL url(NULL, data);
  ASSERT_TRUE(url.image_url_ref().IsValid());
  ASSERT_FALSE(url.image_url_ref().SupportsReplacement());

  // Check term replacement.
  TemplateURLRef::SearchTermsArgs search_args(ASCIIToUTF16("X"));
  search_args.image_thumbnail_content = "dummy-image-thumbnail";
  search_args.image_url = GURL("http://dummyimage.com/dummy.jpg");
  search_args.image_original_size = gfx::Size(10, 10);
  // Replacement operation with no post_data buffer should still return
  // the parsed URL.
  GURL result(url.image_url_ref().ReplaceSearchTerms(search_args));
  ASSERT_TRUE(result.is_valid());
  EXPECT_EQ(KImageSearchURL, result.spec());
  TemplateURLRef::PostContent post_content;
  TestSearchTermsData search_terms_data("http://X");
  result = GURL(url.image_url_ref().ReplaceSearchTermsUsingTermsData(
      search_args, search_terms_data, &post_content));
  ASSERT_TRUE(result.is_valid());
  EXPECT_EQ(KImageSearchURL, result.spec());
  ASSERT_FALSE(post_content.first.empty());
  ASSERT_FALSE(post_content.second.empty());

  // Check parsed result of post parameters.
  const TemplateURLRef::Replacements& replacements =
      url.image_url_ref().replacements_;
  const TemplateURLRef::PostParams& post_params =
      url.image_url_ref().post_params_;
  EXPECT_EQ(7U, post_params.size());
  for (TemplateURLRef::PostParams::const_iterator i = post_params.begin();
       i != post_params.end(); ++i) {
    TemplateURLRef::Replacements::const_iterator j = replacements.begin();
    for (; j != replacements.end(); ++j) {
      if (j->is_post_param && j->index ==
          static_cast<size_t>(i - post_params.begin())) {
        switch (j->type) {
          case TemplateURLRef::GOOGLE_IMAGE_ORIGINAL_WIDTH:
            EXPECT_EQ("width", i->first);
            EXPECT_EQ(
                base::IntToString(search_args.image_original_size.width()),
                i->second);
            break;
          case TemplateURLRef::GOOGLE_IMAGE_THUMBNAIL:
            EXPECT_EQ("image_content", i->first);
            EXPECT_EQ(search_args.image_thumbnail_content, i->second);
            break;
          case TemplateURLRef::GOOGLE_IMAGE_URL:
            EXPECT_EQ("image_url", i->first);
            EXPECT_EQ(search_args.image_url.spec(), i->second);
            break;
          case TemplateURLRef::LANGUAGE:
            EXPECT_EQ("language", i->first);
            EXPECT_EQ("en", i->second);
            break;
          default:
            ADD_FAILURE();  // Should never go here.
        }
        break;
      }
    }
    if (j != replacements.end())
      continue;
    if (i->first == "empty_param") {
      EXPECT_TRUE(i->second.empty());
    } else if (i->first == "sbisrc") {
      EXPECT_FALSE(i->second.empty());
    } else {
      EXPECT_EQ("constant_param", i->first);
      EXPECT_EQ("constant", i->second);
    }
  }
}

// Test that setting the prepopulate ID from TemplateURL causes the stored
// TemplateURLRef to handle parsing the URL parameters differently.
TEST_F(TemplateURLTest, SetPrepopulatedAndParse) {
  TemplateURLData data;
  data.SetURL("http://foo{fhqwhgads}bar");
  TemplateURL url(NULL, data);
  TemplateURLRef::Replacements replacements;
  bool valid = false;
  EXPECT_EQ("http://foo{fhqwhgads}bar", url.url_ref().ParseURL(
      "http://foo{fhqwhgads}bar", &replacements, NULL, &valid));
  EXPECT_TRUE(replacements.empty());
  EXPECT_TRUE(valid);

  data.prepopulate_id = 123;
  TemplateURL url2(NULL, data);
  EXPECT_EQ("http://foobar", url2.url_ref().ParseURL("http://foo{fhqwhgads}bar",
                                                     &replacements, NULL,
                                                     &valid));
  EXPECT_TRUE(replacements.empty());
  EXPECT_TRUE(valid);
}

TEST_F(TemplateURLTest, InputEncodingBeforeSearchTerm) {
  TemplateURLData data;
  data.SetURL("http://foox{inputEncoding?}a{searchTerms}y{outputEncoding?}b");
  TemplateURL url(NULL, data);
  EXPECT_TRUE(url.url_ref().IsValid());
  ASSERT_TRUE(url.url_ref().SupportsReplacement());
  GURL result(url.url_ref().ReplaceSearchTerms(
      TemplateURLRef::SearchTermsArgs(ASCIIToUTF16("X"))));
  ASSERT_TRUE(result.is_valid());
  EXPECT_EQ("http://fooxutf-8axyb/", result.spec());
}

TEST_F(TemplateURLTest, URLRefTestEncoding2) {
  TemplateURLData data;
  data.SetURL("http://foo{searchTerms}x{inputEncoding}y{outputEncoding}a");
  TemplateURL url(NULL, data);
  EXPECT_TRUE(url.url_ref().IsValid());
  ASSERT_TRUE(url.url_ref().SupportsReplacement());
  GURL result(url.url_ref().ReplaceSearchTerms(
      TemplateURLRef::SearchTermsArgs(ASCIIToUTF16("X"))));
  ASSERT_TRUE(result.is_valid());
  EXPECT_EQ("http://fooxxutf-8yutf-8a/", result.spec());
}

TEST_F(TemplateURLTest, URLRefTestSearchTermsUsingTermsData) {
  struct SearchTermsCase {
    const char* url;
    const base::string16 terms;
    const char* output;
  } search_term_cases[] = {
    { "{google:baseURL}{language}{searchTerms}", base::string16(),
      "http://example.com/e/en" },
    { "{google:baseSuggestURL}{searchTerms}", base::string16(),
      "http://example.com/complete/" }
  };

  TestSearchTermsData search_terms_data("http://example.com/e/");
  TemplateURLData data;
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(search_term_cases); ++i) {
    const SearchTermsCase& value = search_term_cases[i];
    data.SetURL(value.url);
    TemplateURL url(NULL, data);
    EXPECT_TRUE(url.url_ref().IsValid());
    ASSERT_TRUE(url.url_ref().SupportsReplacement());
    GURL result(url.url_ref().ReplaceSearchTermsUsingTermsData(
        TemplateURLRef::SearchTermsArgs(value.terms), search_terms_data, NULL));
    ASSERT_TRUE(result.is_valid());
    EXPECT_EQ(value.output, result.spec());
  }
}

TEST_F(TemplateURLTest, URLRefTermToWide) {
  struct ToWideCase {
    const char* encoded_search_term;
    const base::string16 expected_decoded_term;
  } to_wide_cases[] = {
    {"hello+world", ASCIIToUTF16("hello world")},
    // Test some big-5 input.
    {"%a7A%A6%6e+to+you", base::WideToUTF16(L"\x4f60\x597d to you")},
    // Test some UTF-8 input. We should fall back to this when the encoding
    // doesn't look like big-5. We have a '5' in the middle, which is an invalid
    // Big-5 trailing byte.
    {"%e4%bd%a05%e5%a5%bd+to+you",
        base::WideToUTF16(L"\x4f60\x35\x597d to you")},
    // Undecodable input should stay escaped.
    {"%91%01+abcd", base::WideToUTF16(L"%91%01 abcd")},
    // Make sure we convert %2B to +.
    {"C%2B%2B", ASCIIToUTF16("C++")},
    // C%2B is escaped as C%252B, make sure we unescape it properly.
    {"C%252B", ASCIIToUTF16("C%2B")},
  };

  // Set one input encoding: big-5. This is so we can test fallback to UTF-8.
  TemplateURLData data;
  data.SetURL("http://foo?q={searchTerms}");
  data.input_encodings.push_back("big-5");
  TemplateURL url(NULL, data);
  EXPECT_TRUE(url.url_ref().IsValid());
  ASSERT_TRUE(url.url_ref().SupportsReplacement());
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(to_wide_cases); i++) {
    EXPECT_EQ(to_wide_cases[i].expected_decoded_term,
              url.url_ref().SearchTermToString16(
                  to_wide_cases[i].encoded_search_term));
  }
}

TEST_F(TemplateURLTest, DisplayURLToURLRef) {
  struct TestData {
    const std::string url;
    const base::string16 expected_result;
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
  TemplateURLData data;
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_data); ++i) {
    data.SetURL(test_data[i].url);
    TemplateURL url(NULL, data);
    EXPECT_EQ(test_data[i].expected_result, url.url_ref().DisplayURL());
    EXPECT_EQ(test_data[i].url,
              TemplateURLRef::DisplayURLToURLRef(url.url_ref().DisplayURL()));
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
  TemplateURLData data;
  data.input_encodings.push_back("UTF-8");
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_data); ++i) {
    data.SetURL(test_data[i].url);
    TemplateURL url(NULL, data);
    EXPECT_TRUE(url.url_ref().IsValid());
    ASSERT_TRUE(url.url_ref().SupportsReplacement());
    std::string expected_result = test_data[i].expected_result;
    ReplaceSubstringsAfterOffset(&expected_result, 0, "{language}",
        g_browser_process->GetApplicationLocale());
    GURL result(url.url_ref().ReplaceSearchTerms(
        TemplateURLRef::SearchTermsArgs(ASCIIToUTF16("X"))));
    ASSERT_TRUE(result.is_valid());
    EXPECT_EQ(expected_result, result.spec());
  }
}


// Tests replacing search terms in various encodings and making sure the
// generated URL matches the expected value.
TEST_F(TemplateURLTest, ReplaceArbitrarySearchTerms) {
  struct TestData {
    const std::string encoding;
    const base::string16 search_term;
    const std::string url;
    const std::string expected_result;
  } test_data[] = {
    { "BIG5",  base::WideToUTF16(L"\x60BD"),
      "http://foo/?{searchTerms}{inputEncoding}",
      "http://foo/?%B1~BIG5" },
    { "UTF-8", ASCIIToUTF16("blah"),
      "http://foo/?{searchTerms}{inputEncoding}",
      "http://foo/?blahUTF-8" },
    { "Shift_JIS", base::UTF8ToUTF16("\xe3\x81\x82"),
      "http://foo/{searchTerms}/bar",
      "http://foo/%82%A0/bar"},
    { "Shift_JIS", base::UTF8ToUTF16("\xe3\x81\x82 \xe3\x81\x84"),
      "http://foo/{searchTerms}/bar",
      "http://foo/%82%A0%20%82%A2/bar"},
  };
  TemplateURLData data;
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_data); ++i) {
    data.SetURL(test_data[i].url);
    data.input_encodings.clear();
    data.input_encodings.push_back(test_data[i].encoding);
    TemplateURL url(NULL, data);
    EXPECT_TRUE(url.url_ref().IsValid());
    ASSERT_TRUE(url.url_ref().SupportsReplacement());
    GURL result(url.url_ref().ReplaceSearchTerms(
        TemplateURLRef::SearchTermsArgs(test_data[i].search_term)));
    ASSERT_TRUE(result.is_valid());
    EXPECT_EQ(test_data[i].expected_result, result.spec());
  }
}

// Tests replacing assisted query stats (AQS) in various scenarios.
TEST_F(TemplateURLTest, ReplaceAssistedQueryStats) {
  struct TestData {
    const base::string16 search_term;
    const std::string aqs;
    const std::string base_url;
    const std::string url;
    const std::string expected_result;
  } test_data[] = {
    // No HTTPS, no AQS.
    { ASCIIToUTF16("foo"),
      "chrome.0.0l6",
      "http://foo/",
      "{google:baseURL}?{searchTerms}{google:assistedQueryStats}",
      "http://foo/?foo" },
    // HTTPS available, AQS should be replaced.
    { ASCIIToUTF16("foo"),
      "chrome.0.0l6",
      "https://foo/",
      "{google:baseURL}?{searchTerms}{google:assistedQueryStats}",
      "https://foo/?fooaqs=chrome.0.0l6&" },
    // HTTPS available, however AQS is empty.
    { ASCIIToUTF16("foo"),
      "",
      "https://foo/",
      "{google:baseURL}?{searchTerms}{google:assistedQueryStats}",
      "https://foo/?foo" },
    // No {google:baseURL} and protocol is HTTP, we must not substitute AQS.
    { ASCIIToUTF16("foo"),
      "chrome.0.0l6",
      "",
      "http://foo?{searchTerms}{google:assistedQueryStats}",
      "http://foo/?foo" },
    // A non-Google search provider with HTTPS should allow AQS.
    { ASCIIToUTF16("foo"),
      "chrome.0.0l6",
      "",
      "https://foo?{searchTerms}{google:assistedQueryStats}",
      "https://foo/?fooaqs=chrome.0.0l6&" },
  };
  TemplateURLData data;
  data.input_encodings.push_back("UTF-8");
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_data); ++i) {
    data.SetURL(test_data[i].url);
    TemplateURL url(NULL, data);
    EXPECT_TRUE(url.url_ref().IsValid());
    ASSERT_TRUE(url.url_ref().SupportsReplacement());
    TemplateURLRef::SearchTermsArgs search_terms_args(test_data[i].search_term);
    search_terms_args.assisted_query_stats = test_data[i].aqs;
    UIThreadSearchTermsData::SetGoogleBaseURL(test_data[i].base_url);
    GURL result(url.url_ref().ReplaceSearchTerms(search_terms_args));
    ASSERT_TRUE(result.is_valid());
    EXPECT_EQ(test_data[i].expected_result, result.spec());
  }
}

// Tests replacing cursor position.
TEST_F(TemplateURLTest, ReplaceCursorPosition) {
  struct TestData {
    const base::string16 search_term;
    size_t cursor_position;
    const std::string url;
    const std::string expected_result;
  } test_data[] = {
    { ASCIIToUTF16("foo"),
      base::string16::npos,
      "{google:baseURL}?{searchTerms}&{google:cursorPosition}",
      "http://www.google.com/?foo&" },
    { ASCIIToUTF16("foo"),
      2,
      "{google:baseURL}?{searchTerms}&{google:cursorPosition}",
      "http://www.google.com/?foo&cp=2&" },
    { ASCIIToUTF16("foo"),
      15,
      "{google:baseURL}?{searchTerms}&{google:cursorPosition}",
      "http://www.google.com/?foo&cp=15&" },
  };
  TemplateURLData data;
  data.input_encodings.push_back("UTF-8");
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_data); ++i) {
    data.SetURL(test_data[i].url);
    TemplateURL url(NULL, data);
    EXPECT_TRUE(url.url_ref().IsValid());
    ASSERT_TRUE(url.url_ref().SupportsReplacement());
    TemplateURLRef::SearchTermsArgs search_terms_args(test_data[i].search_term);
    search_terms_args.cursor_position = test_data[i].cursor_position;
    GURL result(url.url_ref().ReplaceSearchTerms(search_terms_args));
    ASSERT_TRUE(result.is_valid());
    EXPECT_EQ(test_data[i].expected_result, result.spec());
  }
}

// Tests replacing currentPageUrl.
TEST_F(TemplateURLTest, ReplaceCurrentPageUrl) {
  struct TestData {
    const base::string16 search_term;
    const std::string current_page_url;
    const std::string url;
    const std::string expected_result;
  } test_data[] = {
    { ASCIIToUTF16("foo"),
      "http://www.google.com/",
      "{google:baseURL}?{searchTerms}&{google:currentPageUrl}",
      "http://www.google.com/?foo&url=http%3A%2F%2Fwww.google.com%2F&" },
    { ASCIIToUTF16("foo"),
      "",
      "{google:baseURL}?{searchTerms}&{google:currentPageUrl}",
      "http://www.google.com/?foo&" },
    { ASCIIToUTF16("foo"),
      "http://g.com/+-/*&=",
      "{google:baseURL}?{searchTerms}&{google:currentPageUrl}",
      "http://www.google.com/?foo&url=http%3A%2F%2Fg.com%2F%2B-%2F*%26%3D&" },
  };
  TemplateURLData data;
  data.input_encodings.push_back("UTF-8");
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_data); ++i) {
    data.SetURL(test_data[i].url);
    TemplateURL url(NULL, data);
    EXPECT_TRUE(url.url_ref().IsValid());
    ASSERT_TRUE(url.url_ref().SupportsReplacement());
    TemplateURLRef::SearchTermsArgs search_terms_args(test_data[i].search_term);
    search_terms_args.current_page_url = test_data[i].current_page_url;
    GURL result(url.url_ref().ReplaceSearchTerms(search_terms_args));
    ASSERT_TRUE(result.is_valid());
    EXPECT_EQ(test_data[i].expected_result, result.spec());
  }
}

TEST_F(TemplateURLTest, Suggestions) {
  struct TestData {
    const int accepted_suggestion;
    const base::string16 original_query_for_suggestion;
    const std::string expected_result;
  } test_data[] = {
    { TemplateURLRef::NO_SUGGESTIONS_AVAILABLE, base::string16(),
      "http://bar/foo?q=foobar" },
    { TemplateURLRef::NO_SUGGESTIONS_AVAILABLE, ASCIIToUTF16("foo"),
      "http://bar/foo?q=foobar" },
    { TemplateURLRef::NO_SUGGESTION_CHOSEN, base::string16(),
      "http://bar/foo?q=foobar" },
    { TemplateURLRef::NO_SUGGESTION_CHOSEN, ASCIIToUTF16("foo"),
      "http://bar/foo?q=foobar" },
    { 0, base::string16(), "http://bar/foo?oq=&q=foobar" },
    { 1, ASCIIToUTF16("foo"), "http://bar/foo?oq=foo&q=foobar" },
  };
  TemplateURLData data;
  data.SetURL("http://bar/foo?{google:originalQueryForSuggestion}"
              "q={searchTerms}");
  data.input_encodings.push_back("UTF-8");
  TemplateURL url(NULL, data);
  EXPECT_TRUE(url.url_ref().IsValid());
  ASSERT_TRUE(url.url_ref().SupportsReplacement());
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_data); ++i) {
    TemplateURLRef::SearchTermsArgs search_terms_args(
        ASCIIToUTF16("foobar"));
    search_terms_args.accepted_suggestion = test_data[i].accepted_suggestion;
    search_terms_args.original_query =
        test_data[i].original_query_for_suggestion;
    GURL result(url.url_ref().ReplaceSearchTerms(search_terms_args));
    ASSERT_TRUE(result.is_valid());
    EXPECT_EQ(test_data[i].expected_result, result.spec());
  }
}

TEST_F(TemplateURLTest, RLZ) {
  base::string16 rlz_string;
#if defined(ENABLE_RLZ)
  std::string brand;
  if (google_util::GetBrand(&brand) && !brand.empty() &&
      !google_util::IsOrganic(brand)) {
    RLZTracker::GetAccessPointRlz(RLZTracker::CHROME_OMNIBOX, &rlz_string);
  }
#elif defined(OS_ANDROID)
  SearchTermsDataAndroid::rlz_parameter_value_.Get() =
      ASCIIToUTF16("android_test");
  rlz_string = SearchTermsDataAndroid::rlz_parameter_value_.Get();
#endif

  TemplateURLData data;
  data.SetURL("http://bar/?{google:RLZ}{searchTerms}");
  TemplateURL url(NULL, data);
  EXPECT_TRUE(url.url_ref().IsValid());
  ASSERT_TRUE(url.url_ref().SupportsReplacement());
  GURL result(url.url_ref().ReplaceSearchTerms(
      TemplateURLRef::SearchTermsArgs(ASCIIToUTF16("x"))));
  ASSERT_TRUE(result.is_valid());
  std::string expected_url = "http://bar/?";
  if (!rlz_string.empty())
    expected_url += "rlz=" + base::UTF16ToUTF8(rlz_string) + "&";
  expected_url += "x";
  EXPECT_EQ(expected_url, result.spec());
}

#if !defined(OS_ANDROID) && !defined(OS_IOS)
TEST_F(TemplateURLTest, RLZFromAppList) {
  base::string16 rlz_string;
#if defined(ENABLE_RLZ)
  std::string brand;
  if (google_util::GetBrand(&brand) && !brand.empty() &&
      !google_util::IsOrganic(brand)) {
    RLZTracker::GetAccessPointRlz(RLZTracker::CHROME_APP_LIST, &rlz_string);
  }
#endif

  TemplateURLData data;
  data.SetURL("http://bar/?{google:RLZ}{searchTerms}");
  TemplateURL url(NULL, data);
  EXPECT_TRUE(url.url_ref().IsValid());
  ASSERT_TRUE(url.url_ref().SupportsReplacement());
  TemplateURLRef::SearchTermsArgs args(ASCIIToUTF16("x"));
  args.from_app_list = true;
  GURL result(url.url_ref().ReplaceSearchTerms(args));
  ASSERT_TRUE(result.is_valid());
  std::string expected_url = "http://bar/?";
  if (!rlz_string.empty())
    expected_url += "rlz=" + base::UTF16ToUTF8(rlz_string) + "&";
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
  } test_data[] = {
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
    { "https://blah/?q={searchTerms}", "blah", "/", "q"},

    // Single term with extra chars in value should match.
    { "http://blah/?q=stock:{searchTerms}", "blah", "/", "q"},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_data); ++i) {
    TemplateURLData data;
    data.SetURL(test_data[i].url);
    TemplateURL url(NULL, data);
    EXPECT_EQ(test_data[i].host, url.url_ref().GetHost());
    EXPECT_EQ(test_data[i].path, url.url_ref().GetPath());
    EXPECT_EQ(test_data[i].search_term_key, url.url_ref().GetSearchTermKey());
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

TEST_F(TemplateURLTest, ParseParameterKnown) {
  std::string parsed_url("{searchTerms}");
  TemplateURLData data;
  data.SetURL(parsed_url);
  TemplateURL url(NULL, data);
  TemplateURLRef::Replacements replacements;
  EXPECT_TRUE(url.url_ref().ParseParameter(0, 12, &parsed_url, &replacements));
  EXPECT_EQ(std::string(), parsed_url);
  ASSERT_EQ(1U, replacements.size());
  EXPECT_EQ(0U, replacements[0].index);
  EXPECT_EQ(TemplateURLRef::SEARCH_TERMS, replacements[0].type);
}

TEST_F(TemplateURLTest, ParseParameterUnknown) {
  std::string parsed_url("{fhqwhgads}abc");
  TemplateURLData data;
  data.SetURL(parsed_url);
  TemplateURL url(NULL, data);
  TemplateURLRef::Replacements replacements;

  // By default, TemplateURLRef should not consider itself prepopulated.
  // Therefore we should not replace the unknown parameter.
  EXPECT_FALSE(url.url_ref().ParseParameter(0, 10, &parsed_url, &replacements));
  EXPECT_EQ("{fhqwhgads}abc", parsed_url);
  EXPECT_TRUE(replacements.empty());

  // If the TemplateURLRef is prepopulated, we should remove unknown parameters.
  parsed_url = "{fhqwhgads}abc";
  data.prepopulate_id = 1;
  TemplateURL url2(NULL, data);
  EXPECT_TRUE(url2.url_ref().ParseParameter(0, 10, &parsed_url, &replacements));
  EXPECT_EQ("abc", parsed_url);
  EXPECT_TRUE(replacements.empty());
}

TEST_F(TemplateURLTest, ParseURLEmpty) {
  TemplateURL url(NULL, TemplateURLData());
  TemplateURLRef::Replacements replacements;
  bool valid = false;
  EXPECT_EQ(std::string(),
            url.url_ref().ParseURL(std::string(), &replacements, NULL, &valid));
  EXPECT_TRUE(replacements.empty());
  EXPECT_TRUE(valid);
}

TEST_F(TemplateURLTest, ParseURLNoTemplateEnd) {
  TemplateURLData data;
  data.SetURL("{");
  TemplateURL url(NULL, data);
  TemplateURLRef::Replacements replacements;
  bool valid = false;
  EXPECT_EQ(std::string(), url.url_ref().ParseURL("{", &replacements, NULL,
                                                  &valid));
  EXPECT_TRUE(replacements.empty());
  EXPECT_FALSE(valid);
}

TEST_F(TemplateURLTest, ParseURLNoKnownParameters) {
  TemplateURLData data;
  data.SetURL("{}");
  TemplateURL url(NULL, data);
  TemplateURLRef::Replacements replacements;
  bool valid = false;
  EXPECT_EQ("{}", url.url_ref().ParseURL("{}", &replacements, NULL, &valid));
  EXPECT_TRUE(replacements.empty());
  EXPECT_TRUE(valid);
}

TEST_F(TemplateURLTest, ParseURLTwoParameters) {
  TemplateURLData data;
  data.SetURL("{}{{%s}}");
  TemplateURL url(NULL, data);
  TemplateURLRef::Replacements replacements;
  bool valid = false;
  EXPECT_EQ("{}{}",
            url.url_ref().ParseURL("{}{{searchTerms}}", &replacements, NULL,
                                   &valid));
  ASSERT_EQ(1U, replacements.size());
  EXPECT_EQ(3U, replacements[0].index);
  EXPECT_EQ(TemplateURLRef::SEARCH_TERMS, replacements[0].type);
  EXPECT_TRUE(valid);
}

TEST_F(TemplateURLTest, ParseURLNestedParameter) {
  TemplateURLData data;
  data.SetURL("{%s");
  TemplateURL url(NULL, data);
  TemplateURLRef::Replacements replacements;
  bool valid = false;
  EXPECT_EQ("{",
            url.url_ref().ParseURL("{{searchTerms}", &replacements, NULL,
                                   &valid));
  ASSERT_EQ(1U, replacements.size());
  EXPECT_EQ(1U, replacements[0].index);
  EXPECT_EQ(TemplateURLRef::SEARCH_TERMS, replacements[0].type);
  EXPECT_TRUE(valid);
}

#if defined(OS_ANDROID)
TEST_F(TemplateURLTest, SearchClient) {
  const std::string base_url_str("http://google.com/?");
  const std::string terms_str("{searchTerms}&{google:searchClient}");
  const std::string full_url_str = base_url_str + terms_str;
  const base::string16 terms(ASCIIToUTF16(terms_str));
  UIThreadSearchTermsData::SetGoogleBaseURL(base_url_str);

  TemplateURLData data;
  data.SetURL(full_url_str);
  TemplateURL url(NULL, data);
  EXPECT_TRUE(url.url_ref().IsValid());
  ASSERT_TRUE(url.url_ref().SupportsReplacement());
  TemplateURLRef::SearchTermsArgs search_terms_args(ASCIIToUTF16("foobar"));

  // Check that the URL is correct when a client is not present.
  GURL result(url.url_ref().ReplaceSearchTerms(search_terms_args));
  ASSERT_TRUE(result.is_valid());
  EXPECT_EQ("http://google.com/?foobar&", result.spec());

  // Check that the URL is correct when a client is present.
  SearchTermsDataAndroid::search_client_.Get() = "android_test";
  GURL result_2(url.url_ref().ReplaceSearchTerms(search_terms_args));
  ASSERT_TRUE(result_2.is_valid());
  EXPECT_EQ("http://google.com/?foobar&client=android_test&", result_2.spec());
}
#endif

TEST_F(TemplateURLTest, GetURLNoInstantURL) {
  TemplateURLData data;
  data.SetURL("http://google.com/?q={searchTerms}");
  data.suggestions_url = "http://google.com/suggest?q={searchTerms}";
  data.alternate_urls.push_back("http://google.com/alt?q={searchTerms}");
  data.alternate_urls.push_back("{google:baseURL}/alt/#q={searchTerms}");
  TemplateURL url(NULL, data);
  ASSERT_EQ(3U, url.URLCount());
  EXPECT_EQ("http://google.com/alt?q={searchTerms}", url.GetURL(0));
  EXPECT_EQ("{google:baseURL}/alt/#q={searchTerms}", url.GetURL(1));
  EXPECT_EQ("http://google.com/?q={searchTerms}", url.GetURL(2));
}

TEST_F(TemplateURLTest, GetURLNoSuggestionsURL) {
  TemplateURLData data;
  data.SetURL("http://google.com/?q={searchTerms}");
  data.instant_url = "http://google.com/instant#q={searchTerms}";
  data.alternate_urls.push_back("http://google.com/alt?q={searchTerms}");
  data.alternate_urls.push_back("{google:baseURL}/alt/#q={searchTerms}");
  TemplateURL url(NULL, data);
  ASSERT_EQ(3U, url.URLCount());
  EXPECT_EQ("http://google.com/alt?q={searchTerms}", url.GetURL(0));
  EXPECT_EQ("{google:baseURL}/alt/#q={searchTerms}", url.GetURL(1));
  EXPECT_EQ("http://google.com/?q={searchTerms}", url.GetURL(2));
}

TEST_F(TemplateURLTest, GetURLOnlyOneURL) {
  TemplateURLData data;
  data.SetURL("http://www.google.co.uk/");
  TemplateURL url(NULL, data);
  ASSERT_EQ(1U, url.URLCount());
  EXPECT_EQ("http://www.google.co.uk/", url.GetURL(0));
}

TEST_F(TemplateURLTest, ExtractSearchTermsFromURL) {
  TemplateURLData data;
  data.SetURL("http://google.com/?q={searchTerms}");
  data.instant_url = "http://google.com/instant#q={searchTerms}";
  data.alternate_urls.push_back("http://google.com/alt/#q={searchTerms}");
  data.alternate_urls.push_back(
      "http://google.com/alt/?ext=foo&q={searchTerms}#ref=bar");
  TemplateURL url(NULL, data);
  base::string16 result;

  EXPECT_TRUE(url.ExtractSearchTermsFromURL(
      GURL("http://google.com/?q=something"), &result));
  EXPECT_EQ(ASCIIToUTF16("something"), result);

  EXPECT_TRUE(url.ExtractSearchTermsFromURL(
      GURL("http://google.com/?espv&q=something"), &result));
  EXPECT_EQ(ASCIIToUTF16("something"), result);

  EXPECT_TRUE(url.ExtractSearchTermsFromURL(
      GURL("http://google.com/?espv=1&q=something"), &result));
  EXPECT_EQ(ASCIIToUTF16("something"), result);

  EXPECT_TRUE(url.ExtractSearchTermsFromURL(
      GURL("http://google.com/?espv=0&q=something"), &result));
  EXPECT_EQ(ASCIIToUTF16("something"), result);

  EXPECT_TRUE(url.ExtractSearchTermsFromURL(
      GURL("http://google.com/alt/#q=something"), &result));
  EXPECT_EQ(ASCIIToUTF16("something"), result);

  EXPECT_TRUE(url.ExtractSearchTermsFromURL(
      GURL("http://google.com/alt/#espv&q=something"), &result));
  EXPECT_EQ(ASCIIToUTF16("something"), result);

  EXPECT_TRUE(url.ExtractSearchTermsFromURL(
      GURL("http://google.com/alt/#espv=1&q=something"), &result));
  EXPECT_EQ(ASCIIToUTF16("something"), result);

  EXPECT_TRUE(url.ExtractSearchTermsFromURL(
      GURL("http://google.com/alt/#espv=0&q=something"), &result));
  EXPECT_EQ(ASCIIToUTF16("something"), result);

  EXPECT_FALSE(url.ExtractSearchTermsFromURL(
      GURL("http://google.ca/?q=something"), &result));
  EXPECT_EQ(base::string16(), result);

  EXPECT_FALSE(url.ExtractSearchTermsFromURL(
      GURL("http://google.ca/?q=something&q=anything"), &result));
  EXPECT_EQ(base::string16(), result);

  EXPECT_FALSE(url.ExtractSearchTermsFromURL(
      GURL("http://google.com/foo/?q=foo"), &result));
  EXPECT_EQ(base::string16(), result);

  EXPECT_TRUE(url.ExtractSearchTermsFromURL(
      GURL("https://google.com/?q=foo"), &result));
  EXPECT_EQ(ASCIIToUTF16("foo"), result);

  EXPECT_FALSE(url.ExtractSearchTermsFromURL(
      GURL("http://google.com:8080/?q=foo"), &result));
  EXPECT_EQ(base::string16(), result);

  EXPECT_TRUE(url.ExtractSearchTermsFromURL(
      GURL("http://google.com/?q=1+2+3&b=456"), &result));
  EXPECT_EQ(ASCIIToUTF16("1 2 3"), result);

  EXPECT_TRUE(url.ExtractSearchTermsFromURL(
      GURL("http://google.com/alt/?q=123#q=456"), &result));
  EXPECT_EQ(ASCIIToUTF16("456"), result);

  EXPECT_TRUE(url.ExtractSearchTermsFromURL(
      GURL("http://google.com/alt/?a=012&q=123&b=456#f=789"), &result));
  EXPECT_EQ(ASCIIToUTF16("123"), result);

  EXPECT_TRUE(url.ExtractSearchTermsFromURL(GURL(
      "http://google.com/alt/?a=012&q=123&b=456#j=abc&q=789&h=def9"), &result));
  EXPECT_EQ(ASCIIToUTF16("789"), result);

  EXPECT_FALSE(url.ExtractSearchTermsFromURL(
      GURL("http://google.com/alt/?q="), &result));
  EXPECT_EQ(base::string16(), result);

  EXPECT_FALSE(url.ExtractSearchTermsFromURL(
      GURL("http://google.com/alt/?#q="), &result));
  EXPECT_EQ(base::string16(), result);

  EXPECT_FALSE(url.ExtractSearchTermsFromURL(
      GURL("http://google.com/alt/?q=#q="), &result));
  EXPECT_EQ(base::string16(), result);

  EXPECT_FALSE(url.ExtractSearchTermsFromURL(
      GURL("http://google.com/alt/?q=123#q="), &result));
  EXPECT_EQ(base::string16(), result);

  EXPECT_TRUE(url.ExtractSearchTermsFromURL(
      GURL("http://google.com/alt/?q=#q=123"), &result));
  EXPECT_EQ(ASCIIToUTF16("123"), result);
}

TEST_F(TemplateURLTest, HasSearchTermsReplacementKey) {
  TemplateURLData data;
  data.SetURL("http://google.com/?q={searchTerms}");
  data.instant_url = "http://google.com/instant#q={searchTerms}";
  data.alternate_urls.push_back("http://google.com/alt/#q={searchTerms}");
  data.alternate_urls.push_back(
      "http://google.com/alt/?ext=foo&q={searchTerms}#ref=bar");
  data.search_terms_replacement_key = "espv";
  TemplateURL url(NULL, data);

  // Test with instant enabled required.
  EXPECT_FALSE(url.HasSearchTermsReplacementKey(
      GURL("http://google.com/")));

  EXPECT_TRUE(url.HasSearchTermsReplacementKey(
      GURL("http://google.com/?espv")));

  EXPECT_TRUE(url.HasSearchTermsReplacementKey(
      GURL("http://google.com/#espv")));

  EXPECT_FALSE(url.HasSearchTermsReplacementKey(
      GURL("http://google.com/?q=something")));

  EXPECT_TRUE(url.HasSearchTermsReplacementKey(
      GURL("http://google.com/?q=something&espv")));

  EXPECT_TRUE(url.HasSearchTermsReplacementKey(
      GURL("http://google.com/?q=something&espv=1")));

  EXPECT_TRUE(url.HasSearchTermsReplacementKey(
      GURL("http://google.com/?q=something&espv=0")));

  EXPECT_TRUE(url.HasSearchTermsReplacementKey(
      GURL("http://google.com/?espv&q=something")));

  EXPECT_TRUE(url.HasSearchTermsReplacementKey(
      GURL("http://google.com/?espv=1&q=something")));

  EXPECT_TRUE(url.HasSearchTermsReplacementKey(
      GURL("http://google.com/?espv=0&q=something")));

  EXPECT_FALSE(url.HasSearchTermsReplacementKey(
      GURL("http://google.com/alt/#q=something")));

  EXPECT_TRUE(url.HasSearchTermsReplacementKey(
      GURL("http://google.com/alt/#q=something&espv")));

  EXPECT_TRUE(url.HasSearchTermsReplacementKey(
      GURL("http://google.com/alt/#q=something&espv=1")));

  EXPECT_TRUE(url.HasSearchTermsReplacementKey(
      GURL("http://google.com/alt/#q=something&espv=0")));

  EXPECT_TRUE(url.HasSearchTermsReplacementKey(
      GURL("http://google.com/alt/#espv&q=something")));

  EXPECT_TRUE(url.HasSearchTermsReplacementKey(
      GURL("http://google.com/alt/#espv=1&q=something")));

  EXPECT_TRUE(url.HasSearchTermsReplacementKey(
      GURL("http://google.com/alt/#espv=0&q=something")));

  EXPECT_TRUE(url.HasSearchTermsReplacementKey(
      GURL("http://google.com/?espv#q=something")));

  EXPECT_TRUE(url.HasSearchTermsReplacementKey(
      GURL("http://google.com/?espv=1#q=something")));

  EXPECT_TRUE(url.HasSearchTermsReplacementKey(
      GURL("http://google.com/?q=something#espv")));

  EXPECT_TRUE(url.HasSearchTermsReplacementKey(
      GURL("http://google.com/?q=something#espv=1")));

  // This does not ensure the domain matches.
  EXPECT_TRUE(url.HasSearchTermsReplacementKey(
      GURL("http://bing.com/?espv")));

  EXPECT_TRUE(url.HasSearchTermsReplacementKey(
      GURL("http://bing.com/#espv")));
}

TEST_F(TemplateURLTest, ReplaceSearchTermsInURL) {
  TemplateURLData data;
  data.SetURL("http://google.com/?q={searchTerms}");
  data.instant_url = "http://google.com/instant#q={searchTerms}";
  data.alternate_urls.push_back("http://google.com/alt/#q={searchTerms}");
  data.alternate_urls.push_back(
      "http://google.com/alt/?ext=foo&q={searchTerms}#ref=bar");
  TemplateURL url(NULL, data);
  TemplateURLRef::SearchTermsArgs search_terms(ASCIIToUTF16("Bob Morane"));
  GURL result;

  EXPECT_TRUE(url.ReplaceSearchTermsInURL(
      GURL("http://google.com/?q=something"), search_terms, &result));
  EXPECT_EQ(GURL("http://google.com/?q=Bob%20Morane"), result);

  result = GURL("http://should.not.change.com");
  EXPECT_FALSE(url.ReplaceSearchTermsInURL(
      GURL("http://google.ca/?q=something"), search_terms, &result));
  EXPECT_EQ(GURL("http://should.not.change.com"), result);

  EXPECT_FALSE(url.ReplaceSearchTermsInURL(
      GURL("http://google.com/foo/?q=foo"), search_terms, &result));

  EXPECT_TRUE(url.ReplaceSearchTermsInURL(
      GURL("https://google.com/?q=foo"), search_terms, &result));
  EXPECT_EQ(GURL("https://google.com/?q=Bob%20Morane"), result);

  EXPECT_FALSE(url.ReplaceSearchTermsInURL(
      GURL("http://google.com:8080/?q=foo"), search_terms, &result));

  EXPECT_TRUE(url.ReplaceSearchTermsInURL(
      GURL("http://google.com/?q=1+2+3&b=456"), search_terms, &result));
  EXPECT_EQ(GURL("http://google.com/?q=Bob%20Morane&b=456"), result);

  // Note: Spaces in REF parameters are not escaped. See TryEncoding() in
  // template_url.cc for details.
  EXPECT_TRUE(url.ReplaceSearchTermsInURL(
      GURL("http://google.com/alt/?q=123#q=456"), search_terms, &result));
  EXPECT_EQ(GURL("http://google.com/alt/?q=123#q=Bob Morane"), result);

  EXPECT_TRUE(url.ReplaceSearchTermsInURL(
      GURL("http://google.com/alt/?a=012&q=123&b=456#f=789"), search_terms,
      &result));
  EXPECT_EQ(GURL("http://google.com/alt/?a=012&q=Bob%20Morane&b=456#f=789"),
            result);

  EXPECT_TRUE(url.ReplaceSearchTermsInURL(
      GURL("http://google.com/alt/?a=012&q=123&b=456#j=abc&q=789&h=def9"),
      search_terms, &result));
  EXPECT_EQ(GURL("http://google.com/alt/?a=012&q=123&b=456"
                 "#j=abc&q=Bob Morane&h=def9"), result);

  EXPECT_FALSE(url.ReplaceSearchTermsInURL(
      GURL("http://google.com/alt/?q="), search_terms, &result));

  EXPECT_FALSE(url.ReplaceSearchTermsInURL(
      GURL("http://google.com/alt/?#q="), search_terms, &result));

  EXPECT_FALSE(url.ReplaceSearchTermsInURL(
      GURL("http://google.com/alt/?q=#q="), search_terms, &result));

  EXPECT_FALSE(url.ReplaceSearchTermsInURL(
      GURL("http://google.com/alt/?q=123#q="), search_terms, &result));

  EXPECT_TRUE(url.ReplaceSearchTermsInURL(
      GURL("http://google.com/alt/?q=#q=123"), search_terms, &result));
  EXPECT_EQ(GURL("http://google.com/alt/?q=#q=Bob Morane"), result);
}

// Test the |suggest_query_params| field of SearchTermsArgs.
TEST_F(TemplateURLTest, SuggestQueryParams) {
  UIThreadSearchTermsData::SetGoogleBaseURL("http://www.google.com/");
  TemplateURLData data;
  // Pick a URL with replacements before, during, and after the query, to ensure
  // we don't goof up any of them.
  data.SetURL("{google:baseURL}search?q={searchTerms}"
      "#{google:originalQueryForSuggestion}x");
  TemplateURL url(NULL, data);

  // Baseline: no |suggest_query_params| field.
  TemplateURLRef::SearchTermsArgs search_terms(ASCIIToUTF16("abc"));
  search_terms.original_query = ASCIIToUTF16("def");
  search_terms.accepted_suggestion = 0;
  EXPECT_EQ("http://www.google.com/search?q=abc#oq=def&x",
            url.url_ref().ReplaceSearchTerms(search_terms));

  // Set the suggest_query_params.
  search_terms.suggest_query_params = "pq=xyz";
  EXPECT_EQ("http://www.google.com/search?pq=xyz&q=abc#oq=def&x",
            url.url_ref().ReplaceSearchTerms(search_terms));

  // Add extra_query_params in the mix, and ensure it works.
  search_terms.append_extra_query_params = true;
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kExtraSearchQueryParams, "a=b");
  EXPECT_EQ("http://www.google.com/search?a=b&pq=xyz&q=abc#oq=def&x",
            url.url_ref().ReplaceSearchTerms(search_terms));
}

// Test the |append_extra_query_params| field of SearchTermsArgs.
TEST_F(TemplateURLTest, ExtraQueryParams) {
  UIThreadSearchTermsData::SetGoogleBaseURL("http://www.google.com/");
  TemplateURLData data;
  // Pick a URL with replacements before, during, and after the query, to ensure
  // we don't goof up any of them.
  data.SetURL("{google:baseURL}search?q={searchTerms}"
      "#{google:originalQueryForSuggestion}x");
  TemplateURL url(NULL, data);

  // Baseline: no command-line args, no |append_extra_query_params| flag.
  TemplateURLRef::SearchTermsArgs search_terms(ASCIIToUTF16("abc"));
  search_terms.original_query = ASCIIToUTF16("def");
  search_terms.accepted_suggestion = 0;
  EXPECT_EQ("http://www.google.com/search?q=abc#oq=def&x",
            url.url_ref().ReplaceSearchTerms(search_terms));

  // Set the flag.  Since there are no command-line args, this should have no
  // effect.
  search_terms.append_extra_query_params = true;
  EXPECT_EQ("http://www.google.com/search?q=abc#oq=def&x",
            url.url_ref().ReplaceSearchTerms(search_terms));

  // Now append the command-line arg.  This should be inserted into the query.
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kExtraSearchQueryParams, "a=b");
  EXPECT_EQ("http://www.google.com/search?a=b&q=abc#oq=def&x",
            url.url_ref().ReplaceSearchTerms(search_terms));

  // Turn off the flag.  Now the command-line arg should be ignored again.
  search_terms.append_extra_query_params = false;
  EXPECT_EQ("http://www.google.com/search?q=abc#oq=def&x",
            url.url_ref().ReplaceSearchTerms(search_terms));
}

// Tests replacing pageClassification.
TEST_F(TemplateURLTest, ReplacePageClassification) {
  UIThreadSearchTermsData::SetGoogleBaseURL("http://www.google.com/");
  TemplateURLData data;
  data.input_encodings.push_back("UTF-8");
  data.SetURL("{google:baseURL}?{google:pageClassification}q={searchTerms}");
  TemplateURL url(NULL, data);
  EXPECT_TRUE(url.url_ref().IsValid());
  ASSERT_TRUE(url.url_ref().SupportsReplacement());
  TemplateURLRef::SearchTermsArgs search_terms_args(ASCIIToUTF16("foo"));

  std::string result = url.url_ref().ReplaceSearchTerms(search_terms_args);
  EXPECT_EQ("http://www.google.com/?q=foo", result);

  search_terms_args.page_classification = AutocompleteInput::NTP;
  result = url.url_ref().ReplaceSearchTerms(search_terms_args);
  EXPECT_EQ("http://www.google.com/?pgcl=1&q=foo", result);

  search_terms_args.page_classification =
      AutocompleteInput::HOME_PAGE;
  result = url.url_ref().ReplaceSearchTerms(search_terms_args);
  EXPECT_EQ("http://www.google.com/?pgcl=3&q=foo", result);
}

// Test the IsSearchResults function.
TEST_F(TemplateURLTest, IsSearchResults) {
  TemplateURLData data;
  data.SetURL("http://bar/search?q={searchTerms}");
  data.instant_url = "http://bar/instant#q={searchTerms}";
  data.new_tab_url = "http://bar/newtab";
  data.alternate_urls.push_back("http://bar/?q={searchTerms}");
  data.alternate_urls.push_back("http://bar/#q={searchTerms}");
  data.alternate_urls.push_back("http://bar/search#q{searchTerms}");
  data.alternate_urls.push_back("http://bar/webhp#q={searchTerms}");
  TemplateURL search_provider(NULL, data);

  const struct {
    const char* const url;
    bool result;
  } url_data[] = {
    { "http://bar/search?q=foo&oq=foo", true, },
    { "http://bar/?q=foo&oq=foo", true, },
    { "http://bar/#output=search&q=foo&oq=foo", true, },
    { "http://bar/webhp#q=foo&oq=foo", true, },
    { "http://bar/#q=foo&oq=foo", true, },
    { "http://bar/?ext=foo&q=foo#ref=bar", true, },
    { "http://bar/url?url=http://www.foo.com/&q=foo#ref=bar", false, },
    { "http://bar/", false, },
    { "http://foo/", false, },
    { "http://bar/newtab", false, },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(url_data); ++i) {
    EXPECT_EQ(url_data[i].result,
              search_provider.IsSearchURL(GURL(url_data[i].url)));
  }
}

TEST_F(TemplateURLTest, ReflectsBookmarkBarPinned) {
  TemplateURLData data;
  data.input_encodings.push_back("UTF-8");
  data.SetURL("{google:baseURL}?{google:bookmarkBarPinned}q={searchTerms}");
  TemplateURL url(NULL, data);
  EXPECT_TRUE(url.url_ref().IsValid());
  ASSERT_TRUE(url.url_ref().SupportsReplacement());
  TemplateURLRef::SearchTermsArgs search_terms_args(ASCIIToUTF16("foo"));

  // Do not add the param when InstantExtended is suppressed on SRPs.
  url.url_ref_.showing_search_terms_ = false;
  std::string result = url.url_ref().ReplaceSearchTerms(search_terms_args);
  EXPECT_EQ("http://www.google.com/?q=foo", result);

  // Add the param when InstantExtended is not suppressed on SRPs.
  url.url_ref_.showing_search_terms_ = true;
  search_terms_args.bookmark_bar_pinned = false;
  result = url.url_ref().ReplaceSearchTerms(search_terms_args);
  EXPECT_EQ("http://www.google.com/?bmbp=0&q=foo", result);

  url.url_ref_.showing_search_terms_ = true;
  search_terms_args.bookmark_bar_pinned = true;
  result = url.url_ref().ReplaceSearchTerms(search_terms_args);
  EXPECT_EQ("http://www.google.com/?bmbp=1&q=foo", result);
}
