// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/web_contents_user_data.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class WebContentsAttachedClass1
    : public WebContentsUserData<WebContentsAttachedClass1> {
 public:
  explicit WebContentsAttachedClass1(content::WebContents* contents) {}
  virtual ~WebContentsAttachedClass1() {}
};

class WebContentsAttachedClass2
    : public WebContentsUserData<WebContentsAttachedClass2> {
 public:
  explicit WebContentsAttachedClass2(content::WebContents* contents) {}
  virtual ~WebContentsAttachedClass2() {}
};

}  // namespace

typedef ChromeRenderViewHostTestHarness WebContentsUserDataTest;

TEST_F(WebContentsUserDataTest, BasicTest) {
  content::WebContents* contents = web_contents();
  WebContentsAttachedClass1* class1 =
      WebContentsAttachedClass1::FromWebContents(contents);
  ASSERT_EQ(NULL, class1);
  WebContentsAttachedClass2* class2 =
      WebContentsAttachedClass2::FromWebContents(contents);
  ASSERT_EQ(NULL, class2);

  WebContentsAttachedClass1::CreateForWebContents(contents);
  class1 = WebContentsAttachedClass1::FromWebContents(contents);
  ASSERT_TRUE(class1 != NULL);
  class2 = WebContentsAttachedClass2::FromWebContents(contents);
  ASSERT_EQ(NULL, class2);

  WebContentsAttachedClass2::CreateForWebContents(contents);
  WebContentsAttachedClass1* class1again =
      WebContentsAttachedClass1::FromWebContents(contents);
  ASSERT_TRUE(class1again != NULL);
  class2 = WebContentsAttachedClass2::FromWebContents(contents);
  ASSERT_TRUE(class2 != NULL);
  ASSERT_EQ(class1, class1again);
  ASSERT_NE(static_cast<void*>(class1), static_cast<void*>(class2));
}
