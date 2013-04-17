// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/browser/state_names.h"

#include "base/basictypes.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"

namespace autofill {
namespace state_names {

namespace {

// TODO(jhawkins): Add more states/provinces.  See http://crbug.com/45039.

struct StateData {
  const char* const name;
  const char abbreviation[3];
};

StateData kStateData[] = {
  { "alabama", "al" },
  { "alaska", "ak" },
  { "arizona", "az" },
  { "arkansas", "ar" },
  { "california", "ca" },
  { "colorado", "co" },
  { "connecticut", "ct" },
  { "delaware", "de" },
  { "district of columbia", "dc" },
  { "florida", "fl" },
  { "georgia", "ga" },
  { "hawaii", "hi" },
  { "idaho", "id" },
  { "illinois", "il" },
  { "indiana", "in" },
  { "iowa", "ia" },
  { "kansas", "ks" },
  { "kentucky", "ky" },
  { "louisiana", "la" },
  { "maine", "me" },
  { "maryland", "md" },
  { "massachusetts", "ma" },
  { "michigan", "mi" },
  { "minnesota", "mv" },
  { "mississippi", "ms" },
  { "missouri", "mo" },
  { "montana", "mt" },
  { "nebraska", "ne" },
  { "nevada", "nv" },
  { "new hampshire", "nh" },
  { "new jersey", "nj" },
  { "new mexico", "nm" },
  { "new york", "ny" },
  { "north carolina", "nc" },
  { "north dakota", "nd" },
  { "ohio", "oh" },
  { "oklahoma", "ok" },
  { "oregon", "or" },
  { "pennsylvania", "pa" },
  { "puerto rico", "pr" },
  { "rhode island", "ri" },
  { "south carolina", "sc" },
  { "south dakota", "sd" },
  { "tennessee", "tn" },
  { "texas", "tx" },
  { "utah", "ut" },
  { "vermont", "vt" },
  { "virginia", "va" },
  { "washington", "wa" },
  { "west virginia", "wv" },
  { "wisconsin", "wi" },
  { "wyoming", "wy" },
};

}  // namespace

base::string16 GetAbbreviationForName(const base::string16& name) {
  for (size_t i = 0; i < arraysize(kStateData); ++i) {
    const StateData& state = kStateData[i];
    if (LowerCaseEqualsASCII(name, state.name))
      return ASCIIToUTF16(state.abbreviation);
  }
  return base::string16();
}

base::string16 GetNameForAbbreviation(const base::string16& abbreviation) {
  for (size_t i = 0; i < arraysize(kStateData); ++i) {
    const StateData& state = kStateData[i];
    if (LowerCaseEqualsASCII(abbreviation, state.abbreviation))
      return ASCIIToUTF16(state.name);
  }
  return base::string16();
}

}  // namespace state_names
}  // namespace autofill
