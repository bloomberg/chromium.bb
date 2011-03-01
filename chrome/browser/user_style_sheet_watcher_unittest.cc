// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/user_style_sheet_watcher.h"

#include "base/base64.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/scoped_temp_dir.h"
#include "base/string_util.h"
#include "content/browser/browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(UserStyleSheetWatcherTest, StyleLoad) {
  ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());

  std::string css_file_contents = "a { color: green; }";
  FilePath style_sheet_file = dir.path().AppendASCII("User StyleSheets")
                              .AppendASCII("Custom.css");
  file_util::CreateDirectory(style_sheet_file.DirName());
  ASSERT_TRUE(file_util::WriteFile(style_sheet_file,
              css_file_contents.data(), css_file_contents.length()));

  scoped_refptr<UserStyleSheetWatcher> style_sheet_watcher(
      new UserStyleSheetWatcher(dir.path()));
  MessageLoop loop;
  BrowserThread ui_thread(BrowserThread::UI, &loop);
  BrowserThread file_thread(BrowserThread::FILE, &loop);
  style_sheet_watcher->Init();

  loop.RunAllPending();

  GURL result_url = style_sheet_watcher->user_style_sheet();
  std::string result = result_url.spec();
  std::string prefix = "data:text/css;charset=utf-8;base64,";
  EXPECT_TRUE(StartsWithASCII(result, prefix, true));
  result = result.substr(prefix.length());
  std::string decoded;
  EXPECT_TRUE(base::Base64Decode(result, &decoded));
  EXPECT_EQ(css_file_contents, decoded);
  style_sheet_watcher = NULL;
}
