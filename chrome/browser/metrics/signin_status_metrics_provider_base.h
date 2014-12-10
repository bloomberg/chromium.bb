// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_SIGNIN_STATUS_METRICS_PROVIDER_BASE_H_
#define CHROME_BROWSER_METRICS_SIGNIN_STATUS_METRICS_PROVIDER_BASE_H_

#include "components/metrics/metrics_provider.h"

// The base class for collecting login status of all opened profiles during one
// UMA session and recording the value into a histogram before UMA log is
// uploaded. Overriding class SigninStatusMetricsProvider supports platform
// Window, Linux, Macintosh and Android, and overriding class
// SigninStatusMetricsProviderChromeOS supports ChromeOS. It is currently not
// supported on iOS.
class SigninStatusMetricsProviderBase : public metrics::MetricsProvider {
 public:
  SigninStatusMetricsProviderBase();
  ~SigninStatusMetricsProviderBase() override;

  // Possible sign-in status of all opened profiles during one UMA session. For
  // MIXED_SIGNIN_STATUS, at least one signed-in profile and at least one
  // unsigned-in profile were opened between two UMA log uploads. Not every
  // possibilities can be applied to each platform.
  enum SigninStatus {
    ALL_PROFILES_SIGNED_IN,
    ALL_PROFILES_NOT_SIGNED_IN,
    MIXED_SIGNIN_STATUS,
    UNKNOWN_SIGNIN_STATUS,
    ERROR_GETTING_SIGNIN_STATUS,
    SIGNIN_STATUS_MAX,
  };

 protected:
  // Record the sign in status into the proper histogram bucket. This should be
  // called exactly once for each UMA session.
  void RecordSigninStatusHistogram(SigninStatus signin_status);

  // Sets the value of |signin_status_|. It ensures that |signin_status_| will
  // not be changed if its value is already ERROR_GETTING_SIGNIN_STATUS.
  void SetSigninStatus(SigninStatus new_status);

  // Resets the value of |signin_status_| to be UNKNOWN_SIGNIN_STATUS regardless
  // of its current value;
  void ResetSigninStatus();

  SigninStatus signin_status() const { return signin_status_; }

 private:
  // Sign-in status of all profiles seen so far.
  SigninStatus signin_status_;

  DISALLOW_COPY_AND_ASSIGN(SigninStatusMetricsProviderBase);
};

#endif  // CHROME_BROWSER_METRICS_SIGNIN_STATUS_METRICS_PROVIDER_BASE_H_
