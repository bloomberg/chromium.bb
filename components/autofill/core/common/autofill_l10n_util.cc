// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_l10n_util.h"

#include "base/i18n/string_compare.h"
#include "base/logging.h"

namespace autofill {
namespace l10n {

CaseInsensitiveCompare::CaseInsensitiveCompare() {
  UErrorCode error = U_ZERO_ERROR;
  collator_.reset(icu::Collator::createInstance(error));
  DCHECK(U_SUCCESS(error));
  collator_->setStrength(icu::Collator::PRIMARY);
}

CaseInsensitiveCompare::~CaseInsensitiveCompare() {
}

bool CaseInsensitiveCompare::StringsEqual(const base::string16& lhs,
                                          const base::string16& rhs) const {
  return base::i18n::CompareString16WithCollator(*collator_, lhs, rhs) ==
         UCOL_EQUAL;
}

}  // namespace l10n
}  // namespace autofill
