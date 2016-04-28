// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chrome_browser_main_extra_parts_metrics.h"

#include <memory>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/test/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/screen.h"
#include "ui/events/test/device_data_manager_test_api.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/test/test_screen.h"

namespace {

const char kTouchEventsEnabledHistogramName[] =
    "Touchscreen.TouchEventsEnabled";

}  // namespace

class ChromeBrowserMainExtraPartsMetricsTest : public testing::Test {
 public:
  ChromeBrowserMainExtraPartsMetricsTest();
  ~ChromeBrowserMainExtraPartsMetricsTest() override;

 protected:
  // Test API wrapping |device_data_manager_|.
  ui::test::DeviceDataManagerTestAPI device_data_manager_test_api_;

 private:
  // Required by a ChromeBrowserMainExtraPartsMetrics test target.
  base::MessageLoop message_loop_;

  // Dummy screen required by a ChromeBrowserMainExtraPartsMetrics test target.
  gfx::test::TestScreen test_screen_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserMainExtraPartsMetricsTest);
};

ChromeBrowserMainExtraPartsMetricsTest::ChromeBrowserMainExtraPartsMetricsTest()
    : device_data_manager_test_api_() {
  display::Screen::SetScreenInstance(&test_screen_);
}

ChromeBrowserMainExtraPartsMetricsTest::
    ~ChromeBrowserMainExtraPartsMetricsTest() {
  display::Screen::SetScreenInstance(nullptr);
}

// Verify a TouchEventsEnabled value isn't recorded during construction.
TEST_F(ChromeBrowserMainExtraPartsMetricsTest,
       VerifyTouchEventsEnabledIsNotRecordedAfterConstruction) {
  base::HistogramTester histogram_tester;
  ChromeBrowserMainExtraPartsMetrics test_target;
  histogram_tester.ExpectTotalCount(kTouchEventsEnabledHistogramName, 0);
}

#if defined(USE_OZONE) || defined(USE_X11)

// Verify a TouchEventsEnabled value isn't recorded during PostBrowserStart if
// the device scan hasn't completed yet.
TEST_F(ChromeBrowserMainExtraPartsMetricsTest,
       VerifyTouchEventsEnabledIsNotRecordedAfterPostBrowserStart) {
  base::HistogramTester histogram_tester;

  ChromeBrowserMainExtraPartsMetrics test_target;

  test_target.PostBrowserStart();
  histogram_tester.ExpectTotalCount(kTouchEventsEnabledHistogramName, 0);
}

// Verify a TouchEventsEnabled value is recorded during PostBrowserStart if the
// device scan has already completed.
TEST_F(ChromeBrowserMainExtraPartsMetricsTest,
       VerifyTouchEventsEnabledIsRecordedAfterPostBrowserStart) {
  base::HistogramTester histogram_tester;

  device_data_manager_test_api_.OnDeviceListsComplete();

  ChromeBrowserMainExtraPartsMetrics test_target;

  test_target.PostBrowserStart();
  histogram_tester.ExpectTotalCount(kTouchEventsEnabledHistogramName, 1);
}

// Verify a TouchEventsEnabled value is recorded when an asynchronous device
// scan completes.
TEST_F(ChromeBrowserMainExtraPartsMetricsTest,
       VerifyTouchEventsEnabledIsRecordedWhenDeviceListsComplete) {
  base::HistogramTester histogram_tester;
  ChromeBrowserMainExtraPartsMetrics test_target;

  test_target.PostBrowserStart();
  device_data_manager_test_api_.NotifyObserversDeviceListsComplete();
  histogram_tester.ExpectTotalCount(kTouchEventsEnabledHistogramName, 1);
}

// Verify a TouchEventsEnabled value is only recorded once if multiple
// asynchronous device scans happen.
TEST_F(ChromeBrowserMainExtraPartsMetricsTest,
       VerifyTouchEventsEnabledIsOnlyRecordedOnce) {
  base::HistogramTester histogram_tester;
  ChromeBrowserMainExtraPartsMetrics test_target;

  test_target.PostBrowserStart();
  device_data_manager_test_api_.NotifyObserversDeviceListsComplete();
  device_data_manager_test_api_.NotifyObserversDeviceListsComplete();
  histogram_tester.ExpectTotalCount(kTouchEventsEnabledHistogramName, 1);
}

#else

// Verify a TouchEventsEnabled value is recorded during PostBrowserStart.
TEST_F(ChromeBrowserMainExtraPartsMetricsTest,
       VerifyTouchEventsEnabledIsRecordedAfterPostBrowserStart) {
  base::HistogramTester histogram_tester;
  ChromeBrowserMainExtraPartsMetrics test_target;

  test_target.PostBrowserStart();
  histogram_tester.ExpectTotalCount(kTouchEventsEnabledHistogramName, 1);
}

#endif  // defined(USE_OZONE) || defined(USE_X11)
