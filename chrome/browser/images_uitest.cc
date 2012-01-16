// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "net/base/net_util.h"
#include "chrome/test/ui/ui_test.h"

typedef UITest ImagesTest;

TEST_F(ImagesTest, AnimatedGIFs) {
  FilePath test_file(test_data_directory_);
  test_file = test_file.AppendASCII("animated-gifs.html");
  NavigateToURL(net::FilePathToFileURL(test_file));

  // Let the GIFs fully animate.
  base::PlatformThread::Sleep(TestTimeouts::action_timeout_ms());

  std::wstring page_title = L"animated gif test";
  EXPECT_EQ(page_title, GetActiveTabTitle());

  // UI test framework will check if this crashed.
}
