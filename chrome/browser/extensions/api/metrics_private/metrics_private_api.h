// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_METRICS_PRIVATE_METRICS_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_METRICS_PRIVATE_METRICS_PRIVATE_API_H_

#include <string>

#include "base/metrics/histogram.h"
#include "chrome/browser/extensions/extension_function.h"

namespace extensions {

class MetricsPrivateRecordUserActionFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("metricsPrivate.recordUserAction",
                             METRICSPRIVATE_RECORDUSERACTION)

 protected:
  virtual ~MetricsPrivateRecordUserActionFunction() {}

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

class MetricsPrivateRecordValueFunction
    : public MetricsHistogramHelperFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("metricsPrivate.recordValue",
                             METRICSPRIVATE_RECORDVALUE)

 protected:
  virtual ~MetricsPrivateRecordValueFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class MetricsPrivateRecordPercentageFunction
    : public MetricsHistogramHelperFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("metricsPrivate.recordPercentage",
                             METRICSPRIVATE_RECORDPERCENTAGE)

 protected:
  virtual ~MetricsPrivateRecordPercentageFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class MetricsPrivateRecordCountFunction
    : public MetricsHistogramHelperFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("metricsPrivate.recordCount",
                             METRICSPRIVATE_RECORDCOUNT)

 protected:
  virtual ~MetricsPrivateRecordCountFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class MetricsPrivateRecordSmallCountFunction
    : public MetricsHistogramHelperFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("metricsPrivate.recordSmallCount",
                             METRICSPRIVATE_RECORDSMALLCOUNT)

 protected:
  virtual ~MetricsPrivateRecordSmallCountFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class MetricsPrivateRecordMediumCountFunction
    : public MetricsHistogramHelperFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("metricsPrivate.recordMediumCount",
                             METRICSPRIVATE_RECORDMEDIUMCOUNT)

 protected:
  virtual ~MetricsPrivateRecordMediumCountFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class MetricsPrivateRecordTimeFunction : public MetricsHistogramHelperFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("metricsPrivate.recordTime",
                             METRICSPRIVATE_RECORDTIME)

 protected:
  virtual ~MetricsPrivateRecordTimeFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class MetricsPrivateRecordMediumTimeFunction
    : public MetricsHistogramHelperFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("metricsPrivate.recordMediumTime",
                             METRICSPRIVATE_RECORDMEDIUMTIME)

 protected:
  virtual ~MetricsPrivateRecordMediumTimeFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class MetricsPrivateRecordLongTimeFunction
    : public MetricsHistogramHelperFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("metricsPrivate.recordLongTime",
                             METRICSPRIVATE_RECORDLONGTIME)

 protected:
  virtual ~MetricsPrivateRecordLongTimeFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

} // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_METRICS_PRIVATE_METRICS_PRIVATE_API_H_
