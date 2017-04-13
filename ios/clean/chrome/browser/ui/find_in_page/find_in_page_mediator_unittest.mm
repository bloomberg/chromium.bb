// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/find_in_page/find_in_page_mediator.h"

#include "base/memory/ptr_util.h"
#include "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#include "ios/chrome/browser/web_state_list/web_state_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class FindInPageMediatorTest : public PlatformTest {
 public:
  FindInPageMediatorTest() {
    web_state_list_ = base::MakeUnique<WebStateList>(&web_state_list_delegate_);
    provider_ = [OCMockObject
        niceMockForProtocol:@protocol(FindInPageConsumerProvider)];
  }

 protected:
  std::unique_ptr<WebStateList> web_state_list_;
  FakeWebStateListDelegate web_state_list_delegate_;
  id provider_;
};

}  // namespace

TEST_F(FindInPageMediatorTest, Init) {
  FindInPageMediator* mediator =
      [[FindInPageMediator alloc] initWithWebStateList:web_state_list_.get()
                                              provider:provider_
                                            dispatcher:nil];
  [mediator self];
}
