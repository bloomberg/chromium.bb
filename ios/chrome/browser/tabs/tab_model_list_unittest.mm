// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/tab_model_list.h"

#import "base/mac/scoped_nsobject.h"
#include "base/memory/ptr_util.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/sessions/test_session_service.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class TabModelListTest : public PlatformTest {
 public:
  TabModelListTest() {
    TestChromeBrowserState::Builder test_cbs_builder;
    chrome_browser_state_ = test_cbs_builder.Build();
  }

  TabModel* CreateTabModel() {
    return [[TabModel alloc]
        initWithSessionWindow:nil
               sessionService:[[TestSessionService alloc] init]
                 browserState:chrome_browser_state_.get()];
  }

  NSArray<TabModel*>* RegisteredTabModels() {
    return GetTabModelsForChromeBrowserState(chrome_browser_state_.get());
  }

 private:
  web::TestWebThreadBundle thread_bundle_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;

  DISALLOW_COPY_AND_ASSIGN(TabModelListTest);
};

TEST_F(TabModelListTest, RegisterAndUnregister) {
  EXPECT_EQ([RegisteredTabModels() count], 0u);

  TabModel* tab_model = CreateTabModel();
  EXPECT_EQ([RegisteredTabModels() count], 1u);
  EXPECT_NE([RegisteredTabModels() indexOfObject:tab_model],
            static_cast<NSUInteger>(NSNotFound));

  [tab_model browserStateDestroyed];

  EXPECT_EQ([RegisteredTabModels() count], 0u);
}

TEST_F(TabModelListTest, SupportsMultipleTabModel) {
  EXPECT_EQ([RegisteredTabModels() count], 0u);

  TabModel* tab_model1 = CreateTabModel();
  EXPECT_EQ([RegisteredTabModels() count], 1u);
  EXPECT_NE([RegisteredTabModels() indexOfObject:tab_model1],
            static_cast<NSUInteger>(NSNotFound));

  TabModel* tab_model2 = CreateTabModel();
  EXPECT_EQ([RegisteredTabModels() count], 2u);
  EXPECT_NE([RegisteredTabModels() indexOfObject:tab_model2],
            static_cast<NSUInteger>(NSNotFound));

  [tab_model1 browserStateDestroyed];
  [tab_model2 browserStateDestroyed];

  EXPECT_EQ([RegisteredTabModels() count], 0u);
}
