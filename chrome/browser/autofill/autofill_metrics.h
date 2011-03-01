// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_METRICS_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_METRICS_H_
#pragma once

#include <string>

#include "base/basictypes.h"

class AutofillMetrics {
 public:
  // Each of these is logged at most once per query to the server, which in turn
  // occurs at most once per page load.
  enum ServerQueryMetric {
    // Logged for each query sent to the server.
    QUERY_SENT = 0,
    // Logged for each query response received from the server.
    QUERY_RESPONSE_RECEIVED,
    // Logged for each parsable response received from the server.
    QUERY_RESPONSE_PARSED,
    // Logged for each parsable response that provided no improvements relative
    // to our heuristics.
    QUERY_RESPONSE_MATCHED_LOCAL_HEURISTICS,
    // Logged for each page for which our heuristics detected at least one
    // auto-fillable field, but the server response overrode the type of at
    // least one field.
    QUERY_RESPONSE_OVERRODE_LOCAL_HEURISTICS,
    // Logged for each page for which our heuristics did not detect any
    // auto-fillable fields, but the server response did detect some.
    QUERY_RESPONSE_WITH_NO_LOCAL_HEURISTICS,
    NUM_SERVER_QUERY_METRICS
  };

  // Each of these is logged at most once per form submission.
  enum QualityMetric {
    // Logged for each field in a submitted form.
    FIELD_SUBMITTED = 0,
    // A simple successs metric, logged for each field that returns true for
    // |is_autofilled()| and has a value that is present in the personal data
    // manager. There is a small chance of false positives from filling via
    // autocomplete rather than autofill.
    FIELD_AUTOFILLED,
    // A simple failure metric, logged for each field that returns false for
    // |is_autofilled()| but as a value that is present in the personal data
    // manager.
    FIELD_AUTOFILL_FAILED,
    // The below are only logged when |FIELD_AUTOFILL_FAILED| is also logged.
    FIELD_HEURISTIC_TYPE_UNKNOWN,
    FIELD_HEURISTIC_TYPE_MATCH,
    FIELD_HEURISTIC_TYPE_MISMATCH,
    FIELD_SERVER_TYPE_UNKNOWN,
    FIELD_SERVER_TYPE_MATCH,
    FIELD_SERVER_TYPE_MISMATCH,
    NUM_QUALITY_METRICS
  };

  AutofillMetrics();
  virtual ~AutofillMetrics();

  virtual void Log(ServerQueryMetric metric) const;
  virtual void Log(QualityMetric metric,
                   const std::string& experiment_id) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(AutofillMetrics);
};

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_METRICS_H_
