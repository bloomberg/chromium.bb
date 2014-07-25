// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_SSL_ERROR_CLASSIFICATION_H_
#define CHROME_BROWSER_SSL_SSL_ERROR_CLASSIFICATION_H_

#include "base/time/time.h"
#include "net/cert/x509_certificate.h"

// This class calculates the severity scores for the different type of SSL
// errors.
class SSLErrorClassification {
 public:
  SSLErrorClassification(base::Time current_time,
                         const net::X509Certificate& cert);
  ~SSLErrorClassification();

  // This method checks whether the user clock is in the past or not.
  static bool IsUserClockInThePast(base::Time time_now);

  // This method checks whether the system time is too far in the future or
  // the user is using a version of Chrome which is more than 1 year old.
  static bool IsUserClockInTheFuture(base::Time time_now);

  static bool IsWindowsVersionSP3OrLower();

  // A method which calculates the severity score when the ssl error is
  // CERT_DATE_INVALID.
  float InvalidDateSeverityScore() const;

  static void RecordUMAStatistics(bool overridable);
  base::TimeDelta TimePassedSinceExpiry() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(SSLErrorClassification, TestDateInvalidScore);

  float CalculateScoreTimePassedSinceExpiry() const;

  // This stores the current time.
  base::Time current_time_;

  // This stores the certificate.
  const net::X509Certificate& cert_;
};

#endif  // CHROME_BROWSER_SSL_SSL_ERROR_CLASSIFICATION_H_
