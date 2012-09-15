// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/web_contents_user_data.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class WebContentsAttachedClass1
    : public WebContentsUserData<WebContentsAttachedClass1> {
 public:
  virtual ~WebContentsAttachedClass1() {}
 private:
  explicit WebContentsAttachedClass1(content::WebContents* contents) {}
  static int kUserDataKey;
  friend class WebContentsUserData<WebContentsAttachedClass1>;
};

class WebContentsAttachedClass2
    : public WebContentsUserData<WebContentsAttachedClass2> {
 public:
  virtual ~WebContentsAttachedClass2() {}
 private:
  explicit WebContentsAttachedClass2(content::WebContents* contents) {}
  static int kUserDataKey;
  friend class WebContentsUserData<WebContentsAttachedClass2>;
};

int WebContentsAttachedClass1::kUserDataKey;
int WebContentsAttachedClass2::kUserDataKey;

}  // namespace

typedef ChromeRenderViewHostTestHarness WebContentsUserDataTest;

TEST_F(WebContentsUserDataTest, OneInstanceTwoAttachments) {
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

TEST_F(WebContentsUserDataTest, TwoInstancesOneAttachment) {
  content::WebContents* contents1 = web_contents();
  scoped_ptr<content::WebContents> contents2(
      content::WebContentsTester::CreateTestWebContents(profile(), NULL));

  WebContentsAttachedClass1* one_class =
      WebContentsAttachedClass1::FromWebContents(contents1);
  ASSERT_EQ(NULL, one_class);
  WebContentsAttachedClass1* two_class =
      WebContentsAttachedClass1::FromWebContents(contents2.get());
  ASSERT_EQ(NULL, two_class);

  WebContentsAttachedClass1::CreateForWebContents(contents1);
  one_class = WebContentsAttachedClass1::FromWebContents(contents1);
  ASSERT_TRUE(one_class != NULL);
  two_class = WebContentsAttachedClass1::FromWebContents(contents2.get());
  ASSERT_EQ(NULL, two_class);

  WebContentsAttachedClass1::CreateForWebContents(contents2.get());
  WebContentsAttachedClass1* one_class_again =
      WebContentsAttachedClass1::FromWebContents(contents1);
  ASSERT_TRUE(one_class_again != NULL);
  two_class = WebContentsAttachedClass1::FromWebContents(contents2.get());
  ASSERT_TRUE(two_class != NULL);
  ASSERT_EQ(one_class, one_class_again);
  ASSERT_NE(one_class, two_class);
}

TEST_F(WebContentsUserDataTest, Idempotence) {
  content::WebContents* contents = web_contents();
  WebContentsAttachedClass1* clazz =
      WebContentsAttachedClass1::FromWebContents(contents);
  ASSERT_EQ(NULL, clazz);

  WebContentsAttachedClass1::CreateForWebContents(contents);
  clazz = WebContentsAttachedClass1::FromWebContents(contents);
  ASSERT_TRUE(clazz != NULL);

  WebContentsAttachedClass1::CreateForWebContents(contents);
  WebContentsAttachedClass1* class_again =
      WebContentsAttachedClass1::FromWebContents(contents);
  ASSERT_TRUE(class_again != NULL);
  ASSERT_EQ(clazz, class_again);
}
