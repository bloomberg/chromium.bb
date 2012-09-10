// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <atlsecurity.h>
#include <shellapi.h>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_handle.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/utf_string_conversions.h"
#include "net/base/net_util.h"

#include "chrome/browser/automation/url_request_automation_job.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome_frame/chrome_frame_automation.h"
#include "chrome_frame/chrome_frame_delegate.h"
#include "chrome_frame/html_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/user_agent/user_agent_util.h"

const char kChromeFrameUserAgent[] = "chromeframe";

class HtmlUtilUnittest : public testing::Test {
 protected:
  // Constructor
  HtmlUtilUnittest() {}

  // Returns the test path given a test case.
  virtual bool GetTestPath(const std::string& test_case, FilePath* path) {
    if (!path) {
      NOTREACHED();
      return false;
    }

    FilePath test_path;
    if (!PathService::Get(base::DIR_SOURCE_ROOT, &test_path)) {
      NOTREACHED();
      return false;
    }

    test_path = test_path.AppendASCII("chrome_frame");
    test_path = test_path.AppendASCII("test");
    test_path = test_path.AppendASCII("html_util_test_data");
    test_path = test_path.AppendASCII(test_case);

    *path = test_path;
    return true;
  }

  virtual bool GetTestData(const std::string& test_case, std::wstring* data) {
    if (!data) {
      NOTREACHED();
      return false;
    }

    FilePath path;
    if (!GetTestPath(test_case, &path)) {
      NOTREACHED();
      return false;
    }

    std::string raw_data;
    file_util::ReadFileToString(path, &raw_data);

    // Convert to wide using the "best effort" assurance described in
    // string_util.h
    data->assign(UTF8ToWide(raw_data));
    return true;
  }
};

TEST_F(HtmlUtilUnittest, BasicTest) {
  std::wstring test_data;
  GetTestData("basic_test.html", &test_data);

  HTMLScanner scanner(test_data.c_str());

  // Grab the meta tag from the document and ensure that we get exactly one.
  HTMLScanner::StringRangeList tag_list;
  scanner.GetTagsByName(L"meta", &tag_list, L"body");
  ASSERT_EQ(1, tag_list.size());

  // Pull out the http-equiv attribute and check its value:
  HTMLScanner::StringRange attribute_value;
  EXPECT_TRUE(tag_list[0].GetTagAttribute(L"http-equiv", &attribute_value));
  EXPECT_TRUE(attribute_value.Equals(L"X-UA-Compatible"));

  // Pull out the content attribute and check its value:
  EXPECT_TRUE(tag_list[0].GetTagAttribute(L"content", &attribute_value));
  EXPECT_TRUE(attribute_value.Equals(L"chrome=1"));
}

TEST_F(HtmlUtilUnittest, QuotesTest) {
  std::wstring test_data;
  GetTestData("quotes_test.html", &test_data);

  HTMLScanner scanner(test_data.c_str());

  // Grab the meta tag from the document and ensure that we get exactly one.
  HTMLScanner::StringRangeList tag_list;
  scanner.GetTagsByName(L"meta", &tag_list, L"body");
  ASSERT_EQ(1, tag_list.size());

  // Pull out the http-equiv attribute and check its value:
  HTMLScanner::StringRange attribute_value;
  EXPECT_TRUE(tag_list[0].GetTagAttribute(L"http-equiv", &attribute_value));
  EXPECT_TRUE(attribute_value.Equals(L"X-UA-Compatible"));

  // Pull out the content attribute and check its value:
  EXPECT_TRUE(tag_list[0].GetTagAttribute(L"content", &attribute_value));
  EXPECT_TRUE(attribute_value.Equals(L"chrome=1"));
}

TEST_F(HtmlUtilUnittest, DegenerateCasesTest) {
  std::wstring test_data;
  GetTestData("degenerate_cases_test.html", &test_data);

  HTMLScanner scanner(test_data.c_str());

  // Scan for meta tags in the document. We expect not to pick up the one
  // that appears to be there since it is technically inside a quote block.
  HTMLScanner::StringRangeList tag_list;
  scanner.GetTagsByName(L"meta", &tag_list, L"body");
  EXPECT_TRUE(tag_list.empty());
}

