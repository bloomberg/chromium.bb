// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_IOS_CHROME_METRICS_SERVICE_ACCESSOR_H_
#define IOS_CHROME_BROWSER_METRICS_IOS_CHROME_METRICS_SERVICE_ACCESSOR_H_

#include <stdint.h>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "components/metrics/metrics_service_accessor.h"

class IOSChromeDataReductionProxySettings;

namespace {
class CrashesDOMHandler;
}

// This class limits and documents access to metrics service helper methods.
// Since these methods are private, each user has to be explicitly declared
// as a 'friend' below.
class IOSChromeMetricsServiceAccessor : public metrics::MetricsServiceAccessor {
 public:
  // If arg is non-null, the value will be returned from future calls to
  // IsMetricsAndCrashReportingEnabled(). Pointer must be valid until it is
  // reset to null here.
  static void SetMetricsAndCrashReportingForTesting(const bool* value);

 private:
  friend class IOSChromeMetricsServicesManagerClient;

  friend class CrashesDOMHandler;
  friend class IOSChromeDataReductionProxySettings;

  FRIEND_TEST_ALL_PREFIXES(IOSChromeMetricsServiceAccessorTest,
                           MetricsReportingEnabled);

  // Returns true if metrics reporting is enabled.
  static bool IsMetricsAndCrashReportingEnabled();

  // Calls metrics::MetricsServiceAccessor::RegisterSyntheticFieldTrial() with
  // ApplicationContext's MetricsService. See that function's declaration for
  // details.
  static bool RegisterSyntheticFieldTrial(const std::string& trial_name,
                                          const std::string& group_name);

  // Calls
  // metrics::MetricsServiceAccessor::RegisterSyntheticFieldTrialWithNameHash()
  // with ApplicationContext's MetricsService. See that function's declaration
  // for details.
  static bool RegisterSyntheticFieldTrialWithNameHash(
      uint32_t trial_name_hash,
      const std::string& group_name);

  DISALLOW_IMPLICIT_CONSTRUCTORS(IOSChromeMetricsServiceAccessor);
};

#endif  // IOS_CHROME_BROWSER_METRICS_IOS_CHROME_METRICS_SERVICE_ACCESSOR_H_
