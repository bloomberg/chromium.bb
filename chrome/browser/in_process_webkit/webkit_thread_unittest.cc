// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/in_process_webkit/webkit_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(WebKitThreadTest, DISABLED_ExposedInChromeThread) {
  int* null = NULL;  // Help the template system out.
  EXPECT_FALSE(ChromeThread::DeleteSoon(ChromeThread::WEBKIT, FROM_HERE, null));
  {
    WebKitThread thread;
    EXPECT_FALSE(ChromeThread::DeleteSoon(ChromeThread::WEBKIT,
                                          FROM_HERE, null));
    thread.Initialize();
    EXPECT_TRUE(ChromeThread::DeleteSoon(ChromeThread::WEBKIT,
                                         FROM_HERE, null));
  }
  EXPECT_FALSE(ChromeThread::DeleteSoon(ChromeThread::WEBKIT,
                                        FROM_HERE, null));
}
