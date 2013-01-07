// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_regexes.h"

#include <map>
#include <utility>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/stl_util.h"
#include "base/string16.h"
#include "third_party/icu/public/i18n/unicode/regex.h"

namespace {

// A singleton class that serves as a cache of compiled regex patterns.
class AutofillRegexes {
 public:
  static AutofillRegexes* GetInstance();

  // Returns the compiled regex matcher corresponding to |pattern|.
  icu::RegexMatcher* GetMatcher(const string16& pattern);

 private:
  AutofillRegexes();
  ~AutofillRegexes();
  friend struct DefaultSingletonTraits<AutofillRegexes>;

  // Maps patterns to their corresponding regex matchers.
  std::map<string16, icu::RegexMatcher*> matchers_;

  DISALLOW_COPY_AND_ASSIGN(AutofillRegexes);
};

// static
AutofillRegexes* AutofillRegexes::GetInstance() {
  return Singleton<AutofillRegexes>::get();
}

AutofillRegexes::AutofillRegexes() {
}

AutofillRegexes::~AutofillRegexes() {
  STLDeleteContainerPairSecondPointers(matchers_.begin(),
                                       matchers_.end());
}

icu::RegexMatcher* AutofillRegexes::GetMatcher(const string16& pattern) {
  if (!matchers_.count(pattern)) {
    const icu::UnicodeString icu_pattern(pattern.data(), pattern.length());

    UErrorCode status = U_ZERO_ERROR;
    icu::RegexMatcher* matcher = new icu::RegexMatcher(icu_pattern,
                                                       UREGEX_CASE_INSENSITIVE,
                                                       status);
    DCHECK(U_SUCCESS(status));

    matchers_.insert(std::make_pair(pattern, matcher));
  }

  return matchers_[pattern];
}

}  // namespace

namespace autofill {

bool MatchesPattern(const string16& input, const string16& pattern) {
  icu::RegexMatcher* matcher =
      AutofillRegexes::GetInstance()->GetMatcher(pattern);
  icu::UnicodeString icu_input(input.data(), input.length());
  matcher->reset(icu_input);

  UErrorCode status = U_ZERO_ERROR;
  UBool match = matcher->find(0, status);
  DCHECK(U_SUCCESS(status));
  return !!match;
}

}  // namespace autofill
