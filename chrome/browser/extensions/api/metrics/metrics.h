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
  DECLARE_EXTENSION_FUNCTION("metricsPrivate.recordUserAction",
                             METRICSPRIVATE_RECORDUSERACTION)

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
                           base::HistogramType type,
                           int min, int max, size_t buckets,
                           int sample);
};

class MetricsRecordValueFunction : public MetricsHistogramHelperFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("metricsPrivate.recordValue",
                             METRICSPRIVATE_RECORDVALUE)

 protected:
  virtual ~MetricsRecordValueFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class MetricsRecordPercentageFunction : public MetricsHistogramHelperFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("metricsPrivate.recordPercentage",
                             METRICSPRIVATE_RECORDPERCENTAGE)

 protected:
  virtual ~MetricsRecordPercentageFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class MetricsRecordCountFunction : public MetricsHistogramHelperFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("metricsPrivate.recordCount",
                             METRICSPRIVATE_RECORDCOUNT)

 protected:
  virtual ~MetricsRecordCountFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class MetricsRecordSmallCountFunction : public MetricsHistogramHelperFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("metricsPrivate.recordSmallCount",
                             METRICSPRIVATE_RECORDSMALLCOUNT)

 protected:
  virtual ~MetricsRecordSmallCountFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class MetricsRecordMediumCountFunction : public MetricsHistogramHelperFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("metricsPrivate.recordMediumCount",
                             METRICSPRIVATE_RECORDMEDIUMCOUNT)

 protected:
  virtual ~MetricsRecordMediumCountFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class MetricsRecordTimeFunction : public MetricsHistogramHelperFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("metricsPrivate.recordTime",
                             METRICSPRIVATE_RECORDTIME)

 protected:
  virtual ~MetricsRecordTimeFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class MetricsRecordMediumTimeFunction : public MetricsHistogramHelperFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("metricsPrivate.recordMediumTime",
                             METRICSPRIVATE_RECORDMEDIUMTIME)

 protected:
  virtual ~MetricsRecordMediumTimeFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class MetricsRecordLongTimeFunction : public MetricsHistogramHelperFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("metricsPrivate.recordLongTime",
                             METRICSPRIVATE_RECORDLONGTIME)

 protected:
  virtual ~MetricsRecordLongTimeFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

} // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_METRICS_METRICS_H_
