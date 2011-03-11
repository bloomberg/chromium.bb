// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_country.h"

#include <map>
#include <utility>

#include "base/scoped_ptr.h"
#include "base/singleton.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util_collator.h"
#include "ui/base/l10n/l10n_util.h"
#include "unicode/coll.h"
#include "unicode/locid.h"
#include "unicode/sortkey.h"
#include "unicode/ucol.h"
#include "unicode/uloc.h"

namespace {

struct CountryData {
  std::string country_code;
  int postal_code_label_id;
  int state_label_id;
};

// The maximum capacity needed to store a locale up to the country code.
const size_t kLocaleCapacity =
    ULOC_LANG_CAPACITY + ULOC_SCRIPT_CAPACITY + ULOC_COUNTRY_CAPACITY + 1;

// Maps country codes to localized label string identifiers.
const CountryData kCountryData[] = {
  {"AD", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PARISH},
  {"AE", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_EMIRATE},
  {"AF", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"AG", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"AI", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"AL", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"AM", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"AN", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"AO", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"AQ", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"AR", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_STATE},
  {"AS", IDS_AUTOFILL_DIALOG_ZIP_CODE,    IDS_AUTOFILL_DIALOG_STATE},
  {"AT", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"AU", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_STATE},
  {"AW", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"AX", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"AZ", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"BA", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"BB", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PARISH},
  {"BD", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"BE", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"BF", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"BG", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"BH", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"BI", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"BJ", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"BL", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"BM", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"BN", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"BO", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"BR", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_STATE},
  {"BS", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_ISLAND},
  {"BT", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"BV", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"BW", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"BY", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"BZ", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"CA", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"CC", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"CD", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"CF", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"CG", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"CH", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"CI", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"CK", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"CL", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_STATE},
  {"CM", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"CN", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"CO", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"CR", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"CV", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_ISLAND},
  {"CX", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"CY", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"CZ", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"DE", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"DJ", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"DK", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"DM", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"DO", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"DZ", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"EC", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"EE", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"EG", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"EH", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"ER", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"ES", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"ET", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"FI", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"FJ", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"FK", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"FM", IDS_AUTOFILL_DIALOG_ZIP_CODE,    IDS_AUTOFILL_DIALOG_STATE},
  {"FO", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"FR", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"GA", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"GB", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_COUNTY},
  {"GD", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"GE", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"GF", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"GG", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"GH", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"GI", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"GL", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"GM", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"GN", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"GP", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"GQ", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"GR", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"GS", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"GT", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"GU", IDS_AUTOFILL_DIALOG_ZIP_CODE,    IDS_AUTOFILL_DIALOG_STATE},
  {"GW", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"GY", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"HK", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_AREA},
  {"HM", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"HN", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"HR", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"HT", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"HU", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"ID", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"IE", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_COUNTY},
  {"IL", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"IM", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"IN", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_STATE},
  {"IO", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"IQ", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"IS", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"IT", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"JE", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"JM", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PARISH},
  {"JO", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"JP", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PREFECTURE},
  {"KE", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"KG", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"KH", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"KI", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_ISLAND},
  {"KM", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"KN", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_ISLAND},
  {"KP", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"KR", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"KW", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"KY", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_ISLAND},
  {"KZ", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"LA", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"LB", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"LC", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"LI", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"LK", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"LR", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"LS", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"LT", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"LU", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"LV", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"LY", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"MA", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"MC", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"MD", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"ME", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"MF", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"MG", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"MH", IDS_AUTOFILL_DIALOG_ZIP_CODE,    IDS_AUTOFILL_DIALOG_STATE},
  {"MK", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"ML", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"MN", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"MO", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"MP", IDS_AUTOFILL_DIALOG_ZIP_CODE,    IDS_AUTOFILL_DIALOG_STATE},
  {"MQ", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"MR", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"MS", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"MT", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"MU", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"MV", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"MW", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"MX", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_STATE},
  {"MY", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_STATE},
  {"MZ", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"NA", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"NC", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"NE", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"NF", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"NG", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_STATE},
  {"NI", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_DEPARTMENT},
  {"NL", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"NO", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"NP", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"NR", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_DISTRICT},
  {"NU", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"NZ", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"OM", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"PA", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"PE", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"PF", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_ISLAND},
  {"PG", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"PH", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"PK", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"PL", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"PM", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"PN", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"PR", IDS_AUTOFILL_DIALOG_ZIP_CODE,    IDS_AUTOFILL_DIALOG_PROVINCE},
  {"PS", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"PT", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"PW", IDS_AUTOFILL_DIALOG_ZIP_CODE,    IDS_AUTOFILL_DIALOG_STATE},
  {"PY", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"QA", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"RE", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"RO", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"RS", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"RU", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"RW", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"SA", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"SB", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"SC", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_ISLAND},
  {"SE", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"SG", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"SH", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"SI", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"SJ", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"SK", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"SL", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"SM", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"SN", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"SO", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"SR", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"ST", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"SV", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"SZ", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"TC", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"TD", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"TF", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"TG", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"TH", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"TJ", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"TK", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"TL", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"TM", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"TN", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"TO", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"TR", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"TT", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"TV", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_ISLAND},
  {"TW", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_COUNTY},
  {"TZ", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"UA", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"UG", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"UM", IDS_AUTOFILL_DIALOG_ZIP_CODE,    IDS_AUTOFILL_DIALOG_STATE},
  {"US", IDS_AUTOFILL_DIALOG_ZIP_CODE,    IDS_AUTOFILL_DIALOG_STATE},
  {"UY", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"UZ", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"VA", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"VC", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"VE", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"VG", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"VI", IDS_AUTOFILL_DIALOG_ZIP_CODE,    IDS_AUTOFILL_DIALOG_STATE},
  {"VN", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"VU", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"WF", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"WS", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"YE", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"YT", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"ZA", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"ZM", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
  {"ZW", IDS_AUTOFILL_DIALOG_POSTAL_CODE, IDS_AUTOFILL_DIALOG_PROVINCE},
};

// A singleton class that encapsulates a map from country codes to country data.
class CountryDataMap {
 public:
  // A const iterator over the wrapped map data.
  typedef std::map<std::string, CountryData>::const_iterator Iterator;

  static CountryDataMap* GetInstance();
  static const Iterator Begin();
  static const Iterator End();
  static const Iterator Find(const std::string& country_code);

 private:
  CountryDataMap();
  friend struct DefaultSingletonTraits<CountryDataMap>;

  std::map<std::string, CountryData> country_data_;

  DISALLOW_COPY_AND_ASSIGN(CountryDataMap);
};

// static
CountryDataMap* CountryDataMap::GetInstance() {
  return Singleton<CountryDataMap>::get();
}

CountryDataMap::CountryDataMap() {
  // Add all the countries we have explicit data for.
  for (size_t i = 0; i < arraysize(kCountryData); ++i) {
    const CountryData& data = kCountryData[i];
    country_data_.insert(std::make_pair(data.country_code, data));
  }

  // Add any other countries that ICU knows about, falling back to default data
  // values.
  for (const char* const* country_pointer = Locale::getISOCountries();
       *country_pointer;
       ++country_pointer) {
    std::string country_code = *country_pointer;
    if (!country_data_.count(country_code)) {
      CountryData data = {
        country_code,
        IDS_AUTOFILL_DIALOG_POSTAL_CODE,
        IDS_AUTOFILL_DIALOG_PROVINCE
      };
      country_data_.insert(std::make_pair(country_code, data));
    }
  }
}

const CountryDataMap::Iterator CountryDataMap::Begin() {
  return GetInstance()->country_data_.begin();
}

const CountryDataMap::Iterator CountryDataMap::End() {
  return GetInstance()->country_data_.end();
}

const CountryDataMap::Iterator CountryDataMap::Find(
    const std::string& country_code) {
  return GetInstance()->country_data_.find(country_code);
}

// A singleton class that encapsulates mappings from country names to their
// corresponding country codes.
class CountryNames {
 public:
  static CountryNames* GetInstance();

  // Returns the country code corresponding to |country|, which should be a
  // country code or country name localized to |locale|.
  const std::string GetCountryCode(const string16& country,
                                   const std::string& locale);

 private:
  CountryNames();
  friend struct DefaultSingletonTraits<CountryNames>;

  // Populates |locales_to_localized_names_| with the mapping of country names
  // localized to |locale| to their corresponding country codes.
  void AddLocalizedNamesForLocale(const std::string& locale);

  // Interprets |country_name| as a full country name localized to the given
  // |locale| and returns the corresponding country code stored in
  // |locales_to_localized_names_|, or an empty string if there is none.
  const std::string GetCountryCodeForLocalizedName(const string16& country_name,
                                                   const std::string& locale);

  // Returns an ICU collator -- i.e. string comparator -- appropriate for the
  // given |locale|. The caller owns the returned value.
  icu::Collator* GetCollatorForLocale(const icu::Locale& locale) const;

  // Returns the ICU sort key corresponding to |str| for the given |collator|.
  // Uses |buffer| as temporary storage, and might resize |buffer| as a side-
  // effect. |buffer_size| should specify the |buffer|'s size, and is updated if
  // the |buffer| is resized.
  const std::string GetSortKey(const icu::Collator& collator,
                               const icu::UnicodeString& str,
                               scoped_array<uint8_t>* buffer,
                               int32_t* buffer_size) const;


  // Maps from common country names, including 2- and 3-letter country codes,
  // to the corresponding 2-letter country codes. The keys are uppercase ASCII
  // strings.
  std::map<std::string, std::string> common_names_;

  // The outer map keys are ICU locale identifiers.
  // The inner maps map from localized country names to their corresponding
  // country codes. The inner map keys are ICU collation sort keys corresponding
  // to the target localized country name.
  std::map<std::string, std::map<std::string, std::string> >
      locales_to_localized_names_;

  DISALLOW_COPY_AND_ASSIGN(CountryNames);
};

// static
CountryNames* CountryNames::GetInstance() {
  return Singleton<CountryNames>::get();
}

CountryNames::CountryNames() {
  // Add 2- and 3-letter ISO country codes.
  for (CountryDataMap::Iterator it = CountryDataMap::Begin();
       it != CountryDataMap::End();
       ++it) {
    const std::string& country_code = it->first;
    std::string iso3_country_code =
    icu::Locale(NULL, country_code.c_str()).getISO3Country();

    common_names_.insert(std::make_pair(country_code, country_code));
    common_names_.insert(std::make_pair(iso3_country_code, country_code));
  }

  // Add a few other common synonyms.
  common_names_.insert(std::make_pair("UNITED STATES OF AMERICA", "US"));
  common_names_.insert(std::make_pair("GREAT BRITAIN", "GB"));
  common_names_.insert(std::make_pair("UK", "GB"));
  common_names_.insert(std::make_pair("BRASIL", "BR"));
  common_names_.insert(std::make_pair("DEUTSCHLAND", "DE"));
}

const std::string CountryNames::GetCountryCode(const string16& country,
                                               const std::string& locale) {
  // First, check common country names, including 2- and 3-letter country codes.
  std::string country_utf8 = UTF16ToUTF8(StringToUpperASCII(country));
  std::map<std::string, std::string>::const_iterator result =
      common_names_.find(country_utf8);
  if (result != common_names_.end())
    return result->second;

  // Next, check country names localized to |locale|.
  std::string country_code = GetCountryCodeForLocalizedName(country, locale);
  if (!country_code.empty())
    return country_code;

  // Finally, check country names localized to US English.
  return GetCountryCodeForLocalizedName(country, "en_US");
}

void CountryNames::AddLocalizedNamesForLocale(const std::string& locale) {
  // Nothing to do if we've previously added the localized names for the given
  // |locale|.
  if (locales_to_localized_names_.count(locale))
    return;

  std::map<std::string, std::string> localized_names;

  icu::Locale icu_locale(locale.c_str());
  scoped_ptr<icu::Collator> collator(GetCollatorForLocale(icu_locale));

  int32_t buffer_size = 1000;
  scoped_array<uint8_t> buffer(new uint8_t[buffer_size]);

  for (CountryDataMap::Iterator it = CountryDataMap::Begin();
       it != CountryDataMap::End();
       ++it) {
    const std::string& country_code = it->first;

    icu::Locale country_locale(NULL, country_code.c_str());
    icu::UnicodeString country_name;
    country_locale.getDisplayName(icu_locale, country_name);
    std::string sort_key = GetSortKey(*collator.get(),
                                      country_name,
                                      &buffer,
                                      &buffer_size);

    localized_names.insert(std::make_pair(sort_key, country_code));
  }

  locales_to_localized_names_.insert(std::make_pair(locale, localized_names));
}

const std::string CountryNames::GetCountryCodeForLocalizedName(
    const string16& country_name,
    const std::string& locale) {
  AddLocalizedNamesForLocale(locale);

  const icu::Locale icu_locale(locale.c_str());
  scoped_ptr<icu::Collator> collator(GetCollatorForLocale(icu_locale));

  // As recommended[1] by ICU, initialize the buffer size to four times the
  // source string length.
  // [1] http://userguide.icu-project.org/collation/api#TOC-Examples
  int32_t buffer_size = country_name.size() * 4;
  scoped_array<uint8_t> buffer(new uint8_t[buffer_size]);
  std::string sort_key = GetSortKey(*collator.get(),
                                    country_name.c_str(),
                                    &buffer,
                                    &buffer_size);

  const std::map<std::string, std::string>& localized_names =
      locales_to_localized_names_[locale];
  std::map<std::string, std::string>::const_iterator result =
      localized_names.find(sort_key);

  if (result != localized_names.end())
    return result->second;

  return std::string();
}


icu::Collator* CountryNames::GetCollatorForLocale(
    const icu::Locale& locale) const {
  UErrorCode ignored = U_ZERO_ERROR;
  icu::Collator* collator(icu::Collator::createInstance(locale, ignored));

  // Compare case-insensitively and ignoring punctuation.
  ignored = U_ZERO_ERROR;
  collator->setAttribute(UCOL_STRENGTH, UCOL_SECONDARY, ignored);
  ignored = U_ZERO_ERROR;
  collator->setAttribute(UCOL_ALTERNATE_HANDLING, UCOL_SHIFTED, ignored);

  return collator;
}

const std::string CountryNames::GetSortKey(const icu::Collator& collator,
                                           const icu::UnicodeString& str,
                                           scoped_array<uint8_t>* buffer,
                                           int32_t* buffer_size) const {
  DCHECK(buffer);
  DCHECK(buffer_size);

  int32_t expected_size = collator.getSortKey(str, buffer->get(), *buffer_size);
  if (expected_size > *buffer_size) {
    // If there wasn't enough space, grow the buffer and try again.
    *buffer_size = expected_size;
    buffer->reset(new uint8_t[*buffer_size]);
    DCHECK(buffer->get());

    expected_size = collator.getSortKey(str, buffer->get(), *buffer_size);
    DCHECK_EQ(*buffer_size, expected_size);
  }

  return std::string(reinterpret_cast<const char*>(buffer->get()));
}

// Returns the country name corresponding to |country_code|, localized to the
// |display_locale|.
string16 GetDisplayName(const std::string& country_code,
                        const icu::Locale& display_locale) {
  icu::Locale country_locale(NULL, country_code.c_str());
  icu::UnicodeString name;
  country_locale.getDisplayName(display_locale, name);

  DCHECK_GT(name.length(), 0);
  return string16(name.getBuffer(), name.length());
}

}  // namespace

AutofillCountry::AutofillCountry(const std::string& country_code,
                                 const std::string& locale) {
  const CountryDataMap::Iterator result = CountryDataMap::Find(country_code);
  DCHECK(result != CountryDataMap::End());
  const CountryData& data = result->second;

  country_code_ = country_code;
  name_ = GetDisplayName(country_code, icu::Locale(locale.c_str()));
  postal_code_label_ = l10n_util::GetStringUTF16(data.postal_code_label_id);
  state_label_ = l10n_util::GetStringUTF16(data.state_label_id);
}

AutofillCountry::~AutofillCountry() {
}

// static
void AutofillCountry::GetAvailableCountries(
    std::vector<std::string>* country_codes) {
  DCHECK(country_codes);

  for (CountryDataMap::Iterator it = CountryDataMap::Begin();
       it != CountryDataMap::End();
       ++it) {
    country_codes->push_back(it->first);
  }
}

// static
const std::string AutofillCountry::CountryCodeForLocale(
    const std::string& locale) {
  // Add likely subtags to the locale. In particular, add any likely country
  // subtags -- e.g. for locales like "ru" that only include the language.
  std::string likely_locale;
  UErrorCode error_ignored = U_ZERO_ERROR;
  uloc_addLikelySubtags(locale.c_str(),
                        WriteInto(&likely_locale, kLocaleCapacity),
                        kLocaleCapacity,
                        &error_ignored);

  // Extract the country code.
  std::string country_code = icu::Locale(likely_locale.c_str()).getCountry();

  // TODO(isherman): Return an empty string and update the clients instead.
  // Default to the United States if we have no better guess.
  if (CountryDataMap::Find(country_code) == CountryDataMap::End())
    return "US";

  return country_code;
}

// static
const std::string AutofillCountry::GetCountryCode(const string16& country,
                                                  const std::string& locale) {
  return CountryNames::GetInstance()->GetCountryCode(country, locale);
}

// static
const std::string AutofillCountry::ApplicationLocale() {
  return g_browser_process->GetApplicationLocale();
}

AutofillCountry::AutofillCountry(const std::string& country_code,
                                 const string16& name,
                                 const string16& postal_code_label,
                                 const string16& state_label)
    : country_code_(country_code),
      name_(name),
      postal_code_label_(postal_code_label),
      state_label_(state_label) {
}
