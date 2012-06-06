// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_contents/test_tab_contents.h"

#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"

using content::WebContents;

TabContentsTestHarness::TabContentsTestHarness()
    : ChromeRenderViewHostTestHarness() {
}

TabContentsTestHarness::~TabContentsTestHarness() {
}

WebContents* TabContentsTestHarness::web_contents() {
  return tab_contents_.get() ? tab_contents_->web_contents() : NULL;
}

TabContents* TabContentsTestHarness::tab_contents() {
  return tab_contents_.get();
}

void TabContentsTestHarness::SetContents(WebContents* contents) {
  tab_contents_.reset(contents ? new TabContents(contents) : NULL);
}

void TabContentsTestHarness::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();
  SetContents(CreateTestWebContents());
}

void TabContentsTestHarness::TearDown() {
  tab_contents_.reset();

  // Make sure that we flush any messages related to WebContents destruction
  // before we destroy the browser context.
  MessageLoop::current()->RunAllPending();

  // Release the browser context on the UI thread.
  message_loop_.DeleteSoon(FROM_HERE, browser_context_.release());
  message_loop_.RunAllPending();

  ChromeRenderViewHostTestHarness::TearDown();
}
