// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_METRICS_SERVICE_ACCESSOR_H_
#define CHROME_BROWSER_METRICS_METRICS_SERVICE_ACCESSOR_H_

#include <string>

#include "base/macros.h"

class MetricsService;
class MetricsServiceObserver;

// This class limits and documents access to metrics service helper methods.
// These methods are protected so each user has to inherit own program-specific
// specialization and enable access there by declaring friends.
class MetricsServiceAccessor {
 protected:
  // Constructor declared as protected to enable inheritance. Decendants should
  // disallow instantiation.
  MetricsServiceAccessor() {}

  // Registers/unregisters |observer| to receive MetricsLog notifications
  // from metrics service.
  static void AddMetricsServiceObserver(MetricsServiceObserver* observer);
  static void RemoveMetricsServiceObserver(MetricsServiceObserver* observer);

  // Registers a field trial name and group to be used to annotate a UMA report
  // with a particular Chrome configuration state. A UMA report will be
  // annotated with this trial group if and only if all events in the report
  // were created after the trial is registered. Only one group name may be
  // registered at a time for a given trial name. Only the last group name that
  // is registered for a given trial name will be recorded. The values passed
  // in must not correspond to any real field trial in the code.
  static bool RegisterSyntheticFieldTrial(MetricsService* metrics_service,
                                          const std::string& trial,
                                          const std::string& group);

 private:
  DISALLOW_COPY_AND_ASSIGN(MetricsServiceAccessor);
};

#endif  // CHROME_BROWSER_METRICS_METRICS_SERVICE_ACCESSOR_H_
