// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_contents/test_tab_contents_wrapper.h"

#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/test/testing_profile.h"
#include "content/browser/tab_contents/test_tab_contents.h"

TabContentsWrapperTestHarness::TabContentsWrapperTestHarness()
    : RenderViewHostTestHarness() {
}

TabContentsWrapperTestHarness::~TabContentsWrapperTestHarness() {
}

TestTabContents* TabContentsWrapperTestHarness::contents() {
  return static_cast<TestTabContents*>(contents_wrapper_.get()->tab_contents());
}

TabContentsWrapper* TabContentsWrapperTestHarness::contents_wrapper() {
  return contents_wrapper_.get();
}

void TabContentsWrapperTestHarness::SetContents(TestTabContents* contents) {
  contents_wrapper_.reset(new TabContentsWrapper(contents));
}

void TabContentsWrapperTestHarness::SetUp() {
  SetContents(CreateTestTabContents());
}

void TabContentsWrapperTestHarness::TearDown() {
  contents_wrapper_.reset();

  // Make sure that we flush any messages related to TabContents destruction
  // before we destroy the profile.
  MessageLoop::current()->RunAllPending();

  // Release the profile on the UI thread.
  message_loop_.DeleteSoon(FROM_HERE, profile_.release());
  message_loop_.RunAllPending();
}
