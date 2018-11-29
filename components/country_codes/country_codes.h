// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Please refer to ISO 3166-1 for information about the two-character country
// codes; http://en.wikipedia.org/wiki/ISO_3166-1_alpha-2 is useful. In the
// following (C++) code, we pack the two letters of the country code into an int
// value we call the CountryID.

#ifndef COMPONENTS_COUNTRY_CODES_COUNTRY_CODES_H_
#define COMPONENTS_COUNTRY_CODES_COUNTRY_CODES_H_

#include <string>

class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace country_codes {

// Integer containing the system Country ID the first time we checked the
// template URL prepopulate data.  This is used to avoid adding a whole bunch of
// new search engine choices if prepopulation runs when the user's Country ID
// differs from their previous Country ID.  This pref does not exist until
// prepopulation has been run at least once.
extern const char kCountryIDAtInstall[];

const int kCountryIDUnknown = -1;

// Takes in each of the two characters of a ISO 3166-1 country code, and
// converts it into an int value to be used as a reference to that country.
constexpr int CountryCharsToCountryID(char c1, char c2) {
  return c1 << 8 | c2;
}

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

// Returns the identifier for the user current country. Used to update the list
// of search engines when user switches device region settings. For use on iOS
// only.
// TODO(ios): Once user can customize search engines ( http://crbug.com/153047 )
// this declaration should be removed and the definition in the .cc file be
// moved back to the anonymous namespace.
int GetCurrentCountryID();

// Converts a two-letter country code to an integer-based country identifier.
int CountryStringToCountryID(const std::string& country);

// Returns the country identifier that was stored at install. If no such pref
// is available, it will return identifier of the current country instead.
int GetCountryIDFromPrefs(PrefService* prefs);

}  // namespace country_codes

#endif  // COMPONENTS_COUNTRY_CODES_COUNTRY_CODES_H_
