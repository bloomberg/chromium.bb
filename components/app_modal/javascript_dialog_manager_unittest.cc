// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/app_modal/javascript_dialog_manager.h"

// #include <string>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_modal {

TEST(JavaScriptDialogManagerTest, GetTitle) {
  struct Case {
    const char* parent_url;
    const char* alerting_url;
    const char* expected_titlecase;
    const char* expected_no_titlecase;
    const char* expected_android;
  } cases[] = {
      // Standard main frame alert.
      {"http://foo.com/", "http://foo.com/", "foo.com Says", "foo.com says",
       "foo.com says"},

      // Subframe alert from the same origin.
      {"http://foo.com/1", "http://foo.com/2", "foo.com Says", "foo.com says",
       "foo.com says"},
      // Subframe alert from a different origin.
      {"http://foo.com/", "http://bar.com/", "An Embedded Page at bar.com Says",
       "An embedded page at bar.com says", "An embedded page at bar.com says"},

      // file:
      // -main frame:
      {"file:///path/to/page.html", "file:///path/to/page.html",
       "This Page Says", "This page says", "This page says"},
      // -subframe:
      {"http://foo.com/", "file:///path/to/page.html",
       "An Embedded Page on This Page Says",
       "An embedded page on this page says",
       "An embedded page on this page says"},

      // ftp:
      // -main frame:
      {"ftp://foo.com/path/to/page.html", "ftp://foo.com/path/to/page.html",
       "foo.com Says", "foo.com says", "ftp://foo.com says"},
      // -subframe:
      {"http://foo.com/", "ftp://foo.com/path/to/page.html",
       "An Embedded Page at foo.com Says", "An embedded page at foo.com says",
       "An embedded page at ftp://foo.com says"},

      // data:
      // -main frame:
      {"data:blahblah", "data:blahblah", "This Page Says", "This page says",
       "This page says"},
      // -subframe:
      {"http://foo.com/", "data:blahblah", "An Embedded Page on This Page Says",
       "An embedded page on this page says",
       "An embedded page on this page says"},

      // javascript:
      // -main frame:
      {"javascript:abc", "javascript:abc", "This Page Says", "This page says",
       "This page says"},
      // -subframe:
      {"http://foo.com/", "javascript:abc",
       "An Embedded Page on This Page Says",
       "An embedded page on this page says",
       "An embedded page on this page says"},

      // about:
      // -main frame:
      {"about:blank", "about:blank", "This Page Says", "This page says",
       "This page says"},
      // -subframe:
      {"http://foo.com/", "about:blank", "An Embedded Page on This Page Says",
       "An embedded page on this page says",
       "An embedded page on this page says"},

      // blob:
      // -main frame:
      {"blob:http://foo.com/66666666-6666-6666-6666-666666666666",
       "blob:http://foo.com/66666666-6666-6666-6666-666666666666",
       "This Page Says", "This page says", "This page says"},
      // -subframe:
      {"http://foo.com/",
       "blob:http://foo.com/66666666-6666-6666-6666-666666666666",
       "An Embedded Page on This Page Says",
       "An embedded page on this page says",
       "An embedded page on this page says"},

      // filesystem:
      // -main frame:
      {"filesystem:http://foo.com/", "filesystem:http://foo.com/",
       "This Page Says", "This page says", "This page says"},
      // -subframe:
      {"http://foo.com/", "filesystem:http://foo.com/",
       "An Embedded Page on This Page Says",
       "An embedded page on this page says",
       "An embedded page on this page says"},
  };

  for (const auto& test_case : cases) {
    base::string16 result = JavaScriptDialogManager::GetTitleImpl(
        GURL(test_case.parent_url), GURL(test_case.alerting_url));
#if defined(OS_MACOSX)
    EXPECT_EQ(test_case.expected_titlecase, base::UTF16ToUTF8(result));
#elif defined(OS_ANDROID)
    EXPECT_EQ(test_case.expected_android, base::UTF16ToUTF8(result));
#else
    EXPECT_EQ(test_case.expected_no_titlecase, base::UTF16ToUTF8(result));
#endif
  }
}

}  // namespace app_modal
