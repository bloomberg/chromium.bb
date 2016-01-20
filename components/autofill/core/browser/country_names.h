// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_COUNTRY_NAMES_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_COUNTRY_NAMES_H_

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "third_party/icu/source/i18n/unicode/coll.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace autofill {

// A singleton class that encapsulates mappings from country names to their
// corresponding country codes.
class CountryNames {
 public:
  static CountryNames* GetInstance();

  // Returns the country code corresponding to |country|, which should be a
  // country code or country name localized to |locale_name|. This function
  // can be expensive so use judiciously.
  const std::string GetCountryCode(const base::string16& country,
                                   const std::string& locale_name);

 private:
  CountryNames();
  ~CountryNames();
  friend struct base::DefaultSingletonTraits<CountryNames>;

  // Populates |locales_to_localized_names_| with the mapping of country names
  // localized to |locale| to their corresponding country codes. Uses a
  // |collator| which is suitable for the locale.
  void AddLocalizedNamesForLocale(const std::string& locale,
                                  const icu::Collator& collator);

  // Interprets |country_name| as a full country name localized to the given
  // |locale| and returns the corresponding country code stored in
  // |locales_to_localized_names_|, or an empty string if there is none.
  const std::string GetCountryCodeForLocalizedName(
      const base::string16& country_name,
      const icu::Locale& locale);

  // Returns an ICU collator -- i.e. string comparator -- appropriate for the
  // given |locale|, or null if no collator is available.
  const icu::Collator* GetCollatorForLocale(const icu::Locale& locale);

  // Maps from common country names, including 2- and 3-letter country codes,
  // to the corresponding 2-letter country codes. The keys are uppercase ASCII
  // strings.
  const std::map<std::string, std::string> common_names_;

  // The outer map keys are ICU locale identifiers.
  // The inner maps map from localized country names to their corresponding
  // country codes. The inner map keys are ICU collation sort keys corresponding
  // to the target localized country name.
  std::map<std::string, std::map<std::string, std::string>>
      locales_to_localized_names_;

  // Maps ICU locale names to their corresponding collators.
  std::map<std::string, scoped_ptr<icu::Collator>> collators_;

  DISALLOW_COPY_AND_ASSIGN(CountryNames);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_COUNTRY_NAMES_H_
