// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bubble/bubble_manager.h"

#include "components/bubble/bubble_controller.h"
#include "components/bubble/bubble_manager_mocks.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Helper class used to test chaining another bubble.
class DelegateChainHelper {
 public:
  DelegateChainHelper(BubbleManager* manager,
                      scoped_ptr<BubbleDelegate> next_delegate);

  // Will show the bubble in |next_delegate_|.
  void Chain() { manager_->ShowBubble(next_delegate_.Pass()); }

  // True if the bubble was taken by the bubble manager.
  bool BubbleWasTaken() { return !next_delegate_; }

 private:
  BubbleManager* manager_;  // Weak.
  scoped_ptr<BubbleDelegate> next_delegate_;

  DISALLOW_COPY_AND_ASSIGN(DelegateChainHelper);
};

DelegateChainHelper::DelegateChainHelper(
    BubbleManager* manager,
    scoped_ptr<BubbleDelegate> next_delegate)
    : manager_(manager), next_delegate_(next_delegate.Pass()) {}

class MockBubbleManagerObserver : public BubbleManager::BubbleManagerObserver {
 public:
  MockBubbleManagerObserver() {}
  ~MockBubbleManagerObserver() override {}

  MOCK_METHOD1(OnBubbleNeverShown, void(BubbleReference));
  MOCK_METHOD2(OnBubbleClosed, void(BubbleReference, BubbleCloseReason));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockBubbleManagerObserver);
};

class BubbleManagerTest : public testing::Test {
 public:
  BubbleManagerTest();
  ~BubbleManagerTest() override {}

  void SetUp() override;
  void TearDown() override;

 protected:
  scoped_ptr<BubbleManager> manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BubbleManagerTest);
};

BubbleManagerTest::BubbleManagerTest() {}

void BubbleManagerTest::SetUp() {
  testing::Test::SetUp();
  manager_.reset(new BubbleManager);
}

void BubbleManagerTest::TearDown() {
  manager_.reset();
  testing::Test::TearDown();
}

TEST_F(BubbleManagerTest, ManagerShowsBubbleUi) {
  scoped_ptr<MockBubbleDelegate> delegate = MockBubbleDelegate::Default();

  MockBubbleUi* bubble_ui = delegate->bubble_ui();
  EXPECT_CALL(*bubble_ui, Destroyed());
  EXPECT_CALL(*bubble_ui, Show(testing::_));
  EXPECT_CALL(*bubble_ui, Close());
  EXPECT_CALL(*bubble_ui, UpdateAnchorPosition()).Times(0);

  manager_->ShowBubble(delegate.Pass());
}

TEST_F(BubbleManagerTest, ManagerUpdatesBubbleUiAnchor) {
  scoped_ptr<MockBubbleDelegate> delegate = MockBubbleDelegate::Default();

  MockBubbleUi* bubble_ui = delegate->bubble_ui();
  EXPECT_CALL(*bubble_ui, Destroyed());
  EXPECT_CALL(*bubble_ui, Show(testing::_));
  EXPECT_CALL(*bubble_ui, Close());
  EXPECT_CALL(*bubble_ui, UpdateAnchorPosition());

  manager_->ShowBubble(delegate.Pass());
  manager_->UpdateAllBubbleAnchors();
}

TEST_F(BubbleManagerTest, CloseOnReferenceInvalidatesReference) {
  BubbleReference ref = manager_->ShowBubble(MockBubbleDelegate::Default());

  ASSERT_TRUE(ref->CloseBubble(BUBBLE_CLOSE_FOCUS_LOST));

  ASSERT_FALSE(ref);
}

TEST_F(BubbleManagerTest, CloseOnStubbornReferenceDoesNotInvalidate) {
  BubbleReference ref = manager_->ShowBubble(MockBubbleDelegate::Stubborn());

  ASSERT_FALSE(ref->CloseBubble(BUBBLE_CLOSE_FOCUS_LOST));

  ASSERT_TRUE(ref);
}

TEST_F(BubbleManagerTest, CloseInvalidatesReference) {
  BubbleReference ref = manager_->ShowBubble(MockBubbleDelegate::Default());

  ASSERT_TRUE(manager_->CloseBubble(ref, BUBBLE_CLOSE_FOCUS_LOST));

  ASSERT_FALSE(ref);
}

TEST_F(BubbleManagerTest, CloseAllInvalidatesReference) {
  BubbleReference ref = manager_->ShowBubble(MockBubbleDelegate::Default());

  manager_->CloseAllBubbles(BUBBLE_CLOSE_FOCUS_LOST);

  ASSERT_FALSE(ref);
}

