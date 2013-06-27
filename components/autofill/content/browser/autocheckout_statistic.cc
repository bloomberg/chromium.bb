// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/autocheckout_statistic.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"

namespace autofill {

namespace {

std::string AutocheckoutStepToString(AutocheckoutStepType step) {
  switch(step) {
    case AUTOCHECKOUT_STEP_SHIPPING:
      return "AUTOCHECKOUT_STEP_SHIPPING";
    case AUTOCHECKOUT_STEP_DELIVERY:
      return "AUTOCHECKOUT_STEP_DELIVERY";
    case AUTOCHECKOUT_STEP_BILLING:
      return "AUTOCHECKOUT_STEP_BILLING";
    case AUTOCHECKOUT_STEP_PROXY_CARD:
      return "AUTOCHECKOUT_STEP_PROXY_CARD";
  }
  NOTREACHED();
  return "NOT_POSSIBLE";
}

}  // namespace

AutocheckoutStatistic::AutocheckoutStatistic() : page_number(-1) {
}

AutocheckoutStatistic::~AutocheckoutStatistic() {
}

scoped_ptr<base::DictionaryValue> AutocheckoutStatistic::ToDictionary() const {
  scoped_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  std::string description = base::IntToString(page_number);
  for (size_t i = 0; i < steps.size(); ++i) {
    description = description + "_" + AutocheckoutStepToString(steps[i]);
  }
  dict->SetString("step_description", description);
  dict->SetInteger("time_taken", time_taken.InMilliseconds());
  DVLOG(1) << "Step : " << description
           << ", time_taken: " << time_taken.InMilliseconds();
  return dict.Pass();
}

}  // namespace autofill

