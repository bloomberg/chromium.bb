// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/firebase_utils.h"

#import <Foundation/Foundation.h>

#include "base/test/metrics/histogram_tester.h"
#import "ios/chrome/app/firebase_buildflags.h"
#if BUILDFLAG(FIREBASE_ENABLED)
#import "ios/third_party/firebase/Analytics/FirebaseCore.framework/Headers/FIRApp.h"
#endif  // BUILDFLAG(FIREBASE_ENABLED)
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class FirebaseUtilsTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
#if BUILDFLAG(FIREBASE_ENABLED)
    firapp_ = OCMClassMock([FIRApp class]);
#endif  // BUILDFLAG(FIREBASE_ENABLED)
  }
  void TearDown() override {
    [firapp_ stopMocking];
    PlatformTest::TearDown();
  }

 protected:
  id firapp_;
  base::HistogramTester histogram_tester_;
};

#if BUILDFLAG(FIREBASE_ENABLED)

TEST_F(FirebaseUtilsTest, EnabledInitializedHistogramFirstRun) {
  [[firapp_ expect] configure];
  InitializeFirebase(true);
  histogram_tester_.ExpectUniqueSample(
      kFirebaseConfiguredHistogramName,
      FirebaseConfiguredState::kEnabledFirstRun, 1);
  EXPECT_OCMOCK_VERIFY(firapp_);
}

TEST_F(FirebaseUtilsTest, EnabledInitializedHistogramNotFirstRun) {
  [[firapp_ expect] configure];
  InitializeFirebase(false);
  histogram_tester_.ExpectUniqueSample(
      kFirebaseConfiguredHistogramName,
      FirebaseConfiguredState::kEnabledNotFirstRun, 1);
  EXPECT_OCMOCK_VERIFY(firapp_);
}

#else

TEST_F(FirebaseUtilsTest, DisabledInitializedHistogram) {
  // FIRApp class should not exist if Firebase is not enabled.
  ASSERT_EQ(nil, NSClassFromString(@"FIRApp"));
  InitializeFirebase(false);
  histogram_tester_.ExpectUniqueSample(kFirebaseConfiguredHistogramName,
                                       FirebaseConfiguredState::kDisabled, 1);
}

#endif  // BUILDFLAG(FIREBASE_ENABLED)