TEST_F(BubbleManagerTest, DestroyInvalidatesReference) {
  BubbleReference ref = manager_->ShowBubble(MockBubbleDelegate::Default());

  manager_.reset();

  ASSERT_FALSE(ref);
}

TEST_F(BubbleManagerTest, CloseInvalidatesStubbornReference) {
  BubbleReference ref = manager_->ShowBubble(MockBubbleDelegate::Stubborn());

  ASSERT_TRUE(manager_->CloseBubble(ref, BUBBLE_CLOSE_FORCED));

  ASSERT_FALSE(ref);
}

TEST_F(BubbleManagerTest, CloseAllInvalidatesStubbornReference) {
  BubbleReference ref = manager_->ShowBubble(MockBubbleDelegate::Stubborn());

  manager_->CloseAllBubbles(BUBBLE_CLOSE_FORCED);

  ASSERT_FALSE(ref);
}

TEST_F(BubbleManagerTest, DestroyInvalidatesStubbornReference) {
  BubbleReference ref = manager_->ShowBubble(MockBubbleDelegate::Stubborn());

  manager_.reset();

  ASSERT_FALSE(ref);
}

TEST_F(BubbleManagerTest, CloseDoesNotInvalidateStubbornReference) {
  BubbleReference ref = manager_->ShowBubble(MockBubbleDelegate::Stubborn());

  ASSERT_FALSE(manager_->CloseBubble(ref, BUBBLE_CLOSE_FOCUS_LOST));

  ASSERT_TRUE(ref);
}

TEST_F(BubbleManagerTest, CloseAllDoesNotInvalidateStubbornReference) {
  BubbleReference ref = manager_->ShowBubble(MockBubbleDelegate::Stubborn());

  manager_->CloseAllBubbles(BUBBLE_CLOSE_FOCUS_LOST);

  ASSERT_TRUE(ref);
}

TEST_F(BubbleManagerTest, CloseAllInvalidatesMixAppropriately) {
  BubbleReference stubborn_ref1 =
      manager_->ShowBubble(MockBubbleDelegate::Stubborn());
  BubbleReference normal_ref1 =
      manager_->ShowBubble(MockBubbleDelegate::Default());
  BubbleReference stubborn_ref2 =
      manager_->ShowBubble(MockBubbleDelegate::Stubborn());
  BubbleReference normal_ref2 =
      manager_->ShowBubble(MockBubbleDelegate::Default());
  BubbleReference stubborn_ref3 =
      manager_->ShowBubble(MockBubbleDelegate::Stubborn());
  BubbleReference normal_ref3 =
      manager_->ShowBubble(MockBubbleDelegate::Default());

  manager_->CloseAllBubbles(BUBBLE_CLOSE_FOCUS_LOST);

  ASSERT_TRUE(stubborn_ref1);
  ASSERT_TRUE(stubborn_ref2);
  ASSERT_TRUE(stubborn_ref3);
  ASSERT_FALSE(normal_ref1);
  ASSERT_FALSE(normal_ref2);
  ASSERT_FALSE(normal_ref3);
}

TEST_F(BubbleManagerTest, UpdateAllShouldWorkWithoutBubbles) {
  // Manager shouldn't crash if bubbles have never been added.
  manager_->UpdateAllBubbleAnchors();

  // Add a bubble and close it.
  BubbleReference ref = manager_->ShowBubble(MockBubbleDelegate::Default());
  ASSERT_TRUE(manager_->CloseBubble(ref, BUBBLE_CLOSE_FORCED));

  // Bubble should NOT get an update event because it's already closed.
  manager_->UpdateAllBubbleAnchors();
}

TEST_F(BubbleManagerTest, CloseAllShouldWorkWithoutBubbles) {
  // Manager shouldn't crash if bubbles have never been added.
  manager_->CloseAllBubbles(BUBBLE_CLOSE_FOCUS_LOST);

  // Add a bubble and close it.
  BubbleReference ref = manager_->ShowBubble(MockBubbleDelegate::Default());
  ASSERT_TRUE(manager_->CloseBubble(ref, BUBBLE_CLOSE_FORCED));

  // Bubble should NOT get a close event because it's already closed.
  manager_->CloseAllBubbles(BUBBLE_CLOSE_FOCUS_LOST);
}

