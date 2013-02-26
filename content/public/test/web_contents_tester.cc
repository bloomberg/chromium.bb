// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/web_contents_tester.h"

#include "content/test/test_web_contents.h"

namespace content {

namespace {

// The two subclasses here are instantiated via the deprecated
// CreateWebContentsFor... factories below.

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
}  // namespace content
