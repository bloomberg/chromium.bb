// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_data_model.h"

#include "components/autofill/core/browser/autofill_type.h"
#include "url/gurl.h"

namespace autofill {

AutofillDataModel::AutofillDataModel(const std::string& guid,
                                     const std::string& origin)
    : guid_(guid), origin_(origin), use_count_(0) {
}
AutofillDataModel::~AutofillDataModel() {}

bool AutofillDataModel::IsVerified() const {
  return !origin_.empty() && !GURL(origin_).is_valid();
}

void AutofillDataModel::RecordUse() {
  ++use_count_;
  use_date_ = base::Time::Now();
}

}  // namespace autofill
