// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/web_contents_tester.h"

#include "content/browser/tab_contents/test_tab_contents.h"

namespace content {

namespace {

// The two subclasses here are instantiated via the deprecated
// CreateWebContentsFor... factories below.

class TestTabContentsCountFocus : public TestTabContents {
 public:
  TestTabContentsCountFocus(content::BrowserContext* browser_context,
                       content::SiteInstance* instance)
      : TestTabContents(browser_context, instance), focus_called_(0) {
  }

  virtual int GetNumberOfFocusCalls() OVERRIDE {
    return focus_called_;
  }

  virtual void Focus() OVERRIDE {
    focus_called_++;
  }

 private:
  int focus_called_;
};

class TestTabContentsCountSetFocusToLocationBar : public TestTabContents {
 public:
  TestTabContentsCountSetFocusToLocationBar(
      content::BrowserContext* browser_context,
      SiteInstance* instance)
      : TestTabContents(browser_context, instance), focus_called_(0) {
  }

  virtual void SetFocusToLocationBar(bool select_all) { ++focus_called_; }
  virtual int GetNumberOfFocusCalls() OVERRIDE {
    return focus_called_;
  }

 private:
  int focus_called_;
};

}  // namespace

// static
WebContentsTester* WebContentsTester::For(WebContents* contents) {
  return static_cast<TestTabContents*>(contents);
}

// static
WebContents* WebContentsTester::CreateTestWebContents(
    BrowserContext* browser_context,
    SiteInstance* instance) {
  return new TestTabContents(browser_context, instance);
}

// static
WebContents* WebContentsTester::CreateTestWebContentsCountSetFocusToLocationBar(
    BrowserContext* browser_context,
    SiteInstance* instance) {
  return new TestTabContentsCountSetFocusToLocationBar(
      browser_context, instance);
}

// static
WebContents* WebContentsTester::CreateTestWebContentsCountFocus(
    BrowserContext* browser_context,
    SiteInstance* instance) {
  return new TestTabContentsCountFocus(browser_context, instance);
}

}  // namespace content
