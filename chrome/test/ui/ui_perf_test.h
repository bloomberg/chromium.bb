// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_UI_UI_PERF_TEST_H_
#define CHROME_TEST_UI_UI_PERF_TEST_H_
#pragma once

#include <string>

#include "chrome/test/ui/ui_test.h"

class UIPerfTest : public UITest {
 protected:
  // Override UITestBase.
  void SetLaunchSwitches();

  // Prints numerical information to stdout in a controlled format, for
  // post-processing. |measurement| is a description of the quantity being
  // measured, e.g. "vm_peak"; |modifier| is provided as a convenience and
  // will be appended directly to the name of the |measurement|, e.g.
  // "_browser"; |trace| is a description of the particular data point, e.g.
  // "reference"; |value| is the measured value; and |units| is a description
  // of the units of measure, e.g. "bytes". If |important| is true, the output
  // line will be specially marked, to notify the post-processor. The strings
  // may be empty.  They should not contain any colons (:) or equals signs (=).
  // A typical post-processing step would be to produce graphs of the data
  // produced for various builds, using the combined |measurement| + |modifier|
  // string to specify a particular graph and the |trace| to identify a trace
  // (i.e., data series) on that graph.
  void PrintResult(const std::string& measurement,
                   const std::string& modifier,
                   const std::string& trace,
                   size_t value,
                   const std::string& units,
                   bool important);

  // Like the above version of PrintResult(), but takes a std::string value
  // instead of a size_t.
  void PrintResult(const std::string& measurement,
                   const std::string& modifier,
                   const std::string& trace,
                   const std::string& value,
                   const std::string& units,
                   bool important);

  // Like PrintResult(), but prints a (mean, standard deviation) result pair.
  // The |<values>| should be two comma-seaprated numbers, the mean and
  // standard deviation (or other error metric) of the measurement.
  void PrintResultMeanAndError(const std::string& measurement,
                               const std::string& modifier,
                               const std::string& trace,
                               const std::string& mean_and_error,
                               const std::string& units,
                               bool important);

  // Like PrintResult(), but prints an entire list of results. The |values|
  // will generally be a list of comma-separated numbers. A typical
  // post-processing step might produce plots of their mean and standard
  // deviation.
  void PrintResultList(const std::string& measurement,
                       const std::string& modifier,
                       const std::string& trace,
                       const std::string& values,
                       const std::string& units,
                       bool important);

  // Prints IO performance data for use by perf graphs.
  void PrintIOPerfInfo(const char* test_name);

  // Prints memory usage data for use by perf graphs.
  void PrintMemoryUsageInfo(const char* test_name);

  // Prints memory commit charge stats for use by perf graphs.
  void PrintSystemCommitCharge(const char* test_name,
                               size_t charge,
                               bool important);

  // Configures the test to use the reference build.
  void UseReferenceBuild();

 private:
  // Common functionality for the public PrintResults methods.
  void PrintResultsImpl(const std::string& measurement,
                        const std::string& modifier,
                        const std::string& trace,
                        const std::string& values,
                        const std::string& prefix,
                        const std::string& suffix,
                        const std::string& units,
                        bool important);
};

#endif  // CHROME_TEST_UI_UI_PERF_TEST_H_
