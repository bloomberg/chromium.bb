// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/guest_view/browser/guest_view_manager.h"
#include "components/guest_view/browser/guest_view_manager_delegate.h"
#include "components/guest_view/browser/test_guest_view_manager.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::WebContents;
using content::WebContentsTester;

namespace guest_view {

namespace {

class GuestViewManagerTest : public content::RenderViewHostTestHarness {
 public:
  GuestViewManagerTest() {}
  ~GuestViewManagerTest() override {}

  scoped_ptr<WebContents> CreateWebContents() {
    return scoped_ptr<WebContents>(
        WebContentsTester::CreateTestWebContents(&browser_context_, NULL));
  }

 private:
  content::TestBrowserContext browser_context_;

  DISALLOW_COPY_AND_ASSIGN(GuestViewManagerTest);
};

}  // namespace

TEST_F(GuestViewManagerTest, AddRemove) {
  content::TestBrowserContext browser_context;
  scoped_ptr<GuestViewManagerDelegate> delegate(
      new GuestViewManagerDelegate());
  scoped_ptr<TestGuestViewManager> manager(
      new TestGuestViewManager(&browser_context, delegate.Pass()));

  scoped_ptr<WebContents> web_contents1(CreateWebContents());
  scoped_ptr<WebContents> web_contents2(CreateWebContents());
  scoped_ptr<WebContents> web_contents3(CreateWebContents());

  EXPECT_EQ(0, manager->last_instance_id_removed());

  EXPECT_TRUE(manager->CanUseGuestInstanceID(1));
  EXPECT_TRUE(manager->CanUseGuestInstanceID(2));
  EXPECT_TRUE(manager->CanUseGuestInstanceID(3));

  manager->AddGuest(1, web_contents1.get());
  manager->AddGuest(2, web_contents2.get());
  manager->RemoveGuest(2);

  // Since we removed 2, it would be an invalid ID.
  EXPECT_TRUE(manager->CanUseGuestInstanceID(1));
  EXPECT_FALSE(manager->CanUseGuestInstanceID(2));
  EXPECT_TRUE(manager->CanUseGuestInstanceID(3));

  EXPECT_EQ(0, manager->last_instance_id_removed());

  EXPECT_TRUE(manager->CanUseGuestInstanceID(3));

  manager->AddGuest(3, web_contents3.get());
  manager->RemoveGuest(1);
  EXPECT_FALSE(manager->CanUseGuestInstanceID(1));
  EXPECT_FALSE(manager->CanUseGuestInstanceID(2));

  EXPECT_EQ(2, manager->last_instance_id_removed());
  manager->RemoveGuest(3);
  EXPECT_EQ(3, manager->last_instance_id_removed());

  EXPECT_FALSE(manager->CanUseGuestInstanceID(1));
  EXPECT_FALSE(manager->CanUseGuestInstanceID(2));
  EXPECT_FALSE(manager->CanUseGuestInstanceID(3));

  EXPECT_EQ(0u, manager->GetNumRemovedInstanceIDs());
}

}  // namespace guest_view
