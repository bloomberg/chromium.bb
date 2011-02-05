// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/file_path.h"
#include "build/build_config.h"
#include "chrome/test/ui/ui_test.h"
#include "net/base/net_util.h"

class IFrameTest : public UITest {
 protected:
  void NavigateAndVerifyTitle(const char* url, const wchar_t* page_title) {
    FilePath test_file(test_data_directory_);
    test_file = test_file.AppendASCII(url);

    NavigateToURL(net::FilePathToFileURL(test_file));
    EXPECT_EQ(std::wstring(page_title), GetActiveTabTitle());
  }
};

TEST_F(IFrameTest, Crash) {
  NavigateAndVerifyTitle("iframe.html", L"iframe test");
}

// Flakily crashes on non linux oses: http://crbug.com/70192
#if defined(OS_LINUX)
#define MAYBE_InEmptyFrame InEmptyFrame
#else
#define MAYBE_InEmptyFrame DISABLED_InEmptyFrame
#endif
TEST_F(IFrameTest, MAYBE_InEmptyFrame) {
  NavigateAndVerifyTitle("iframe_in_empty_frame.html", L"iframe test");
}
