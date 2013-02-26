// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/web_contents_tester.h"

#include "content/test/test_web_contents.h"

namespace content {

namespace {

// The two subclasses here are instantiated via the deprecated
// CreateWebContentsFor... factories below.

class TestWebContentsCountSetFocusToLocationBar : public TestWebContents {
 public:
  explicit TestWebContentsCountSetFocusToLocationBar(
      BrowserContext* browser_context)
      : TestWebContents(browser_context), focus_called_(0) {
  }

  virtual void SetFocusToLocationBar(bool select_all) OVERRIDE {
    ++focus_called_;
  }
  virtual int GetNumberOfFocusCalls() OVERRIDE {
    return focus_called_;
  }

 private:
  int focus_called_;
};

}  // namespace

// static
WebContentsTester* WebContentsTester::For(WebContents* contents) {
  return static_cast<TestWebContents*>(contents);
}

// static
WebContents* WebContentsTester::CreateTestWebContents(
    BrowserContext* browser_context,
    SiteInstance* instance) {
  return TestWebContents::Create(browser_context, instance);
}

// static
WebContents* WebContentsTester::CreateTestWebContentsCountSetFocusToLocationBar(
    BrowserContext* browser_context,
    SiteInstance* instance) {
  TestWebContentsCountSetFocusToLocationBar* web_contents =
      new TestWebContentsCountSetFocusToLocationBar(browser_context);
  web_contents->Init(WebContents::CreateParams(browser_context, instance));
  return web_contents;
}

}  // namespace content