TEST_F(HtmlUtilUnittest, MultipleTagsTest) {
  std::wstring test_data;
  GetTestData("multiple_tags.html", &test_data);

  HTMLScanner scanner(test_data.c_str());

  // Grab the meta tag from the document and ensure that we get exactly three.
  HTMLScanner::StringRangeList tag_list;
  scanner.GetTagsByName(L"meta", &tag_list, L"body");
  EXPECT_EQ(7, tag_list.size());

  // Pull out the content attribute for each tag and check its value:
  HTMLScanner::StringRange attribute_value;
  HTMLScanner::StringRangeList::const_iterator tag_list_iter(
      tag_list.begin());
  int valid_tag_count = 0;
  for (; tag_list_iter != tag_list.end(); tag_list_iter++) {
    HTMLScanner::StringRange attribute_value;
    if (tag_list_iter->GetTagAttribute(L"http-equiv", &attribute_value) &&
        attribute_value.Equals(L"X-UA-Compatible")) {
      EXPECT_TRUE(tag_list_iter->GetTagAttribute(L"content", &attribute_value));
      EXPECT_TRUE(attribute_value.Equals(L"chrome=1"));
      valid_tag_count++;
    }
  }
  EXPECT_EQ(3, valid_tag_count);
}

TEST_F(HtmlUtilUnittest, ShortDegenerateTest1) {
  std::wstring test_data(
      L"<foo><META http-equiv=X-UA-Compatible content='chrome=1'");

  HTMLScanner scanner(test_data.c_str());

  // Scan for meta tags in the document. We expect not to pick up the one
  // that is there since it is not properly closed.
  HTMLScanner::StringRangeList tag_list;
  scanner.GetTagsByName(L"meta", &tag_list, L"body");
  EXPECT_TRUE(tag_list.empty());
}

TEST_F(HtmlUtilUnittest, ShortDegenerateTest2) {
  std::wstring test_data(
    L"<foo <META http-equiv=X-UA-Compatible content='chrome=1'/>");

  HTMLScanner scanner(test_data.c_str());

  // Scan for meta tags in the document. We expect not to pick up the one
  // that appears to be there since it is inside a non-closed tag.
  HTMLScanner::StringRangeList tag_list;
  scanner.GetTagsByName(L"meta", &tag_list, L"body");
  EXPECT_TRUE(tag_list.empty());
}

TEST_F(HtmlUtilUnittest, QuoteInsideHTMLCommentTest) {
  std::wstring test_data(
    L"<!-- comment' --><META http-equiv=X-UA-Compatible content='chrome=1'/>");

  HTMLScanner scanner(test_data.c_str());

  // Grab the meta tag from the document and ensure that we get exactly one.
  HTMLScanner::StringRangeList tag_list;
  scanner.GetTagsByName(L"meta", &tag_list, L"body");
  ASSERT_EQ(1, tag_list.size());

  // Pull out the http-equiv attribute and check its value:
  HTMLScanner::StringRange attribute_value;
  EXPECT_TRUE(tag_list[0].GetTagAttribute(L"http-equiv", &attribute_value));
  EXPECT_TRUE(attribute_value.Equals(L"X-UA-Compatible"));

  // Pull out the content attribute and check its value:
  EXPECT_TRUE(tag_list[0].GetTagAttribute(L"content", &attribute_value));
  EXPECT_TRUE(attribute_value.Equals(L"chrome=1"));
}

TEST_F(HtmlUtilUnittest, CloseTagInsideHTMLCommentTest) {
  std::wstring test_data(
    L"<!-- comment> <META http-equiv=X-UA-Compatible content='chrome=1'/>-->");

  HTMLScanner scanner(test_data.c_str());

  // Ensure that the the meta tag is NOT detected.
  HTMLScanner::StringRangeList tag_list;
  scanner.GetTagsByName(L"meta", &tag_list, L"body");
  ASSERT_TRUE(tag_list.empty());
}

TEST_F(HtmlUtilUnittest, IEConditionalCommentTest) {
  std::wstring test_data(
      L"<!--[if lte IE 8]><META http-equiv=X-UA-Compatible content='chrome=1'/>"
      L"<![endif]-->");

  HTMLScanner scanner(test_data.c_str());

  // Ensure that the the meta tag IS detected.
  HTMLScanner::StringRangeList tag_list;
  scanner.GetTagsByName(L"meta", &tag_list, L"body");
  ASSERT_EQ(1, tag_list.size());
}

TEST_F(HtmlUtilUnittest, IEConditionalCommentWithNestedCommentTest) {
  std::wstring test_data(
      L"<!--[if IE]><!--<META http-equiv=X-UA-Compatible content='chrome=1'/>"
      L"--><![endif]-->");

  HTMLScanner scanner(test_data.c_str());

  // Ensure that the the meta tag IS NOT detected.
  HTMLScanner::StringRangeList tag_list;
  scanner.GetTagsByName(L"meta", &tag_list, L"body");
  ASSERT_TRUE(tag_list.empty());
}

