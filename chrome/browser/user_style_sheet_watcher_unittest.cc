// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/user_style_sheet_watcher.h"

#include "base/basictypes.h"
#include "base/base64.h"
#include "base/file_util.h"
#include "base/memory/scoped_temp_dir.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/threading/thread.h"
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

  MessageLoop loop(MessageLoop::TYPE_UI);
  base::Thread io_thread("UserStyleSheetWatcherTestIOThread");
  base::Thread::Options options(MessageLoop::TYPE_IO, 0);
  ASSERT_TRUE(io_thread.StartWithOptions(options));
  BrowserThread browser_ui_thread(BrowserThread::UI, &loop);
  BrowserThread browser_file_thread(BrowserThread::FILE,
                                    io_thread.message_loop());

  // It is important that the creation of |style_sheet_watcher| occur after the
  // creation of |browser_ui_thread| because UserStyleSheetWatchers are
  // restricted to being deleted only on UI browser threads.
  scoped_refptr<UserStyleSheetWatcher> style_sheet_watcher(
      new UserStyleSheetWatcher(dir.path()));
  style_sheet_watcher->Init();

  io_thread.Stop();
  loop.RunAllPending();

  GURL result_url = style_sheet_watcher->user_style_sheet();
  std::string result = result_url.spec();
  std::string prefix = "data:text/css;charset=utf-8;base64,";
  ASSERT_TRUE(StartsWithASCII(result, prefix, true));
  result = result.substr(prefix.length());
  std::string decoded;
  ASSERT_TRUE(base::Base64Decode(result, &decoded));
  ASSERT_EQ(css_file_contents, decoded);
}
