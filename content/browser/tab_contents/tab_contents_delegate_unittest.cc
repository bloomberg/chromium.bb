// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockWebContentsDelegate : public content::WebContentsDelegate {
 public:
  virtual ~MockWebContentsDelegate() {}

  virtual TabContents* OpenURLFromTab(TabContents* source,
                              const OpenURLParams& params) OVERRIDE {
    return NULL;
  }

  virtual void NavigationStateChanged(const TabContents* source,
                                      unsigned changed_flags) OVERRIDE {}

  virtual void AddNavigationHeaders(
      const GURL& url, std::string* headers) OVERRIDE {
  }

  virtual void AddNewContents(TabContents* source,
                              TabContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture) OVERRIDE {}

  virtual void ActivateContents(TabContents* contents) OVERRIDE {}

  virtual void DeactivateContents(TabContents* contents) OVERRIDE {}

  virtual void LoadingStateChanged(TabContents* source) OVERRIDE {}

  virtual void LoadProgressChanged(double progress) OVERRIDE {}

  virtual void CloseContents(TabContents* source) OVERRIDE {}

  virtual void MoveContents(TabContents* source,
                            const gfx::Rect& pos) OVERRIDE {
  }

  virtual void UpdateTargetURL(TabContents* source, int32 page_id,
                               const GURL& url) {}
};

TEST(WebContentsDelegateTest, UnregisterInDestructor) {
  MessageLoop loop(MessageLoop::TYPE_UI);
  TestBrowserContext browser_context;

  scoped_ptr<TabContents> contents_a(
      new TabContents(&browser_context, NULL, 0, NULL, NULL));
  scoped_ptr<TabContents> contents_b(
      new TabContents(&browser_context, NULL, 0, NULL, NULL));
  EXPECT_TRUE(contents_a->GetDelegate() == NULL);
  EXPECT_TRUE(contents_b->GetDelegate() == NULL);

  scoped_ptr<MockWebContentsDelegate> delegate(new MockWebContentsDelegate());

  // Setting a delegate should work correctly.
  contents_a->SetDelegate(delegate.get());
  EXPECT_EQ(delegate.get(), contents_a->GetDelegate());
  EXPECT_TRUE(contents_b->GetDelegate() == NULL);

  // A delegate can be a delegate to multiple TabContents.
  contents_b->SetDelegate(delegate.get());
  EXPECT_EQ(delegate.get(), contents_a->GetDelegate());
  EXPECT_EQ(delegate.get(), contents_b->GetDelegate());

  // Setting the same delegate multiple times should work correctly.
  contents_b->SetDelegate(delegate.get());
  EXPECT_EQ(delegate.get(), contents_a->GetDelegate());
  EXPECT_EQ(delegate.get(), contents_b->GetDelegate());

  // Setting delegate to NULL should work correctly.
  contents_b->SetDelegate(NULL);
  EXPECT_EQ(delegate.get(), contents_a->GetDelegate());
  EXPECT_TRUE(contents_b->GetDelegate() == NULL);

  // Destroying the delegate while it is still the delegate
  // for a TabContents should unregister it.
  contents_b->SetDelegate(delegate.get());
  EXPECT_EQ(delegate.get(), contents_a->GetDelegate());
  EXPECT_EQ(delegate.get(), contents_b->GetDelegate());
  delegate.reset(NULL);
  EXPECT_TRUE(contents_a->GetDelegate() == NULL);
  EXPECT_TRUE(contents_b->GetDelegate() == NULL);

  // Destroy the tab contents and run the message loop to prevent leaks.
  contents_a.reset(NULL);
  contents_b.reset(NULL);
  loop.RunAllPending();
}

}  // namespace
