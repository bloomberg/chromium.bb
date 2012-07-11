// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_METRICS_METRICS_H_
#define CHROME_BROWSER_EXTENSIONS_API_METRICS_METRICS_H_

#include <string>

#include "base/metrics/histogram.h"
#include "chrome/browser/extensions/extension_function.h"

namespace extensions {

class MetricsRecordUserActionFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("metricsPrivate.recordUserAction")

 protected:
  virtual ~MetricsRecordUserActionFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class MetricsHistogramHelperFunction : public SyncExtensionFunction {
 protected:
  virtual ~MetricsHistogramHelperFunction() {}
  bool GetNameAndSample(std::string* name, int* sample);
  virtual bool RecordValue(const std::string& name,
                           base::Histogram::ClassType type,
                           int min, int max, size_t buckets,
                           int sample);
};

class MetricsRecordValueFunction : public MetricsHistogramHelperFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("metricsPrivate.recordValue")

 protected:
  virtual ~MetricsRecordValueFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class MetricsRecordPercentageFunction : public MetricsHistogramHelperFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("metricsPrivate.recordPercentage")

 protected:
  virtual ~MetricsRecordPercentageFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class MetricsRecordCountFunction : public MetricsHistogramHelperFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("metricsPrivate.recordCount")

 protected:
  virtual ~MetricsRecordCountFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class MetricsRecordSmallCountFunction : public MetricsHistogramHelperFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("metricsPrivate.recordSmallCount")

 protected:
  virtual ~MetricsRecordSmallCountFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class MetricsRecordMediumCountFunction : public MetricsHistogramHelperFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("metricsPrivate.recordMediumCount")

 protected:
  virtual ~MetricsRecordMediumCountFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class MetricsRecordTimeFunction : public MetricsHistogramHelperFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("metricsPrivate.recordTime")

 protected:
  virtual ~MetricsRecordTimeFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class MetricsRecordMediumTimeFunction : public MetricsHistogramHelperFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("metricsPrivate.recordMediumTime")

 protected:
  virtual ~MetricsRecordMediumTimeFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class MetricsRecordLongTimeFunction : public MetricsHistogramHelperFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("metricsPrivate.recordLongTime")

 protected:
  virtual ~MetricsRecordLongTimeFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

} // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_METRICS_METRICS_H_