TEST_F(HtmlUtilUnittest, IEConditionalCommentWithMultipleNestedTagsTest) {
  std::wstring test_data(
      L"<!--[if lte IE 8]>        <META http-equiv=X-UA-Compatible "
      L"content='chrome=1'/><foo bar></foo><foo baz/><![endif]-->"
      L"<boo hoo><boo hah>");

  HTMLScanner scanner(test_data.c_str());

  // Ensure that the the meta tag IS detected.
  HTMLScanner::StringRangeList meta_tag_list;
  scanner.GetTagsByName(L"meta", &meta_tag_list, L"body");
  ASSERT_EQ(1, meta_tag_list.size());

  // Ensure that the foo tags are also detected.
  HTMLScanner::StringRangeList foo_tag_list;
  scanner.GetTagsByName(L"foo", &foo_tag_list, L"body");
  ASSERT_EQ(2, foo_tag_list.size());

  // Ensure that the boo tags are also detected.
  HTMLScanner::StringRangeList boo_tag_list;
  scanner.GetTagsByName(L"boo", &boo_tag_list, L"body");
  ASSERT_EQ(2, boo_tag_list.size());
}

TEST_F(HtmlUtilUnittest, IEConditionalCommentWithAlternateEndingTest) {
  std::wstring test_data(
      L"<!--[if lte IE 8]>        <META http-equiv=X-UA-Compatible "
      L"content='chrome=1'/><foo bar></foo><foo baz/><![endif]>"
      L"<boo hoo><!--><boo hah>");

  HTMLScanner scanner(test_data.c_str());

  // Ensure that the the meta tag IS detected.
  HTMLScanner::StringRangeList meta_tag_list;
  scanner.GetTagsByName(L"meta", &meta_tag_list, L"body");
  ASSERT_EQ(1, meta_tag_list.size());

  // Ensure that the foo tags are also detected.
  HTMLScanner::StringRangeList foo_tag_list;
  scanner.GetTagsByName(L"foo", &foo_tag_list, L"body");
  ASSERT_EQ(2, foo_tag_list.size());

  // Ensure that the boo tags are also detected.
  HTMLScanner::StringRangeList boo_tag_list;
  scanner.GetTagsByName(L"boo", &boo_tag_list, L"body");
  ASSERT_EQ(2, boo_tag_list.size());
}

TEST_F(HtmlUtilUnittest, IEConditionalCommentNonTerminatedTest) {
  // This test shouldn't detect any tags up until the end of the conditional
  // comment tag.
  std::wstring test_data(
      L"<!--[if lte IE 8>        <META http-equiv=X-UA-Compatible "
      L"content='chrome=1'/><foo bar></foo><foo baz/><![endif]>"
      L"<boo hoo><!--><boo hah>");

  HTMLScanner scanner(test_data.c_str());

  // Ensure that the the meta tag IS NOT detected.
  HTMLScanner::StringRangeList meta_tag_list;
  scanner.GetTagsByName(L"meta", &meta_tag_list, L"body");
  ASSERT_TRUE(meta_tag_list.empty());

  // Ensure that the foo tags are NOT detected.
  HTMLScanner::StringRangeList foo_tag_list;
  scanner.GetTagsByName(L"foo", &foo_tag_list, L"body");
  ASSERT_TRUE(foo_tag_list.empty());

  // Ensure that the boo tags are detected.
  HTMLScanner::StringRangeList boo_tag_list;
  scanner.GetTagsByName(L"boo", &boo_tag_list, L"body");
  ASSERT_EQ(2, boo_tag_list.size());
}

