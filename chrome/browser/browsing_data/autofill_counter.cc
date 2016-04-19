// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/autofill_counter.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/memory/scoped_vector.h"
#include "chrome/browser/web_data_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"

AutofillCounter::AutofillCounter() : pref_name_(prefs::kDeleteFormData),
                                     web_data_service_(nullptr),
                                     suggestions_query_(0),
                                     credit_cards_query_(0),
                                     addresses_query_(0),
                                     num_suggestions_(0),
                                     num_credit_cards_(0),
                                     num_addresses_(0) {
}

AutofillCounter::~AutofillCounter() {
  CancelAllRequests();
}

void AutofillCounter::OnInitialized() {
  web_data_service_ = WebDataServiceFactory::GetAutofillWebDataForProfile(
      GetProfile(), ServiceAccessType::EXPLICIT_ACCESS);
  DCHECK(web_data_service_);
}

const std::string& AutofillCounter::GetPrefName() const {
  return pref_name_;
}

void AutofillCounter::SetPeriodStartForTesting(
    const base::Time& period_start_for_testing) {
  period_start_for_testing_ = period_start_for_testing;
}

void AutofillCounter::Count() {
  const base::Time start = period_start_for_testing_.is_null()
      ? GetPeriodStart()
      : period_start_for_testing_;

  CancelAllRequests();

  // Count the autocomplete suggestions (also called form elements in Autofill).
  // Note that |AutofillTable::RemoveFormElementsAddedBetween| only deletes
  // those whose entire existence (i.e. the interval between creation time
  // and last modified time) lies within the deletion time range. Otherwise,
  // it only decreases the count property, but always to a nonzero value,
  // and the suggestion is retained. Therefore here as well, we must only count
  // the entries that are entirely contained in [start, base::Time::Max()).
  // Further, many of these entries may contain the same values, as they are
  // simply the same data point entered on different forms. For example,
  // [name, value] pairs such as:
  //     ["mail", "example@example.com"]
  //     ["email", "example@example.com"]
  //     ["e-mail", "example@example.com"]
  // are stored as three separate entries, but from the user's perspective,
  // they constitute the same suggestion - "my email". Therefore, for the final
  // output, we will consider all entries with the same value as one suggestion,
  // and increment the counter only if all entries with the given value are
  // contained in the interval [start, base::Time::Max()).
  suggestions_query_ = web_data_service_->GetCountOfValuesContainedBetween(
      start, base::Time::Max(), this);

  // Count the credit cards.
  credit_cards_query_ = web_data_service_->GetCreditCards(this);

  // Count the addresses.
  addresses_query_ = web_data_service_->GetAutofillProfiles(this);
}

void AutofillCounter::OnWebDataServiceRequestDone(
    WebDataServiceBase::Handle handle, const WDTypedResult* result) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!result) {
    CancelAllRequests();
    return;
  }

  const base::Time start = period_start_for_testing_.is_null()
      ? GetPeriodStart()
      : period_start_for_testing_;

  if (handle == suggestions_query_) {
    // Autocomplete suggestions.
    DCHECK_EQ(AUTOFILL_VALUE_RESULT, result->GetType());
    num_suggestions_ = static_cast<const WDResult<int>*>(result)->GetValue();
    suggestions_query_ = 0;

  } else if (handle == credit_cards_query_) {
    // Credit cards.
    DCHECK_EQ(AUTOFILL_CREDITCARDS_RESULT, result->GetType());
    const std::vector<autofill::CreditCard*> credit_cards =
        static_cast<const WDResult<std::vector<autofill::CreditCard*>>*>(
            result)->GetValue();

    // We own the result from this query. Make sure it will be deleted.
    ScopedVector<const autofill::CreditCard> owned_result;
    owned_result.assign(credit_cards.begin(), credit_cards.end());

    num_credit_cards_ = std::count_if(
        credit_cards.begin(),
        credit_cards.end(),
        [start](const autofill::CreditCard* card) {
          return card->modification_date() >= start;
        });
    credit_cards_query_ = 0;

  } else if (handle == addresses_query_) {
    // Addresses.
    DCHECK_EQ(AUTOFILL_PROFILES_RESULT, result->GetType());
    const std::vector<autofill::AutofillProfile*> addresses =
        static_cast<const WDResult<std::vector<autofill::AutofillProfile*>>*>(
            result)->GetValue();

    // We own the result from this query. Make sure it will be deleted.
    ScopedVector<const autofill::AutofillProfile> owned_result;
    owned_result.assign(addresses.begin(), addresses.end());

    num_addresses_ = std::count_if(
        addresses.begin(),
        addresses.end(),
        [start](const autofill::AutofillProfile* address) {
          return address->modification_date() >= start;
        });
    addresses_query_ = 0;

  } else {
    NOTREACHED() << "No such query: " << handle;
  }

  // If we still have pending queries, do not report data yet.
  if (HasPendingQuery())
    return;

  std::unique_ptr<Result> reported_result(new AutofillResult(
      this, num_suggestions_, num_credit_cards_, num_addresses_));
  ReportResult(std::move(reported_result));
}

void AutofillCounter::CancelAllRequests() {
  if (suggestions_query_)
    web_data_service_->CancelRequest(suggestions_query_);
  if (credit_cards_query_)
    web_data_service_->CancelRequest(credit_cards_query_);
  if (addresses_query_)
    web_data_service_->CancelRequest(addresses_query_);
}

// AutofillCounter::AutofillResult ---------------------------------------------

AutofillCounter::AutofillResult::AutofillResult(
    const AutofillCounter* source,
    ResultInt num_suggestions,
    ResultInt num_credit_cards,
    ResultInt num_addresses)
    : FinishedResult(source, num_suggestions),
      num_credit_cards_(num_credit_cards),
      num_addresses_(num_addresses) {
}

AutofillCounter::AutofillResult::~AutofillResult() {
}
