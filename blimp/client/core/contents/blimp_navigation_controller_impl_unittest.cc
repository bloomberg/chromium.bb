// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "blimp/client/core/contents/blimp_navigation_controller_delegate.h"
#include "blimp/client/core/contents/blimp_navigation_controller_impl.h"
#include "blimp/client/core/contents/fake_navigation_feature.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace blimp {
namespace client {
namespace {

const GURL kExampleURL = GURL("https://www.example.com/");

class MockBlimpNavigationControllerDelegate
    : public BlimpNavigationControllerDelegate {
 public:
  MockBlimpNavigationControllerDelegate() = default;
  ~MockBlimpNavigationControllerDelegate() override = default;

  MOCK_METHOD0(OnNavigationStateChanged, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockBlimpNavigationControllerDelegate);
};

TEST(BlimpNavigationControllerImplTest, BackForwardNavigation) {
  base::MessageLoop loop;

  testing::StrictMock<MockBlimpNavigationControllerDelegate> delegate;
  testing::StrictMock<FakeNavigationFeature> feature;
  BlimpNavigationControllerImpl navigation_controller(&delegate, &feature);
  feature.SetDelegate(1, &navigation_controller);

  EXPECT_CALL(delegate, OnNavigationStateChanged());

  navigation_controller.LoadURL(kExampleURL);
  loop.RunUntilIdle();
  EXPECT_EQ(kExampleURL, navigation_controller.GetURL());

  EXPECT_CALL(feature, GoBack(_));
  EXPECT_CALL(feature, GoForward(_));
  EXPECT_CALL(feature, Reload(_));

  navigation_controller.GoBack();
  navigation_controller.GoForward();
  navigation_controller.Reload();

  base::RunLoop().RunUntilIdle();
}

}  // namespace
}  // namespace client
}  // namespace blimp
