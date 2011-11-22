// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_METRICS_MODULE_H__
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_METRICS_MODULE_H__
#pragma once

#include <string>

#include "base/metrics/histogram.h"
#include "chrome/browser/extensions/extension_function.h"

class MetricsRecordUserActionFunction : public SyncExtensionFunction {
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("metricsPrivate.recordUserAction")
};

class MetricsHistogramHelperFunction : public SyncExtensionFunction {
 protected:
  bool GetNameAndSample(std::string* name, int* sample);
  virtual bool RecordValue(const std::string& name,
                           base::Histogram::ClassType type,
                           int min, int max, size_t buckets, int sample);
};

class MetricsRecordValueFunction : public MetricsHistogramHelperFunction {
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("metricsPrivate.recordValue")
};

class MetricsRecordPercentageFunction : public MetricsHistogramHelperFunction {
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("metricsPrivate.recordPercentage")
};

class MetricsRecordCountFunction : public MetricsHistogramHelperFunction {
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("metricsPrivate.recordCount")
};

class MetricsRecordSmallCountFunction : public MetricsHistogramHelperFunction {
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("metricsPrivate.recordSmallCount")
};

class MetricsRecordMediumCountFunction : public MetricsHistogramHelperFunction {
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("metricsPrivate.recordMediumCount")
};

class MetricsRecordTimeFunction : public MetricsHistogramHelperFunction {
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("metricsPrivate.recordTime")
};

class MetricsRecordMediumTimeFunction : public MetricsHistogramHelperFunction {
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("metricsPrivate.recordMediumTime")
};

class MetricsRecordLongTimeFunction : public MetricsHistogramHelperFunction {
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("metricsPrivate.recordLongTime")
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_METRICS_MODULE_H__