struct UserAgentTestCase {
  std::string input_;
  std::string expected_;
} user_agent_test_cases[] = {
  {
    "", ""
  }, {
    "Mozilla/4.7 [en] (WinNT; U)",
    "Mozilla/4.7 [en] (WinNT; U; chromeframe/0.0.0.0)"
  }, {
    "Mozilla/4.0 (compatible; MSIE 5.01; Windows NT)",
    "Mozilla/4.0 (compatible; MSIE 5.01; Windows NT; chromeframe/0.0.0.0)"
  }, {
    "Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.0; T312461; "
        ".NET CLR 1.1.4322)",
    "Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.0; T312461; "
        ".NET CLR 1.1.4322; chromeframe/0.0.0.0)"
  }, {
    "Mozilla/4.0 (compatible; MSIE 5.0; Windows NT 4.0) Opera 5.11 [en]",
    "Mozilla/4.0 (compatible; MSIE 5.0; Windows NT 4.0; chromeframe/0.0.0.0) "
        "Opera 5.11 [en]"
  }, {
    "Mozilla/5.0 (compatible; MSIE 9.0; Windows NT 6.1; Trident/5.0)",
    "Mozilla/5.0 (compatible; MSIE 9.0; Windows NT 6.1; Trident/5.0; "
        "chromeframe/0.0.0.0)"
  }, {
    "Mozilla/5.0 (Windows; U; Windows NT 5.0; en-US; rv:1.0.2) "
        "Gecko/20030208 Netscape/7.02",
    "Mozilla/5.0 (Windows; U; Windows NT 5.0; en-US; rv:1.0.2; "
        "chromeframe/0.0.0.0) Gecko/20030208 Netscape/7.02"
  }, {
    "Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.6) Gecko/20040612 "
        "Firefox/0.8",
    "Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.6; chromeframe/0.0.0.0) "
        "Gecko/20040612 Firefox/0.8"
  }, {
    "Mozilla/5.0 (compatible; Konqueror/3.2; Linux) (KHTML, like Gecko)",
    "Mozilla/5.0 (compatible; Konqueror/3.2; Linux; chromeframe/0.0.0.0) "
        "(KHTML, like Gecko)"
  }, {
    "Lynx/2.8.4rel.1 libwww-FM/2.14 SSL-MM/1.4.1 OpenSSL/0.9.6h",
    "Lynx/2.8.4rel.1 libwww-FM/2.14 SSL-MM/1.4.1 "
        "OpenSSL/0.9.6h chromeframe/0.0.0.0",
  }, {
    "Mozilla/5.0 (X11; U; Linux i686 (x86_64); en-US; rv:1.7.10) "
        "Gecko/20050716 Firefox/1.0.6",
    "Mozilla/5.0 (X11; U; Linux i686 (x86_64; chromeframe/0.0.0.0); en-US; "
        "rv:1.7.10) Gecko/20050716 Firefox/1.0.6"
  }, {
    "Invalid/1.1 ((((((",
    "Invalid/1.1 (((((( chromeframe/0.0.0.0",
  }, {
    "Invalid/1.1 ()))))",
    "Invalid/1.1 ( chromeframe/0.0.0.0)))))",
  }, {
    "Strange/1.1 ()",
    "Strange/1.1 ( chromeframe/0.0.0.0)",
  }
};

TEST_F(HtmlUtilUnittest, AddChromeFrameToUserAgentValue) {
  for (int i = 0; i < arraysize(user_agent_test_cases); ++i) {
    std::string new_ua(
        http_utils::AddChromeFrameToUserAgentValue(
            user_agent_test_cases[i].input_));
    EXPECT_EQ(user_agent_test_cases[i].expected_, new_ua);
  }

  // Now do the same test again, but test that we don't add the chromeframe
  // tag if we've already added it.
  for (int i = 0; i < arraysize(user_agent_test_cases); ++i) {
    std::string ua(user_agent_test_cases[i].expected_);
    std::string new_ua(http_utils::AddChromeFrameToUserAgentValue(ua));
    EXPECT_EQ(user_agent_test_cases[i].expected_, new_ua);
  }
}

TEST_F(HtmlUtilUnittest, RemoveChromeFrameFromUserAgentValue) {
  for (int i = 0; i < arraysize(user_agent_test_cases); ++i) {
    std::string new_ua(
        http_utils::RemoveChromeFrameFromUserAgentValue(
            user_agent_test_cases[i].expected_));
    EXPECT_EQ(user_agent_test_cases[i].input_, new_ua);
  }

  // Also test that we don't modify the UA if chromeframe is not present.
  for (int i = 0; i < arraysize(user_agent_test_cases); ++i) {
    std::string ua(user_agent_test_cases[i].input_);
    std::string new_ua(http_utils::RemoveChromeFrameFromUserAgentValue(ua));
    EXPECT_EQ(user_agent_test_cases[i].input_, new_ua);
  }
}

TEST_F(HtmlUtilUnittest, GetDefaultUserAgentHeaderWithCFTag) {
  std::string ua(http_utils::GetDefaultUserAgentHeaderWithCFTag());
  EXPECT_NE(0u, ua.length());
  EXPECT_NE(std::string::npos, ua.find("Mozilla"));
  EXPECT_NE(std::string::npos, ua.find(kChromeFrameUserAgent));
}

