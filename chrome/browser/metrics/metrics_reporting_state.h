// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_METRICS_REPORTING_STATE_H_
#define CHROME_BROWSER_METRICS_METRICS_REPORTING_STATE_H_

// Tries to set crash stats upload consent to |enabled|. Regardless of success,
// if crash stats uploading is enabled, starts the MetricsService; otherwise
// stops it.
// Returns whether crash stats uploading is enabled.
// TODO(tfarina): This method is pretty confusing. Clean this up and rename it
// to something that makes more sense.
bool ResolveMetricsReportingEnabled(bool enabled);

#endif  // CHROME_BROWSER_METRICS_METRICS_REPORTING_STATE_H_
