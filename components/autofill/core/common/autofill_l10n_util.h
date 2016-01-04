// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_L10N_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_L10N_UTIL_H_

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "third_party/icu/source/i18n/unicode/coll.h"

namespace autofill {
namespace l10n {

// TODO(crbug.com/571610): Replace with just icu::Collator once use-after-free
// is eliminated.
// This is a wrapper around icu::Collator checking the collator's validity.
class CollatorWrapper {
 public:
  explicit CollatorWrapper(scoped_ptr<icu::Collator> collator);

  ~CollatorWrapper();

  const icu::Collator* collator() const {
    CHECK(is_valid_);
    return collator_.get();
  }

  icu::Collator* collator() {
    return const_cast<icu::Collator*>(
        static_cast<const CollatorWrapper*>(this)->collator());
  }

 private:
  const scoped_ptr<icu::Collator> collator_;
  bool is_valid_;
};

// Obtains the ICU Collator for this locale. If unsuccessful, attempts to return
// the ICU collator for the English locale. If unsuccessful, returns null.
scoped_ptr<CollatorWrapper> GetCollatorForLocale(const icu::Locale& locale);

// Assists with locale-aware case insensitive string comparisons.
class CaseInsensitiveCompare {
 public:
  CaseInsensitiveCompare();
  // Used for testing.
  explicit CaseInsensitiveCompare(const icu::Locale& locale);
  ~CaseInsensitiveCompare();

  bool StringsEqual(const base::string16& lhs, const base::string16& rhs) const;

 private:
  scoped_ptr<CollatorWrapper> collator_;

  DISALLOW_COPY_AND_ASSIGN(CaseInsensitiveCompare);
};

}  // namespace l10n
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_L10N_UTIL_H_
