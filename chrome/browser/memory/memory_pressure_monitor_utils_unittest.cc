// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory/memory_pressure_monitor_utils.h"

#include "base/test/scoped_task_environment.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace memory {
namespace {

constexpr base::TimeDelta kDefaultWindowLength =
    base::TimeDelta::FromMinutes(1);

}  // namespace

// Test class used to expose some protected members from
// internal::ObservationWindow and to simplify the tests by removing the
// template argument.
class TestObservationWindow : public internal::ObservationWindow<int> {
 public:
  TestObservationWindow()
      : internal::ObservationWindow<int>(kDefaultWindowLength) {}
  ~TestObservationWindow() override = default;

  using internal::ObservationWindow<int>::observations_for_testing;
  using internal::ObservationWindow<int>::set_clock_for_testing;

  bool Empty() { return observations_for_testing().empty(); }

  MOCK_METHOD1(OnSampleAdded, void(const int& sample));
  MOCK_METHOD1(OnSampleRemoved, void(const int& sample));
};

using ObservationWindowTest = testing::Test;

TEST_F(ObservationWindowTest, OnSample) {
  base::test::ScopedTaskEnvironment scoped_task_environment(
      base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME);

  ::testing::StrictMock<TestObservationWindow> window;

  typedef decltype(window.observations_for_testing()) ObservationContainerType;

  const auto* tick_clock = scoped_task_environment.GetMockTickClock();
  window.set_clock_for_testing(tick_clock);

  // The window is expected to be empty by default.
  EXPECT_TRUE(window.Empty());

  // Add a first sample.
  int t0_sample = 1;
  base::TimeTicks t0_timestamp = tick_clock->NowTicks();

  EXPECT_CALL(window, OnSampleAdded(t0_sample)).Times(1);
  window.OnSample(t0_sample);
  ::testing::Mock::VerifyAndClear(&window);

  // decltype is used here to avoid having to expose the internal datatype used
  // to represent this window.
  EXPECT_THAT(window.observations_for_testing(),
              ::testing::ContainerEq(ObservationContainerType{
                  {t0_timestamp, t0_sample},
              }));

  // Fast forward by the length of the observation window, no sample should be
  // removed as all samples have an age that doesn't exceed the window length.
  scoped_task_environment.FastForwardBy(kDefaultWindowLength);

  int t1_sample = 2;
  base::TimeTicks t1_timestamp = tick_clock->NowTicks();

  EXPECT_CALL(window, OnSampleAdded(t1_sample)).Times(1);
  window.OnSample(t1_sample);
  ::testing::Mock::VerifyAndClear(&window);

  EXPECT_THAT(window.observations_for_testing(),
              ::testing::ContainerEq(ObservationContainerType{
                  {t0_timestamp, t0_sample},
                  {t1_timestamp, t1_sample},
              }));

  // Fast forward by one second, the first sample should be removed the next
  // time a sample gets added as its age exceed the length of the observation
  // window.
  scoped_task_environment.FastForwardBy(base::TimeDelta::FromSeconds(1));

  int t2_sample = 3;
  base::TimeTicks t2_timestamp = tick_clock->NowTicks();

  EXPECT_CALL(window, OnSampleAdded(t2_sample)).Times(1);
  EXPECT_CALL(window, OnSampleRemoved(t0_sample)).Times(1);
  window.OnSample(t2_sample);
  ::testing::Mock::VerifyAndClear(&window);

  EXPECT_THAT(window.observations_for_testing(),
              ::testing::ContainerEq(ObservationContainerType{
                  {t1_timestamp, t1_sample},
                  {t2_timestamp, t2_sample},
              }));
}

}  // namespace memory
