// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <atlsecurity.h>
#include <shellapi.h>

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/ref_counted.h"
#include "base/scoped_handle.h"
#include "base/task.h"
#include "base/win_util.h"
#include "net/base/net_util.h"

#include "chrome_frame/test/chrome_frame_unittests.h"
#include "chrome_frame/chrome_frame_automation.h"
#include "chrome_frame/chrome_frame_delegate.h"
#include "chrome_frame/html_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

class HtmlUtilUnittest : public testing::Test {
 protected:
  // Constructor
  HtmlUtilUnittest() {}

  // Returns the test path given a test case.
  virtual bool GetTestPath(const std::wstring& test_case, std::wstring* path) {
    if (!path) {
      NOTREACHED();
      return false;
    }

    std::wstring test_path;
    if (!PathService::Get(base::DIR_SOURCE_ROOT, &test_path)) {
      NOTREACHED();
      return false;
    }

    file_util::AppendToPath(&test_path, L"chrome_frame");
    file_util::AppendToPath(&test_path, L"test");
    file_util::AppendToPath(&test_path, L"html_util_test_data");
    file_util::AppendToPath(&test_path, test_case);

    *path = test_path;
    return true;
  }

  virtual bool GetTestData(const std::wstring& test_case, std::wstring* data) {
    if (!data) {
      NOTREACHED();
      return false;
    }

    std::wstring path;
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
  GetTestData(L"basic_test.html", &test_data);

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
  GetTestData(L"quotes_test.html", &test_data);

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
  GetTestData(L"degenerate_cases_test.html", &test_data);

  HTMLScanner scanner(test_data.c_str());

  // Scan for meta tags in the document. We expect not to pick up the one
  // that appears to be there since it is technically inside a quote block.
  HTMLScanner::StringRangeList tag_list;
  scanner.GetTagsByName(L"meta", &tag_list, L"body");
  EXPECT_TRUE(tag_list.empty());
}

TEST_F(HtmlUtilUnittest, MultipleTagsTest) {
  std::wstring test_data;
  GetTestData(L"multiple_tags.html", &test_data);

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

  // Grab the meta tag from the document and ensure that we get exactly one.
  HTMLScanner::StringRangeList tag_list;
  scanner.GetTagsByName(L"meta", &tag_list, L"body");
  ASSERT_TRUE(tag_list.empty());
}