TEST_F(BubbleManagerTest, AllowBubbleChainingOnClose) {
  scoped_ptr<BubbleDelegate> chained_delegate = MockBubbleDelegate::Default();
  DelegateChainHelper chain_helper(manager_.get(), chained_delegate.Pass());

  // Manager will delete |delegate|.
  MockBubbleDelegate* delegate = new MockBubbleDelegate;
  EXPECT_CALL(*delegate, ShouldClose(testing::_))
      .WillOnce(testing::DoAll(testing::InvokeWithoutArgs(
                                   &chain_helper, &DelegateChainHelper::Chain),
                               testing::Return(true)));

  BubbleReference ref = manager_->ShowBubble(make_scoped_ptr(delegate));
  ASSERT_TRUE(manager_->CloseBubble(ref, BUBBLE_CLOSE_FORCED));

  ASSERT_TRUE(chain_helper.BubbleWasTaken());
}

TEST_F(BubbleManagerTest, AllowBubbleChainingOnCloseAll) {
  scoped_ptr<BubbleDelegate> chained_delegate = MockBubbleDelegate::Default();
  DelegateChainHelper chain_helper(manager_.get(), chained_delegate.Pass());

  // Manager will delete |delegate|.
  MockBubbleDelegate* delegate = new MockBubbleDelegate;
  EXPECT_CALL(*delegate, ShouldClose(testing::_))
      .WillOnce(testing::DoAll(testing::InvokeWithoutArgs(
                                   &chain_helper, &DelegateChainHelper::Chain),
                               testing::Return(true)));

  manager_->ShowBubble(make_scoped_ptr(delegate));
  manager_->CloseAllBubbles(BUBBLE_CLOSE_FORCED);

  ASSERT_TRUE(chain_helper.BubbleWasTaken());
}

TEST_F(BubbleManagerTest, BubblesDoNotChainOnDestroy) {
  // Manager will delete |chained_delegate|.
  MockBubbleDelegate* chained_delegate = new MockBubbleDelegate;
  EXPECT_CALL(*chained_delegate->bubble_ui(), Show(testing::_)).Times(0);
  EXPECT_CALL(*chained_delegate, ShouldClose(testing::_)).Times(0);

  DelegateChainHelper chain_helper(manager_.get(),
                                   make_scoped_ptr(chained_delegate));

  // Manager will delete |delegate|.
  MockBubbleDelegate* delegate = new MockBubbleDelegate;
  EXPECT_CALL(*delegate, ShouldClose(testing::_))
      .WillOnce(testing::DoAll(testing::InvokeWithoutArgs(
                                   &chain_helper, &DelegateChainHelper::Chain),
                               testing::Return(true)));

  manager_->ShowBubble(make_scoped_ptr(delegate));
  manager_.reset();

  // The manager will take the bubble, but not show it.
  ASSERT_TRUE(chain_helper.BubbleWasTaken());
}

TEST_F(BubbleManagerTest, BubbleCloseReasonIsCalled) {
  MockBubbleManagerObserver metrics;
  EXPECT_CALL(metrics, OnBubbleNeverShown(testing::_)).Times(0);
  EXPECT_CALL(metrics, OnBubbleClosed(testing::_, BUBBLE_CLOSE_ACCEPTED));
  manager_->AddBubbleManagerObserver(&metrics);

  BubbleReference ref = manager_->ShowBubble(MockBubbleDelegate::Default());
  ref->CloseBubble(BUBBLE_CLOSE_ACCEPTED);

  // Destroy to verify no events are sent to |metrics| in destructor.
  manager_.reset();
}

TEST_F(BubbleManagerTest, BubbleCloseNeverShownIsCalled) {
  MockBubbleManagerObserver metrics;
  // |chained_delegate| should never be shown.
  EXPECT_CALL(metrics, OnBubbleNeverShown(testing::_));
  // |delegate| should be forced to close when the manager is destroyed.
  EXPECT_CALL(metrics, OnBubbleClosed(testing::_, BUBBLE_CLOSE_FORCED));
  manager_->AddBubbleManagerObserver(&metrics);

  // Manager will delete |chained_delegate|.
  MockBubbleDelegate* chained_delegate = new MockBubbleDelegate;
  EXPECT_CALL(*chained_delegate->bubble_ui(), Show(testing::_)).Times(0);
  EXPECT_CALL(*chained_delegate, ShouldClose(testing::_)).Times(0);

  DelegateChainHelper chain_helper(manager_.get(),
                                   make_scoped_ptr(chained_delegate));

  // Manager will delete |delegate|.
  MockBubbleDelegate* delegate = new MockBubbleDelegate;
  EXPECT_CALL(*delegate, ShouldClose(testing::_))
      .WillOnce(testing::DoAll(testing::InvokeWithoutArgs(
                                   &chain_helper, &DelegateChainHelper::Chain),
                               testing::Return(true)));

  manager_->ShowBubble(make_scoped_ptr(delegate));
  manager_.reset();

  // The manager will take the bubble, but not show it.
  ASSERT_TRUE(chain_helper.BubbleWasTaken());
}

}  // namespace
