// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockWebContentsDelegate : public content::WebContentsDelegate {
 public:
  virtual ~MockWebContentsDelegate() {}
};

TEST(WebContentsDelegateTest, UnregisterInDestructor) {
  MessageLoop loop(MessageLoop::TYPE_UI);
  TestBrowserContext browser_context;

  scoped_ptr<WebContentsImpl> contents_a(new WebContentsImpl(&browser_context,
                                                             NULL,
                                                             MSG_ROUTING_NONE,
                                                             NULL,
                                                             NULL,
                                                             NULL));
  scoped_ptr<WebContentsImpl> contents_b(new WebContentsImpl(&browser_context,
                                                             NULL,
                                                             MSG_ROUTING_NONE,
                                                             NULL,
                                                             NULL,
                                                             NULL));
  EXPECT_TRUE(contents_a->GetDelegate() == NULL);
  EXPECT_TRUE(contents_b->GetDelegate() == NULL);

  scoped_ptr<MockWebContentsDelegate> delegate(new MockWebContentsDelegate());

  // Setting a delegate should work correctly.
  contents_a->SetDelegate(delegate.get());
  EXPECT_EQ(delegate.get(), contents_a->GetDelegate());
  EXPECT_TRUE(contents_b->GetDelegate() == NULL);

  // A delegate can be a delegate to multiple WebContentsImpl.
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
  // for a WebContentsImpl should unregister it.
  contents_b->SetDelegate(delegate.get());
  EXPECT_EQ(delegate.get(), contents_a->GetDelegate());
  EXPECT_EQ(delegate.get(), contents_b->GetDelegate());
  delegate.reset(NULL);
  EXPECT_TRUE(contents_a->GetDelegate() == NULL);
  EXPECT_TRUE(contents_b->GetDelegate() == NULL);

  // Destroy the WebContentses and run the message loop to prevent leaks.
  contents_a.reset(NULL);
  contents_b.reset(NULL);
  loop.RunAllPending();
}

}  // namespace
