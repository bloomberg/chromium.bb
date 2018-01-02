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
      {"http://foo.com/", "http://foo.com/", "From foo.com", "From foo.com",
       "From foo.com"},

      // Subframe alert from the same origin.
      {"http://foo.com/1", "http://foo.com/2", "From foo.com", "From foo.com",
       "From foo.com"},
      // Subframe alert from a different origin.
      {"http://foo.com/", "http://bar.com/", "From an Embedded Page at bar.com",
       "From an embedded page at bar.com", "From an embedded page at bar.com"},

      // file:
      // - main frame:
      {"file:///path/to/page.html", "file:///path/to/page.html",
       "From This Page", "From this page", "From this page"},
      // - subframe:
      {"http://foo.com/", "file:///path/to/page.html",
       "From an Embedded Page on This Page",
       "From an embedded page on this page",
       "From an embedded page on this page"},

      // ftp:
      // - main frame:
      {"ftp://foo.com/path/to/page.html", "ftp://foo.com/path/to/page.html",
       "From foo.com", "From foo.com", "From ftp://foo.com"},
      // - subframe:
      {"http://foo.com/", "ftp://foo.com/path/to/page.html",
       "From an Embedded Page at foo.com", "From an embedded page at foo.com",
       "From an embedded page at ftp://foo.com"},

      // data:
      // - main frame:
      {"data:blahblah", "data:blahblah", "From This Page", "From this page",
       "From this page"},
      // - subframe:
      {"http://foo.com/", "data:blahblah", "From an Embedded Page on This Page",
       "From an embedded page on this page",
       "From an embedded page on this page"},

      // javascript:
      // - main frame:
      {"javascript:abc", "javascript:abc", "From This Page", "From this page",
       "From this page"},
      // - subframe:
      {"http://foo.com/", "javascript:abc",
       "From an Embedded Page on This Page",
       "From an embedded page on this page",
       "From an embedded page on this page"},

      // about:
      // - main frame:
      {"about:blank", "about:blank", "From This Page", "From this page",
       "From this page"},
      // - subframe:
      {"http://foo.com/", "about:blank", "From an Embedded Page on This Page",
       "From an embedded page on this page",
       "From an embedded page on this page"},

      // blob:
      // - main frame:
      {"blob:http://foo.com/66666666-6666-6666-6666-666666666666",
       "blob:http://foo.com/66666666-6666-6666-6666-666666666666",
       "From foo.com", "From foo.com", "From foo.com"},
      // - subframe:
      {"http://bar.com/",
       "blob:http://foo.com/66666666-6666-6666-6666-666666666666",
       "From an Embedded Page at foo.com", "From an embedded page at foo.com",
       "From an embedded page at foo.com"},

      // filesystem:
      // - main frame:
      {"filesystem:http://foo.com/bar.html",
       "filesystem:http://foo.com/bar.html", "From foo.com", "From foo.com",
       "From foo.com"},
      // - subframe:
      {"http://bar.com/", "filesystem:http://foo.com/bar.html",
       "From an Embedded Page at foo.com", "From an embedded page at foo.com",
       "From an embedded page at foo.com"},
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
