// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/chrome_bubble_manager.h"

#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bubble/bubble_controller.h"
#include "components/bubble/bubble_manager_mocks.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ChromeBubbleManagerTest : public testing::Test {
 public:
  ChromeBubbleManagerTest();
  ~ChromeBubbleManagerTest() override;

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

 protected:
  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<TestTabStripModelDelegate> delegate_;
  scoped_ptr<TabStripModel> tabstrip_;
  scoped_ptr<ChromeBubbleManager> manager_;

 private:
  // ChromeBubbleManager expects to be on the UI thread.
  content::TestBrowserThreadBundle thread_bundle_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBubbleManagerTest);
};

ChromeBubbleManagerTest::ChromeBubbleManagerTest()
    : thread_bundle_(content::TestBrowserThreadBundle::DEFAULT) {}

ChromeBubbleManagerTest::~ChromeBubbleManagerTest() {}

void ChromeBubbleManagerTest::SetUp() {
  testing::Test::SetUp();

  profile_.reset(new TestingProfile);
  delegate_.reset(new TestTabStripModelDelegate);
  tabstrip_.reset(new TabStripModel(delegate_.get(), profile_.get()));
  manager_.reset(new ChromeBubbleManager(tabstrip_.get()));
}

void ChromeBubbleManagerTest::TearDown() {
  manager_.reset();
  tabstrip_.reset();
  delegate_.reset();
  profile_.reset();
  testing::Test::TearDown();
}

TEST_F(ChromeBubbleManagerTest, CloseMockBubbleOnDestroy) {
  BubbleReference bubble1 = manager_->ShowBubble(MockBubbleDelegate::Default());
  manager_.reset();
  ASSERT_FALSE(bubble1);
}

TEST_F(ChromeBubbleManagerTest, CloseMockBubbleForTwoDifferentReasons) {
  BubbleReference bubble1 = manager_->ShowBubble(MockBubbleDelegate::Default());
  BubbleReference bubble2 = manager_->ShowBubble(MockBubbleDelegate::Default());

  bubble1->CloseBubble(BUBBLE_CLOSE_ACCEPTED);
  bubble2->CloseBubble(BUBBLE_CLOSE_CANCELED);

  ASSERT_FALSE(bubble1);
  ASSERT_FALSE(bubble2);
}

}  // namespace
