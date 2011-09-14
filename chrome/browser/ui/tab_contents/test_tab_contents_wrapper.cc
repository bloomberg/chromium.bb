// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_contents/test_tab_contents_wrapper.h"

#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/test/base/testing_profile.h"
#include "content/browser/tab_contents/test_tab_contents.h"

TabContentsWrapperTestHarness::TabContentsWrapperTestHarness()
    : ChromeRenderViewHostTestHarness() {
}

TabContentsWrapperTestHarness::~TabContentsWrapperTestHarness() {
}

TestTabContents* TabContentsWrapperTestHarness::contents() {
  return contents_wrapper_.get() ?
      static_cast<TestTabContents*>(contents_wrapper_->tab_contents()) : NULL;
}

TabContentsWrapper* TabContentsWrapperTestHarness::contents_wrapper() {
  return contents_wrapper_.get();
}

void TabContentsWrapperTestHarness::SetContents(TestTabContents* contents) {
  contents_wrapper_.reset(contents ? new TabContentsWrapper(contents) : NULL);
}

void TabContentsWrapperTestHarness::SetUp() {
  if (!browser_context_.get())
    browser_context_.reset(new TestingProfile());
  SetContents(CreateTestTabContents());
}

void TabContentsWrapperTestHarness::TearDown() {
  contents_wrapper_.reset();

  // Make sure that we flush any messages related to TabContents destruction
  // before we destroy the browser context.
  MessageLoop::current()->RunAllPending();

  // Release the browser context on the UI thread.
  message_loop_.DeleteSoon(FROM_HERE, browser_context_.release());
  message_loop_.RunAllPending();
}