TEST_F(HtmlUtilUnittest, GetChromeUserAgent) {
  // This code is duplicated from chrome_content_client.cc to avoid
  // introducing a link-time dependency on chrome_common.
  chrome::VersionInfo version_info;
  std::string product("Chrome/");
  product += version_info.is_valid() ? version_info.Version() : "0.0.0.0";
  std::string chrome_ua(webkit_glue::BuildUserAgentFromProduct(product));

  const char* ua = http_utils::GetChromeUserAgent();
  EXPECT_EQ(ua, chrome_ua);
}

TEST_F(HtmlUtilUnittest, GetDefaultUserAgent) {
  std::string ua(http_utils::GetDefaultUserAgent());
  EXPECT_NE(0u, ua.length());
  EXPECT_NE(std::string::npos, ua.find("Mozilla"));
}

TEST_F(HtmlUtilUnittest, GetChromeFrameUserAgent) {
  const char* call1 = http_utils::GetChromeFrameUserAgent();
  const char* call2 = http_utils::GetChromeFrameUserAgent();
  // Expect static buffer since caller does no cleanup.
  EXPECT_EQ(call1, call2);
  std::string ua(call1);
  EXPECT_EQ("chromeframe/0.0.0.0", ua);
}

TEST(HttpUtils, HasFrameBustingHeader) {
  // Simple negative cases.
  EXPECT_FALSE(http_utils::HasFrameBustingHeader(""));
  EXPECT_FALSE(http_utils::HasFrameBustingHeader("Content-Type: text/plain"));
  EXPECT_FALSE(http_utils::HasFrameBustingHeader("X-Frame-Optionss: ALLOWALL"));
  // Explicit negative cases, test that we ignore case.
  EXPECT_FALSE(http_utils::HasFrameBustingHeader("X-Frame-Options: ALLOWALL"));
  EXPECT_FALSE(http_utils::HasFrameBustingHeader("X-Frame-Options: allowall"));
  EXPECT_FALSE(http_utils::HasFrameBustingHeader("X-Frame-Options: ALLowalL"));
  // Added space, ensure stripped out
  EXPECT_FALSE(http_utils::HasFrameBustingHeader(
    "X-Frame-Options: ALLOWALL "));
  // Added space with linefeed, ensure still stripped out
  EXPECT_FALSE(http_utils::HasFrameBustingHeader(
    "X-Frame-Options: ALLOWALL \r\n"));
  // Multiple identical headers, all of them allowing framing.
  EXPECT_FALSE(http_utils::HasFrameBustingHeader(
    "X-Frame-Options: ALLOWALL\r\n"
    "X-Frame-Options: ALLOWALL\r\n"
    "X-Frame-Options: ALLOWALL"));
  // Interleave with other headers.
  EXPECT_FALSE(http_utils::HasFrameBustingHeader(
    "Content-Type: text/plain\r\n"
    "X-Frame-Options: ALLOWALL\r\n"
    "Content-Length: 42"));

  // Simple positive cases.
  EXPECT_TRUE(http_utils::HasFrameBustingHeader("X-Frame-Options: deny"));
  EXPECT_TRUE(http_utils::HasFrameBustingHeader(
    "X-Frame-Options: SAMEorigin"));

  // Verify that we pick up case changes in the header name too:
  EXPECT_TRUE(http_utils::HasFrameBustingHeader("X-FRAME-OPTIONS: deny"));
  EXPECT_TRUE(http_utils::HasFrameBustingHeader("x-frame-options: deny"));
  EXPECT_TRUE(http_utils::HasFrameBustingHeader("X-frame-optionS: deny"));
  EXPECT_TRUE(http_utils::HasFrameBustingHeader("X-Frame-optionS: deny"));

  // Allowall entries do not override the denying entries, are
  // order-independent, and the deny entries can interleave with
  // other headers.
  EXPECT_TRUE(http_utils::HasFrameBustingHeader(
    "Content-Length: 42\r\n"
    "X-Frame-Options: ALLOWall\r\n"
    "X-Frame-Options: deny\r\n"));
  EXPECT_TRUE(http_utils::HasFrameBustingHeader(
    "X-Frame-Options: ALLOWall\r\n"
    "Content-Length: 42\r\n"
    "X-Frame-Options: SAMEORIGIN\r\n"));
  EXPECT_TRUE(http_utils::HasFrameBustingHeader(
    "X-Frame-Options: deny\r\n"
    "X-Frame-Options: ALLOWall\r\n"
    "Content-Length: 42\r\n"));
  EXPECT_TRUE(http_utils::HasFrameBustingHeader(
    "X-Frame-Options: SAMEORIGIN\r\n"
    "X-Frame-Options: ALLOWall\r\n"));
}
