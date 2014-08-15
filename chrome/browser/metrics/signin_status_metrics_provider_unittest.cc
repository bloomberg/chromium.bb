// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/signin_status_metrics_provider.h"

#include <string>

#include "base/files/file_path.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(SigninStatusMetricsProvider, UpdateInitialSigninStatus) {
  SigninStatusMetricsProvider metrics_provider(true);
  metrics_provider.UpdateInitialSigninStatus(2, 2);
  EXPECT_EQ(SigninStatusMetricsProvider::ALL_PROFILES_SIGNED_IN,
            metrics_provider.GetSigninStatusForTesting());
  metrics_provider.UpdateInitialSigninStatus(2, 0);
  EXPECT_EQ(SigninStatusMetricsProvider::ALL_PROFILES_NOT_SIGNED_IN,
            metrics_provider.GetSigninStatusForTesting());
  metrics_provider.UpdateInitialSigninStatus(2, 1);
  EXPECT_EQ(SigninStatusMetricsProvider::MIXED_SIGNIN_STATUS,
            metrics_provider.GetSigninStatusForTesting());
  metrics_provider.UpdateInitialSigninStatus(0, 0);
  EXPECT_EQ(SigninStatusMetricsProvider::UNKNOWN_SIGNIN_STATUS,
            metrics_provider.GetSigninStatusForTesting());
}

#if !defined(OS_ANDROID)
TEST(SigninStatusMetricsProvider, UpdateStatusWhenBrowserAdded) {
  SigninStatusMetricsProvider metrics_provider(true);

  // Initial status is all signed in and then a signed-in browser is opened.
  metrics_provider.UpdateInitialSigninStatus(2, 2);
  metrics_provider.UpdateStatusWhenBrowserAdded(true);
  EXPECT_EQ(SigninStatusMetricsProvider::ALL_PROFILES_SIGNED_IN,
            metrics_provider.GetSigninStatusForTesting());

  // Initial status is all signed in and then a signed-out browser is opened.
  metrics_provider.UpdateInitialSigninStatus(2, 2);
  metrics_provider.UpdateStatusWhenBrowserAdded(false);
  EXPECT_EQ(SigninStatusMetricsProvider::MIXED_SIGNIN_STATUS,
            metrics_provider.GetSigninStatusForTesting());

  // Initial status is all signed out and then a signed-in browser is opened.
  metrics_provider.UpdateInitialSigninStatus(2, 0);
  metrics_provider.UpdateStatusWhenBrowserAdded(true);
  EXPECT_EQ(SigninStatusMetricsProvider::MIXED_SIGNIN_STATUS,
            metrics_provider.GetSigninStatusForTesting());

  // Initial status is all signed out and then a signed-out browser is opened.
  metrics_provider.UpdateInitialSigninStatus(2, 0);
  metrics_provider.UpdateStatusWhenBrowserAdded(false);
  EXPECT_EQ(SigninStatusMetricsProvider::ALL_PROFILES_NOT_SIGNED_IN,
            metrics_provider.GetSigninStatusForTesting());

  // Initial status is mixed and then a signed-in browser is opened.
  metrics_provider.UpdateInitialSigninStatus(2, 1);
  metrics_provider.UpdateStatusWhenBrowserAdded(true);
  EXPECT_EQ(SigninStatusMetricsProvider::MIXED_SIGNIN_STATUS,
            metrics_provider.GetSigninStatusForTesting());

  // Initial status is mixed and then a signed-out browser is opened.
  metrics_provider.UpdateInitialSigninStatus(2, 1);
  metrics_provider.UpdateStatusWhenBrowserAdded(false);
  EXPECT_EQ(SigninStatusMetricsProvider::MIXED_SIGNIN_STATUS,
            metrics_provider.GetSigninStatusForTesting());
}
#endif

TEST(SigninStatusMetricsProvider, GoogleSigninSucceeded) {
  SigninStatusMetricsProvider metrics_provider(true);

  // Initial status is all signed out and then one of the profiles is signed in.
  metrics_provider.UpdateInitialSigninStatus(2, 0);
  metrics_provider.GoogleSigninSucceeded(std::string(), std::string());
  EXPECT_EQ(SigninStatusMetricsProvider::MIXED_SIGNIN_STATUS,
            metrics_provider.GetSigninStatusForTesting());

  // Initial status is mixed and then one of the profiles is signed in.
  metrics_provider.UpdateInitialSigninStatus(2, 1);
  metrics_provider.GoogleSigninSucceeded(std::string(), std::string());
  EXPECT_EQ(SigninStatusMetricsProvider::MIXED_SIGNIN_STATUS,
            metrics_provider.GetSigninStatusForTesting());
}

TEST(SigninStatusMetricsProvider, GoogleSignedOut) {
  SigninStatusMetricsProvider metrics_provider(true);

  // Initial status is all signed in and then one of the profiles is signed out.
  metrics_provider.UpdateInitialSigninStatus(2, 2);
  metrics_provider.GoogleSignedOut(std::string());
  EXPECT_EQ(SigninStatusMetricsProvider::MIXED_SIGNIN_STATUS,
            metrics_provider.GetSigninStatusForTesting());

  // Initial status is mixed and then one of the profiles is signed out.
  metrics_provider.UpdateInitialSigninStatus(2, 1);
  metrics_provider.GoogleSignedOut(std::string());
  EXPECT_EQ(SigninStatusMetricsProvider::MIXED_SIGNIN_STATUS,
            metrics_provider.GetSigninStatusForTesting());
}
