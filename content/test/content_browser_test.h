// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_CONTENT_BROWSER_TEST_H_
#define CONTENT_TEST_CONTENT_BROWSER_TEST_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "content/test/browser_test_base.h"

class ShellMainDelegate;

class ContentBrowserTest : public BrowserTestBase {
 protected:
  ContentBrowserTest();
  virtual ~ContentBrowserTest();

  // testing::Test:
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  // BrowserTestBase:
  virtual void RunTestOnMainThreadLoop() OVERRIDE;

 private:
  scoped_ptr<ShellMainDelegate> shell_main_delegate_;
};

#endif  // CONTENT_TEST_CONTENT_BROWSER_TEST_H_
