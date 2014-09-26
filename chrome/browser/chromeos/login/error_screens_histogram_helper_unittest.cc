// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/error_screens_histogram_helper.h"

#include "base/metrics/statistics_recorder.h"
#include "base/test/histogram_tester.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

class ErrorScreensHistogramHelperTest : public testing::Test {
 public:
  virtual void SetUp() OVERRIDE {
    helper_.reset(new ErrorScreensHistogramHelper("TestScreen"));
    second_helper_.reset(new ErrorScreensHistogramHelper("TestScreen2"));
  }

  content::TestBrowserThreadBundle thread_bundle_;
  base::HistogramTester histograms_;
  scoped_ptr<ErrorScreensHistogramHelper> helper_;
  scoped_ptr<ErrorScreensHistogramHelper> second_helper_;
};

// No errors when screen was not shown.
TEST_F(ErrorScreensHistogramHelperTest, DoesNotShowScreen) {
  helper_.reset();
  histograms_.ExpectTotalCount("OOBE.NetworkErrorShown.TestScreen", 0);
}

// No errors when screen was shown and error was not.
TEST_F(ErrorScreensHistogramHelperTest, ShowScreenWithoutError) {
  helper_->OnScreenShow();
  helper_.reset();
  second_helper_->OnScreenShow();
  second_helper_.reset();
  histograms_.ExpectUniqueSample(
      "OOBE.NetworkErrorShown.TestScreen", ErrorScreen::ERROR_STATE_NONE, 1);
  histograms_.ExpectUniqueSample(
      "OOBE.NetworkErrorShown.TestScreen2", ErrorScreen::ERROR_STATE_NONE, 1);
}

// Show 3 offline errors and 1 portal error. Make sure in time histograms logged
// portal error only.
TEST_F(ErrorScreensHistogramHelperTest, ShowScreenAndError) {
  helper_->OnScreenShow();
  second_helper_->OnScreenShow();
  helper_->OnErrorShow(ErrorScreen::ERROR_STATE_OFFLINE);
  second_helper_->OnErrorShow(ErrorScreen::ERROR_STATE_PORTAL);
  helper_->OnErrorShow(ErrorScreen::ERROR_STATE_OFFLINE);
  helper_->OnErrorHide();
  second_helper_->OnErrorHide();
  helper_->OnErrorShow(ErrorScreen::ERROR_STATE_OFFLINE);
  histograms_.ExpectUniqueSample(
      "OOBE.NetworkErrorShown.TestScreen", ErrorScreen::ERROR_STATE_OFFLINE, 3);
  histograms_.ExpectUniqueSample(
      "OOBE.NetworkErrorShown.TestScreen2", ErrorScreen::ERROR_STATE_PORTAL, 1);
  helper_->OnErrorShow(ErrorScreen::ERROR_STATE_PORTAL);
  histograms_.ExpectBucketCount(
      "OOBE.NetworkErrorShown.TestScreen", ErrorScreen::ERROR_STATE_PORTAL, 1);
  histograms_.ExpectTotalCount("OOBE.ErrorScreensTime.TestScreen.Portal", 0);
  helper_.reset();
  histograms_.ExpectTotalCount("OOBE.ErrorScreensTime.TestScreen.Portal", 1);
}

// Show error and hide it after 1 sec.
TEST_F(ErrorScreensHistogramHelperTest, TestShowHideTime) {
  helper_->OnScreenShow();
  second_helper_->OnScreenShow();
  base::Time now = base::Time::Now();
  helper_->OnErrorShowTime(ErrorScreen::ERROR_STATE_PORTAL, now);
  now += base::TimeDelta::FromMilliseconds(1000);
  helper_->OnErrorHideTime(now);
  helper_.reset();
  histograms_.ExpectUniqueSample(
      "OOBE.ErrorScreensTime.TestScreen.Portal", 1000, 1);
}

// Show, hide, show, hide error with 1 sec interval. Make sure time logged in
// histogram is 2 sec.
TEST_F(ErrorScreensHistogramHelperTest, TestShowHideShowHideTime) {
  helper_->OnScreenShow();
  second_helper_->OnScreenShow();
  base::Time now = base::Time::Now();
  helper_->OnErrorShowTime(ErrorScreen::ERROR_STATE_PROXY, now);
  now += base::TimeDelta::FromMilliseconds(1000);
  helper_->OnErrorHideTime(now);
  now += base::TimeDelta::FromMilliseconds(1000);
  helper_->OnErrorShowTime(ErrorScreen::ERROR_STATE_PORTAL, now);
  now += base::TimeDelta::FromMilliseconds(1000);
  helper_->OnErrorHideTime(now);
  helper_.reset();
  histograms_.ExpectUniqueSample(
      "OOBE.ErrorScreensTime.TestScreen.Portal", 2000, 1);
}

// Show, show, hide error with 1 sec interval. Make sure time logged in
// histogram is 2 sec.
TEST_F(ErrorScreensHistogramHelperTest, TestShowShowHideTime) {
  helper_->OnScreenShow();
  second_helper_->OnScreenShow();
  base::Time now = base::Time::Now();
  helper_->OnErrorShowTime(ErrorScreen::ERROR_STATE_PROXY, now);
  now += base::TimeDelta::FromMilliseconds(1000);
  helper_->OnErrorShowTime(ErrorScreen::ERROR_STATE_PORTAL, now);
  now += base::TimeDelta::FromMilliseconds(1000);
  helper_->OnErrorHideTime(now);
  helper_.reset();
  histograms_.ExpectUniqueSample(
      "OOBE.ErrorScreensTime.TestScreen.Portal", 2000, 1);
}

}  // namespace chromeos
