// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/in_process_webkit/webkit_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TEST(WebKitThreadTest, DISABLED_ExposedInBrowserThread) {
  int* null = NULL;  // Help the template system out.
  EXPECT_FALSE(BrowserThread::DeleteSoon(BrowserThread::WEBKIT,
                                         FROM_HERE, null));
  {
    WebKitThread thread;
    EXPECT_FALSE(BrowserThread::DeleteSoon(BrowserThread::WEBKIT,
                                           FROM_HERE, null));
    thread.Initialize();
    EXPECT_TRUE(BrowserThread::DeleteSoon(BrowserThread::WEBKIT,
                                          FROM_HERE, null));
  }
  EXPECT_FALSE(BrowserThread::DeleteSoon(BrowserThread::WEBKIT,
                                         FROM_HERE, null));
}

}  // namespace content
