// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "content/public/test/test_browser_thread.h"


namespace prerender {

namespace {

// These tests don't require the dummy PrerenderContents & PrerenderManager that
// the heavier PrerenderTest.* tests do.
class PrerenderManagerTest : public testing::Test {
 public:
  PrerenderManagerTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_),
        prerender_manager_(NULL, NULL) {
  }

  PrerenderManager* prerender_manager() { return &prerender_manager_; }

  MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  PrerenderManager prerender_manager_;
};

TEST_F(PrerenderManagerTest, IsWebContentsPrerenderedTest) {
  // The methods being tested should not dereference their WebContents, instead
  // they use it as a unique identifier.
  int not_a_webcontents = 0;
  content::WebContents* web_contents =
      reinterpret_cast<content::WebContents*>(&not_a_webcontents);

  EXPECT_FALSE(prerender_manager()->IsWebContentsPrerendered(
      web_contents, NULL));

  const Origin origin = ORIGIN_OMNIBOX;
  prerender_manager()->MarkWebContentsAsPrerendered(web_contents, origin);

  Origin test_origin = ORIGIN_NONE;
  EXPECT_TRUE(prerender_manager()->IsWebContentsPrerendered(
      web_contents, &test_origin));

  int also_not_a_webcontents = 1;
  content::WebContents* another_web_contents =
      reinterpret_cast<content::WebContents*>(&also_not_a_webcontents);
  EXPECT_FALSE(prerender_manager()->IsWebContentsPrerendered(
      another_web_contents, NULL));

  EXPECT_EQ(origin, test_origin);

  prerender_manager()->MarkWebContentsAsNotPrerendered(web_contents);
  EXPECT_FALSE(prerender_manager()->IsWebContentsPrerendered(
      web_contents, NULL));
}

TEST_F(PrerenderManagerTest, WouldWebContentsBePrerenderedTest) {
  // The methods being tested should not dereference their WebContents, instead
  // they use it as a unique identifier.
  int not_a_webcontents = 0;
  content::WebContents* web_contents =
      reinterpret_cast<content::WebContents*>(&not_a_webcontents);

  EXPECT_FALSE(prerender_manager()->WouldWebContentsBePrerendered(
      web_contents, NULL));

  const Origin origin = ORIGIN_OMNIBOX;
  prerender_manager()->MarkWebContentsAsWouldBePrerendered(web_contents,
                                                           origin);

  Origin test_origin = ORIGIN_NONE;
  EXPECT_TRUE(prerender_manager()->WouldWebContentsBePrerendered(
      web_contents, &test_origin));
  EXPECT_EQ(origin, test_origin);

  int also_not_a_webcontents = 1;
  content::WebContents* another_web_contents =
      reinterpret_cast<content::WebContents*>(&also_not_a_webcontents);
  EXPECT_FALSE(prerender_manager()->WouldWebContentsBePrerendered(
      another_web_contents, NULL));

  // Control group (aka WouldBe...) web_contents need to be removed twice. See
  // the comment in prerender_manager.cc at the definition of
  // WouldBePrerenderedWebContentsData and its inner enum State.
  prerender_manager()->MarkWebContentsAsNotPrerendered(web_contents);
  EXPECT_TRUE(prerender_manager()->WouldWebContentsBePrerendered(
      web_contents, NULL));

  prerender_manager()->MarkWebContentsAsNotPrerendered(web_contents);
  EXPECT_FALSE(prerender_manager()->WouldWebContentsBePrerendered(
      web_contents, NULL));
}

}  // namespace

}  // namespace prerender
