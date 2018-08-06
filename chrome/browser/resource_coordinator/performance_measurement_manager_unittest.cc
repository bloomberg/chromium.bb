// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/performance_measurement_manager.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/resource_coordinator/page_signal_receiver.h"
#include "chrome/browser/resource_coordinator/render_process_probe.h"
#include "chrome/browser/resource_coordinator/tab_helper.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "services/resource_coordinator/public/cpp/coordination_unit_id.h"
#include "services/resource_coordinator/public/cpp/resource_coordinator_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace resource_coordinator {

class LenientMockRenderProcessProbe : public RenderProcessProbe {
 public:
  MOCK_METHOD0(StartSingleGather, void());
};
using MockRenderProcessProbe =
    testing::StrictMock<LenientMockRenderProcessProbe>;

class PerformanceMeasurementManagerTest
    : public ChromeRenderViewHostTestHarness {
 public:
  std::unique_ptr<content::WebContents> CreateWebContents() {
    std::unique_ptr<content::WebContents> contents = CreateTestWebContents();
    ResourceCoordinatorTabHelper::CreateForWebContents(contents.get());
    return contents;
  }

  MockRenderProcessProbe& mock_render_process_probe() {
    return mock_render_process_probe_;
  }

 private:
  MockRenderProcessProbe mock_render_process_probe_;
};

TEST_F(PerformanceMeasurementManagerTest, StartMeasurementOnPageAlmostIdle) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kPageAlmostIdle);
  ASSERT_TRUE(resource_coordinator::IsPageAlmostIdleSignalEnabled());

  PageSignalReceiver psr;
  PerformanceMeasurementManager performance_measurement_manager(
      &psr, &mock_render_process_probe());

  auto contents = CreateWebContents();
  const CoordinationUnitID contents_id(CoordinationUnitType::kPage,
                                       CoordinationUnitID::RANDOM_ID);
  psr.AssociateCoordinationUnitIDWithWebContents(contents_id, contents.get());
  psr.SetNavigationID(contents.get(), 100);

  const CoordinationUnitID dummy_id(CoordinationUnitType::kPage,
                                    CoordinationUnitID::RANDOM_ID);
  // There should be no measurement when signaling an unknown CU ID.
  psr.NotifyPageAlmostIdle(PageNavigationIdentity{dummy_id, 100, ""});

  // There should be no measurement when signaling the "wrong" navigation ID.
  psr.NotifyPageAlmostIdle(PageNavigationIdentity{contents_id, 99, ""});

  // A measurement should happen on signaling the right navigation ID.
  EXPECT_CALL(mock_render_process_probe(), StartSingleGather());
  psr.NotifyPageAlmostIdle(PageNavigationIdentity{contents_id, 100, ""});
}

}  // namespace resource_coordinator
