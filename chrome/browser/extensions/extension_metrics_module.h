// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_METRICS_MODULE_H__
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_METRICS_MODULE_H__

#include <string>

#include "base/histogram.h"
#include "chrome/browser/extensions/extension_function.h"

class MetricsRecordUserActionFunction : public SyncExtensionFunction {
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("metrics.recordUserAction")
};

class MetricsHistogramHelperFunction : public SyncExtensionFunction {
 protected:
  bool GetNameAndSample(std::string* name, int* sample);
  virtual bool RecordValue(const std::string& name, Histogram::ClassType type,
      int min, int max, size_t buckets, int sample);
};

class MetricsRecordValueFunction : public MetricsHistogramHelperFunction {
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("metrics.recordValue")
};

class MetricsRecordPercentageFunction : public MetricsHistogramHelperFunction {
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("metrics.recordPercentage")
};

class MetricsRecordCountFunction : public MetricsHistogramHelperFunction {
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("metrics.recordCount")
};

class MetricsRecordSmallCountFunction : public MetricsHistogramHelperFunction {
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("metrics.recordSmallCount")
};

class MetricsRecordMediumCountFunction : public MetricsHistogramHelperFunction {
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("metrics.recordMediumCount")
};

class MetricsRecordTimeFunction : public MetricsHistogramHelperFunction {
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("metrics.recordTime")
};

class MetricsRecordMediumTimeFunction : public MetricsHistogramHelperFunction {
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("metrics.recordMediumTime")
};

class MetricsRecordLongTimeFunction : public MetricsHistogramHelperFunction {
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("metrics.recordLongTime")
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_METRICS_MODULE_H__
