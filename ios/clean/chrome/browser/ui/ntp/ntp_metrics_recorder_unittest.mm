// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/ntp/ntp_metrics_recorder.h"

#import <Foundation/Foundation.h>
#include <memory>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/test/histogram_tester.h"
#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/metrics/metrics_test_util.h"
#import "ios/clean/chrome/browser/ui/commands/ntp_commands.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for the NTPMetricsRecorder unit tests.
class NtpMetricsRecorderTest : public PlatformTest {
 protected:
  NtpMetricsRecorderTest()
      : recorder_([[NTPMetricsRecorder alloc] initWithPrefService:&prefs_]) {}

  void SetUp() override {
    prefs_.registry()->RegisterBooleanPref(prefs::kSearchSuggestEnabled, true);
  }

  // Reports logging of histogram information.
  base::HistogramTester histogram_tester_;
  // Test PrefService.
  TestingPrefServiceSimple prefs_;
  // NTPMetricsRecorder under test.
  __strong NTPMetricsRecorder* recorder_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NtpMetricsRecorderTest);
};

// Tests that NTPMetricsRecorder logs the correct histogram bucket when
// Search Suggest is enabled and |recordMetricForInvocation:| is called.
TEST_F(NtpMetricsRecorderTest, ShowNtpHomePanelSuggestEnabled) {
  prefs_.SetUserPref(prefs::kSearchSuggestEnabled,
                     std::make_unique<base::Value>(true));

  NSInvocation* invocation = GetInvocationForProtocolInstanceMethod(
      @protocol(NTPCommands), @selector(showNTPHomePanel), YES);
  [recorder_ recordMetricForInvocation:invocation];

  histogram_tester_.ExpectUniqueSample("IOS.NTP.Impression",
                                       ntp_home::REMOTE_SUGGESTIONS, 1);
}

// Tests that NTPMetricsRecorder logs the correct histogram bucket when
// Search Suggest is disabled and |recordMetricForInvocation:| is called.
TEST_F(NtpMetricsRecorderTest, ShowNtpHomePanelSuggestDisabled) {
  prefs_.SetUserPref(prefs::kSearchSuggestEnabled,
                     std::make_unique<base::Value>(false));

  NSInvocation* invocation = GetInvocationForProtocolInstanceMethod(
      @protocol(NTPCommands), @selector(showNTPHomePanel), YES);
  [recorder_ recordMetricForInvocation:invocation];

  histogram_tester_.ExpectUniqueSample("IOS.NTP.Impression",
                                       ntp_home::LOCAL_SUGGESTIONS, 1);
}
