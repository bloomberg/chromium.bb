// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/contents/blimp_contents_impl.h"

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "blimp/client/core/contents/fake_navigation_feature.h"
#include "blimp/client/core/contents/tab_control_feature.h"
#include "blimp/client/public/contents/blimp_contents_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blimp {
namespace client {
namespace {

const char kExampleURL[] = "https://www.example.com/";
const char kOtherExampleURL[] = "https://www.otherexample.com/";
const int kDummyTabId = 0;

class MockBlimpContentsObserver : public BlimpContentsObserver {
 public:
  explicit MockBlimpContentsObserver(BlimpContents* blimp_contents)
      : BlimpContentsObserver(blimp_contents) {}
  ~MockBlimpContentsObserver() override = default;

  MOCK_METHOD0(OnNavigationStateChanged, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockBlimpContentsObserver);
};

class MockTabControlFeature : public TabControlFeature {
 public:
  MockTabControlFeature() {}
  ~MockTabControlFeature() override = default;
  MOCK_METHOD2(SetSizeAndScale, void(const gfx::Size&, float));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockTabControlFeature);
};

TEST(BlimpContentsImplTest, LoadURLAndNotifyObservers) {
  base::MessageLoop loop;
  BlimpContentsImpl blimp_contents(kDummyTabId, nullptr);

  BlimpNavigationControllerImpl& navigation_controller =
      blimp_contents.GetNavigationController();
  FakeNavigationFeature feature;
  feature.SetDelegate(1, &navigation_controller);
  navigation_controller.SetNavigationFeatureForTesting(&feature);

  testing::StrictMock<MockBlimpContentsObserver> observer1(&blimp_contents);
  testing::StrictMock<MockBlimpContentsObserver> observer2(&blimp_contents);

  EXPECT_CALL(observer1, OnNavigationStateChanged());
  EXPECT_CALL(observer2, OnNavigationStateChanged()).Times(2);

  navigation_controller.LoadURL(GURL(kExampleURL));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(kExampleURL, navigation_controller.GetURL().spec());

  // Observer should no longer receive callbacks.
  blimp_contents.RemoveObserver(&observer1);

  navigation_controller.LoadURL(GURL(kOtherExampleURL));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(kOtherExampleURL, navigation_controller.GetURL().spec());
}

TEST(BlimpContentsImplTest, SetSizeAndScaleThroughTabControlFeature) {
  int width = 10;
  int height = 15;
  float dp_to_px = 1.23f;

  MockTabControlFeature tab_control_feature;
  base::MessageLoop loop;
  BlimpContentsImpl blimp_contents(kDummyTabId, &tab_control_feature);

  EXPECT_CALL(tab_control_feature,
              SetSizeAndScale(gfx::Size(width, height), dp_to_px)).Times(1);

  blimp_contents.SetSizeAndScale(gfx::Size(width, height), dp_to_px);
}

}  // namespace
}  // namespace client
}  // namespace blimp
