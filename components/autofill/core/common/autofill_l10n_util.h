// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "third_party/icu/source/i18n/unicode/coll.h"

namespace autofill {
namespace l10n {

// Assists with locale-aware case insensitive string comparisons.
class CaseInsensitiveCompare {
 public:
  CaseInsensitiveCompare();
  // Used for testing.
  explicit CaseInsensitiveCompare(const icu::Locale& locale);
  ~CaseInsensitiveCompare();

  bool StringsEqual(const base::string16& lhs, const base::string16& rhs) const;

 private:
  scoped_ptr<icu::Collator> collator_;

  DISALLOW_COPY_AND_ASSIGN(CaseInsensitiveCompare);
};

}  // namespace l10n
}  // namespace autofill
