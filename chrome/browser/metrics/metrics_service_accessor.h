// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_METRICS_SERVICE_ACCESSOR_H_
#define CHROME_BROWSER_METRICS_METRICS_SERVICE_ACCESSOR_H_

#include "base/macros.h"

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

 private:
  DISALLOW_COPY_AND_ASSIGN(MetricsServiceAccessor);
};

#endif  // CHROME_BROWSER_METRICS_METRICS_SERVICE_ACCESSOR_H_
