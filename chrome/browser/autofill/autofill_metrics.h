// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_METRICS_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_METRICS_H_
#pragma once

namespace autofill_metrics {

// Each of these should be logged at most once per query to the server, which in
// turn should occur at most once per page load.
enum ServerQueryMetricType {
  // Logged for each query sent to the server
  QUERY_SENT = 0,
  // Logged for each query response received from the server
  QUERY_RESPONSE_RECEIVED,
  // Logged for each parsable response received from the server
  QUERY_RESPONSE_PARSED,
  // Logged for each parsable response that provided no improvements relative to
  // our heuristics.
  QUERY_RESPONSE_MATCHED_LOCAL_HEURISTICS,
  // Logged for each page for which our heuristics detected at least one
  // auto-fillable field, but the server response overrode the type of at least
  // one field
  QUERY_RESPONSE_OVERRODE_LOCAL_HEURISTICS,
  // Logged for each page for which our heuristics did not detect any
  // auto-fillable fields, but the server response did detect some.
  QUERY_RESPONSE_WITH_NO_LOCAL_HEURISTICS,
  NUM_SERVER_QUERY_METRICS
};

void LogServerQueryMetric(ServerQueryMetricType type);

}  // namespace autofill_metrics

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_METRICS_H_
