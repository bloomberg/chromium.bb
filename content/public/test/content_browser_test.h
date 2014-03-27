// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_CONTENT_BROWSER_TEST_H_
#define CONTENT_PUBLIC_TEST_CONTENT_BROWSER_TEST_H_

#include "base/memory/scoped_ptr.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"

namespace content {
class Shell;
class ShellMainDelegate;

// Base class for browser tests which use content_shell.
class ContentBrowserTest : public BrowserTestBase {
 protected:
  ContentBrowserTest();
  virtual ~ContentBrowserTest();

  // testing::Test:
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  // BrowserTestBase:
  virtual void RunTestOnMainThreadLoop() OVERRIDE;

 protected:
  // Creates a new window and loads about:blank.
  Shell* CreateBrowser();

  // Creates an off-the-record window and loads about:blank.
  Shell* CreateOffTheRecordBrowser();

  // Returns the window for the test.
  Shell* shell() const { return shell_; }

 private:
  Shell* shell_;

  bool setup_called_;

#if defined(OS_ANDROID)
  // For all other platforms, this is done automatically when calling into
  // ContentMain. For Android we set things up manually.
  scoped_ptr<ShellMainDelegate> shell_main_delegate_;
#endif
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_CONTENT_BROWSER_TEST_H_
