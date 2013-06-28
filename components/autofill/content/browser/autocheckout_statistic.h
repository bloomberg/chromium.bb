// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_AUTOCHECKOUT_STATISTIC_H__
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_AUTOCHECKOUT_STATISTIC_H__

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "components/autofill/content/browser/autocheckout_steps.h"

namespace base {
class DictionaryValue;
}

namespace autofill {

// Per page statistics gathered in Autocheckout flow.
struct AutocheckoutStatistic {
  AutocheckoutStatistic();
  ~AutocheckoutStatistic();

  // Page number for which this statistic is being recorded.
  int page_number;

  // Autocheckout steps that are part of |page_number|.
  std::vector<AutocheckoutStepType> steps;

  // Time taken for performing |steps|, used for measuring latency.
  base::TimeDelta time_taken;

  // Helper method to convert AutocheckoutStatistic to json representation.
  scoped_ptr<base::DictionaryValue> ToDictionary() const;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_AUTOCHECKOUT_STATISTIC_H__
