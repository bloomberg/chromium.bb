// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_regexes.h"

#include "base/containers/scoped_ptr_hash_map.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/re2/re2/re2.h"

namespace {

// A singleton class that serves as a cache of compiled regex patterns.
class AutofillRegexes {
 public:
  static AutofillRegexes* GetInstance();

  // Returns the compiled regex matcher corresponding to |pattern|.
  re2::RE2* GetMatcher(const std::string& pattern);

 private:
  AutofillRegexes();
  ~AutofillRegexes();
  friend struct base::DefaultSingletonTraits<AutofillRegexes>;

  // Maps patterns to their corresponding regex matchers.
  base::ScopedPtrHashMap<std::string, scoped_ptr<re2::RE2>> matchers_;

  DISALLOW_COPY_AND_ASSIGN(AutofillRegexes);
};

// static
AutofillRegexes* AutofillRegexes::GetInstance() {
  return base::Singleton<AutofillRegexes>::get();
}

AutofillRegexes::AutofillRegexes() {
}

AutofillRegexes::~AutofillRegexes() {
}

re2::RE2* AutofillRegexes::GetMatcher(const std::string& pattern) {
  auto it = matchers_.find(pattern);
  if (it == matchers_.end()) {
    re2::RE2::Options options;
    options.set_case_sensitive(false);
    scoped_ptr<re2::RE2> matcher(new re2::RE2(pattern, options));
    DCHECK(matcher->ok());
    auto result = matchers_.add(pattern, matcher.Pass());
    DCHECK(result.second);
    it = result.first;
  }
  return it->second;
}

}  // namespace

namespace autofill {

bool MatchesPattern(const base::string16& input, const std::string& pattern) {
  // TODO(isherman): Run performance tests to determine whether caching regex
  // matchers is still useful now that we've switched from ICU to RE2.
  // http://crbug.com/470065
  re2::RE2* matcher = AutofillRegexes::GetInstance()->GetMatcher(pattern);
  return re2::RE2::PartialMatch(base::UTF16ToUTF8(input), *matcher);
}

}  // namespace autofill
