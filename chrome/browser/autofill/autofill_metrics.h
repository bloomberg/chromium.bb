// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_METRICS_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_METRICS_H_
#pragma once

#include <stddef.h>
#include <string>

#include "base/basictypes.h"
#include "chrome/browser/autofill/field_types.h"

class AutofillMetrics {
 public:
  enum CreditCardInfoBarMetric {
    CREDIT_CARD_INFOBAR_SHOWN = 0,  // We showed an infobar prompting to save
                                    // credit card info.
    CREDIT_CARD_INFOBAR_ACCEPTED,   // The user explicitly accepted the infobar.
    CREDIT_CARD_INFOBAR_DENIED,     // The user explicitly denied the infobar.
    CREDIT_CARD_INFOBAR_IGNORED,    // The user completely ignored the infobar
                                    // (logged on tab close).
    NUM_CREDIT_CARD_INFO_BAR_METRICS
  };

  // Metrics measuring how well we predict field types. Exactly one metric from
  // each set is logged for each fillable field in a submitted form.
  enum HeuristicTypeQualityMetric {
    HEURISTIC_TYPE_UNKNOWN = 0,  // Our heuristics offered no prediction.
    HEURISTIC_TYPE_MATCH,        // Our heuristics predicted correctly.
    HEURISTIC_TYPE_MISMATCH,     // Our heuristics predicted incorrectly.
    NUM_HEURISTIC_TYPE_QUALITY_METRICS
  };
  enum ServerTypeQualityMetric {
    SERVER_TYPE_UNKNOWN = 0,  // The server offered no prediction.
    SERVER_TYPE_MATCH,        // The server predicted correctly.
    SERVER_TYPE_MISMATCH,     // The server predicted incorrectly.
    NUM_SERVER_TYPE_QUALITY_METRICS
  };
  enum PredictedTypeQualityMetric {
    PREDICTED_TYPE_UNKNOWN = 0,  // Neither server nor heuristics offered a
                                 // prediction.
    PREDICTED_TYPE_MATCH,        // Overall, predicted correctly.
    PREDICTED_TYPE_MISMATCH,     // Overall, predicted incorrectly.
    NUM_PREDICTED_TYPE_QUALITY_METRICS
  };

  enum QualityMetric {
    // Logged for each potentially fillable field in a submitted form.
    FIELD_SUBMITTED = 0,

    // A simple successs metric, logged for each field that returns true for
    // |is_autofilled()|.
    FIELD_AUTOFILLED,

    // A simple failure metric, logged for each field that returns false for
    // |is_autofilled()| but has a value that is present in the personal data
    // manager.
    FIELD_NOT_AUTOFILLED,

    // The below are only logged when |FIELD_AUTOFILL_FAILED| is also logged.
    NOT_AUTOFILLED_HEURISTIC_TYPE_UNKNOWN,
    NOT_AUTOFILLED_HEURISTIC_TYPE_MATCH,
    NOT_AUTOFILLED_HEURISTIC_TYPE_MISMATCH,
    NOT_AUTOFILLED_SERVER_TYPE_UNKNOWN,
    NOT_AUTOFILLED_SERVER_TYPE_MATCH,
    NOT_AUTOFILLED_SERVER_TYPE_MISMATCH,
    NUM_QUALITY_METRICS
  };

  // Each of these is logged at most once per query to the server, which in turn
  // occurs at most once per page load.
  enum ServerQueryMetric {
    QUERY_SENT = 0,           // Sent a query to the server.
    QUERY_RESPONSE_RECEIVED,  // Received a response.
    QUERY_RESPONSE_PARSED,    // Successfully parsed the server response.

    // The response was parseable, but provided no improvements relative to our
    // heuristics.
    QUERY_RESPONSE_MATCHED_LOCAL_HEURISTICS,

    // Our heuristics detected at least one auto-fillable field, and the server
    // response overrode the type of at least one field.
    QUERY_RESPONSE_OVERRODE_LOCAL_HEURISTICS,

    // Our heuristics did not detect any auto-fillable fields, but the server
    // response did detect at least one.
    QUERY_RESPONSE_WITH_NO_LOCAL_HEURISTICS,
    NUM_SERVER_QUERY_METRICS
  };

  AutofillMetrics();
  virtual ~AutofillMetrics();

  virtual void Log(CreditCardInfoBarMetric metric) const;
  virtual void Log(HeuristicTypeQualityMetric metric,
                   AutofillFieldType field_type,
                   const std::string& experiment_id) const;
  virtual void Log(PredictedTypeQualityMetric metric,
                   AutofillFieldType field_type,
                   const std::string& experiment_id) const;
  virtual void Log(QualityMetric metric,
                   const std::string& experiment_id) const;
  virtual void Log(ServerQueryMetric metric) const;
  virtual void Log(ServerTypeQualityMetric metric,
                   AutofillFieldType field_type,
                   const std::string& experiment_id) const;

  // This should be called each time a page containing forms is loaded.
  virtual void LogIsAutofillEnabledAtPageLoad(bool enabled) const;

  // This should be called each time a new profile is launched.
  virtual void LogIsAutofillEnabledAtStartup(bool enabled) const;

  // This should be called each time a new profile is launched.
  virtual void LogStoredProfileCount(size_t num_profiles) const;

  // Log the number of Autofill suggestions presented to the user when filling a
  // form.
  virtual void LogAddressSuggestionsCount(size_t num_suggestions) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(AutofillMetrics);
};

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_METRICS_H_
