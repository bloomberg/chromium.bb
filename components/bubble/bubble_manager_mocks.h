// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BUBBLE_BUBBLE_MANAGER_MOCKS_H_
#define COMPONENTS_BUBBLE_BUBBLE_MANAGER_MOCKS_H_

#include "components/bubble/bubble_delegate.h"
#include "components/bubble/bubble_reference.h"
#include "components/bubble/bubble_ui.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockBubbleUi : public BubbleUi {
 public:
  MockBubbleUi();
  ~MockBubbleUi() override;

  MOCK_METHOD1(Show, void(BubbleReference));
  MOCK_METHOD0(Close, void());
  MOCK_METHOD0(UpdateAnchorPosition, void());

  // To verify destructor call.
  MOCK_METHOD0(Destroyed, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockBubbleUi);
};

class MockBubbleDelegate : public BubbleDelegate {
 public:
  MockBubbleDelegate();
  ~MockBubbleDelegate() override;

  // Default bubble shows UI and closes when asked to close.
  static scoped_ptr<MockBubbleDelegate> Default();

  // Stubborn bubble shows UI and doesn't want to close.
  static scoped_ptr<MockBubbleDelegate> Stubborn();

  MOCK_METHOD1(ShouldClose, bool(BubbleCloseReason reason));

  // A scoped_ptr can't be returned in MOCK_METHOD.
  MOCK_METHOD0(BuildBubbleUiMock, BubbleUi*());
  scoped_ptr<BubbleUi> BuildBubbleUi() override {
    return make_scoped_ptr(BuildBubbleUiMock());
  }

  MOCK_METHOD1(UpdateBubbleUi, bool(BubbleUi*));

  std::string GetName() const override { return "MockBubble"; }

  // To verify destructor call.
  MOCK_METHOD0(Destroyed, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockBubbleDelegate);
};

#endif  // COMPONENTS_BUBBLE_BUBBLE_MANAGER_MOCKS_H_
