// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_AUTOFILL_COUNTER_H_
#define CHROME_BROWSER_BROWSING_DATA_AUTOFILL_COUNTER_H_

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "chrome/browser/browsing_data/browsing_data_counter.h"
#include "components/webdata/common/web_data_service_consumer.h"

namespace autofill {
class AutofillWebDataService;
}

class AutofillCounter: public BrowsingDataCounter,
                       public WebDataServiceConsumer {
 public:
  class AutofillResult : public FinishedResult {
   public:
    AutofillResult(const AutofillCounter* source,
                   ResultInt num_suggestions,
                   ResultInt num_credit_cards,
                   ResultInt num_addresses);
    ~AutofillResult() override;

    ResultInt num_credit_cards() const { return num_credit_cards_; }
    ResultInt num_addresses() const { return num_addresses_; }

   private:
    ResultInt num_credit_cards_;
    ResultInt num_addresses_;

    DISALLOW_COPY_AND_ASSIGN(AutofillResult);
  };

  AutofillCounter();
  ~AutofillCounter() override;

  // BrowsingDataCounter implementation.
  void OnInitialized() override;
  const std::string& GetPrefName() const override;

  // Whether the counting is in progress.
  bool HasPendingQuery() {
    return suggestions_query_ || credit_cards_query_ || addresses_query_;
  }

  // Set the beginning of the time period for testing. AutofillTable does not
  // allow us to set time explicitly, and BrowsingDataCounter recognizes
  // only predefined time periods, out of which the lowest one is one hour.
  // Obviously, the test cannot run that long.
  // TODO(msramek): Consider changing BrowsingDataCounter to use arbitrary
  // time periods instead of BrowsingDataRemover::TimePeriod.
  void SetPeriodStartForTesting(const base::Time& period_start_for_testing);

 private:
  const std::string pref_name_;
  base::ThreadChecker thread_checker_;

  scoped_refptr<autofill::AutofillWebDataService> web_data_service_;

  WebDataServiceBase::Handle suggestions_query_;
  WebDataServiceBase::Handle credit_cards_query_;
  WebDataServiceBase::Handle addresses_query_;

  ResultInt num_suggestions_;
  ResultInt num_credit_cards_;
  ResultInt num_addresses_;

  base::Time period_start_for_testing_;

  void Count() override;

  // WebDataServiceConsumer implementation.
  void OnWebDataServiceRequestDone(WebDataServiceBase::Handle handle,
                                   const WDTypedResult* result) override;

  // Cancel all pending requests to AutofillWebdataService.
  void CancelAllRequests();

  DISALLOW_COPY_AND_ASSIGN(AutofillCounter);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_AUTOFILL_COUNTER_H_
