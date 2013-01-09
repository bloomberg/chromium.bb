// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_country.h"

#include <stddef.h>
#include <stdint.h>
#include <map>
#include <utility>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "base/threading/thread_checker.h"
#include "base/utf_string_conversions.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "grit/generated_resources.h"
#include "third_party/icu/public/common/unicode/locid.h"
#include "third_party/icu/public/common/unicode/uloc.h"
#include "third_party/icu/public/common/unicode/unistr.h"
#include "third_party/icu/public/common/unicode/urename.h"
#include "third_party/icu/public/common/unicode/utypes.h"
#include "third_party/icu/public/i18n/unicode/coll.h"
#include "third_party/icu/public/i18n/unicode/ucol.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;

namespace {

// The maximum capacity needed to store a locale up to the country code.
const size_t kLocaleCapacity =
    ULOC_LANG_CAPACITY + ULOC_SCRIPT_CAPACITY + ULOC_COUNTRY_CAPACITY + 1;

struct CountryData {
  int postal_code_label_id;
  int state_label_id;
};

struct StaticCountryData {
  char country_code[3];
  CountryData country_data;
};

// Maps country codes to localized label string identifiers.
const StaticCountryData kCountryData[] = {
  { "AD", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PARISH } },
  { "AE", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_EMIRATE } },
  { "AF", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "AG", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "AI", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "AL", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "AM", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "AN", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "AO", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "AQ", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "AR", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_STATE } },
  { "AS", { IDS_AUTOFILL_FIELD_LABEL_ZIP_CODE,
            IDS_AUTOFILL_FIELD_LABEL_STATE } },
  { "AT", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "AU", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_STATE } },
  { "AW", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "AX", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "AZ", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "BA", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "BB", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PARISH } },
  { "BD", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "BE", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "BF", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "BG", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "BH", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "BI", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "BJ", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "BL", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "BM", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "BN", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "BO", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "BR", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_STATE } },
  { "BS", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_ISLAND } },
  { "BT", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "BV", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "BW", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "BY", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "BZ", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "CA", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "CC", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "CD", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "CF", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "CG", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "CH", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "CI", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "CK", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "CL", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_STATE } },
  { "CM", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "CN", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "CO", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "CR", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "CV", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_ISLAND } },
  { "CX", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "CY", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "CZ", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "DE", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "DJ", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "DK", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "DM", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "DO", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "DZ", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "EC", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "EE", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "EG", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "EH", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "ER", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "ES", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "ET", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "FI", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "FJ", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "FK", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "FM", { IDS_AUTOFILL_FIELD_LABEL_ZIP_CODE,
            IDS_AUTOFILL_FIELD_LABEL_STATE } },
  { "FO", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "FR", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "GA", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "GB", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_COUNTY } },
  { "GD", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "GE", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "GF", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "GG", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "GH", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "GI", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "GL", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "GM", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "GN", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "GP", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "GQ", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "GR", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "GS", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "GT", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "GU", { IDS_AUTOFILL_FIELD_LABEL_ZIP_CODE,
            IDS_AUTOFILL_FIELD_LABEL_STATE } },
  { "GW", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "GY", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "HK", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_AREA } },
  { "HM", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "HN", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "HR", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "HT", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "HU", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "ID", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "IE", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_COUNTY } },
  { "IL", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "IM", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "IN", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_STATE } },
  { "IO", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "IQ", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "IS", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "IT", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "JE", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "JM", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PARISH } },
  { "JO", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "JP", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PREFECTURE } },
  { "KE", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "KG", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "KH", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "KI", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_ISLAND } },
  { "KM", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "KN", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_ISLAND } },
  { "KP", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "KR", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "KW", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "KY", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_ISLAND } },
  { "KZ", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "LA", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "LB", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "LC", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "LI", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "LK", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "LR", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "LS", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "LT", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "LU", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "LV", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "LY", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "MA", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "MC", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "MD", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "ME", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "MF", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "MG", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "MH", { IDS_AUTOFILL_FIELD_LABEL_ZIP_CODE,
            IDS_AUTOFILL_FIELD_LABEL_STATE } },
  { "MK", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "ML", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "MN", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "MO", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "MP", { IDS_AUTOFILL_FIELD_LABEL_ZIP_CODE,
            IDS_AUTOFILL_FIELD_LABEL_STATE } },
  { "MQ", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "MR", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "MS", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "MT", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "MU", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "MV", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "MW", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "MX", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_STATE } },
  { "MY", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_STATE } },
  { "MZ", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "NA", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "NC", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "NE", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "NF", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "NG", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_STATE } },
  { "NI", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_DEPARTMENT } },
  { "NL", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "NO", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "NP", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "NR", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_DISTRICT } },
  { "NU", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "NZ", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "OM", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "PA", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "PE", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "PF", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_ISLAND } },
  { "PG", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "PH", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "PK", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "PL", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "PM", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "PN", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "PR", { IDS_AUTOFILL_FIELD_LABEL_ZIP_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "PS", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "PT", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "PW", { IDS_AUTOFILL_FIELD_LABEL_ZIP_CODE,
            IDS_AUTOFILL_FIELD_LABEL_STATE } },
  { "PY", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "QA", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "RE", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "RO", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "RS", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "RU", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "RW", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "SA", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "SB", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "SC", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_ISLAND } },
  { "SE", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "SG", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "SH", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "SI", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "SJ", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "SK", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "SL", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "SM", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "SN", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "SO", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "SR", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "ST", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "SV", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "SZ", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "TC", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "TD", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "TF", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "TG", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "TH", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "TJ", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "TK", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "TL", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "TM", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "TN", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "TO", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "TR", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "TT", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "TV", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_ISLAND } },
  { "TW", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_COUNTY } },
  { "TZ", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "UA", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "UG", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "UM", { IDS_AUTOFILL_FIELD_LABEL_ZIP_CODE,
            IDS_AUTOFILL_FIELD_LABEL_STATE } },
  { "US", { IDS_AUTOFILL_FIELD_LABEL_ZIP_CODE,
            IDS_AUTOFILL_FIELD_LABEL_STATE } },
  { "UY", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "UZ", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "VA", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "VC", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "VE", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "VG", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "VI", { IDS_AUTOFILL_FIELD_LABEL_ZIP_CODE,
            IDS_AUTOFILL_FIELD_LABEL_STATE } },
  { "VN", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "VU", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "WF", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "WS", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "YE", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "YT", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "ZA", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "ZM", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
  { "ZW", { IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
            IDS_AUTOFILL_FIELD_LABEL_PROVINCE } },
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
    const StaticCountryData& static_data = kCountryData[i];
    country_data_.insert(std::make_pair(static_data.country_code,
                                        static_data.country_data));
  }

  // Add any other countries that ICU knows about, falling back to default data
  // values.
  for (const char* const* country_pointer = icu::Locale::getISOCountries();
       *country_pointer;
       ++country_pointer) {
    std::string country_code = *country_pointer;
    if (!country_data_.count(country_code)) {
      CountryData data = {
        IDS_AUTOFILL_FIELD_LABEL_POSTAL_CODE,
        IDS_AUTOFILL_FIELD_LABEL_PROVINCE
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

  // Returns the application locale.
  const std::string ApplicationLocale();

  // Returns the country code corresponding to |country|, which should be a
  // country code or country name localized to |locale|.
  const std::string GetCountryCode(const string16& country,
                                   const std::string& locale);

 private:
  CountryNames();
  ~CountryNames();
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
  // given |locale|.
  icu::Collator* GetCollatorForLocale(const std::string& locale);

  // Returns the ICU sort key corresponding to |str| for the given |collator|.
  // Uses |buffer| as temporary storage, and might resize |buffer| as a side-
  // effect. |buffer_size| should specify the |buffer|'s size, and is updated if
  // the |buffer| is resized.
  const std::string GetSortKey(const icu::Collator& collator,
                               const string16& str,
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

  // Maps ICU locale names to their corresponding collators.
  std::map<std::string, icu::Collator*> collators_;

  // Verifies thread-safety of accesses to the application locale.
  base::ThreadChecker thread_checker_;

  // Caches the application locale, for thread-safe access.
  std::string application_locale_;

  DISALLOW_COPY_AND_ASSIGN(CountryNames);
};

// static
CountryNames* CountryNames::GetInstance() {
  return Singleton<CountryNames>::get();
}

const std::string CountryNames::ApplicationLocale() {
  if (application_locale_.empty()) {
    // In production code, this class is always constructed on the UI thread, so
    // the two conditions in the below DCHECK are identical.  In test code,
    // sometimes there is a UI thread, and sometimes there is just the unnamed
    // main thread.  Since this class is a singleton, it needs to support both
    // cases.  Hence, the somewhat strange looking DCHECK below.
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
           thread_checker_.CalledOnValidThread());
    application_locale_ =
        content::GetContentClient()->browser()->GetApplicationLocale();
  }

  return application_locale_;
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
  common_names_.insert(std::make_pair("U.S.A.", "US"));
  common_names_.insert(std::make_pair("GREAT BRITAIN", "GB"));
  common_names_.insert(std::make_pair("UK", "GB"));
  common_names_.insert(std::make_pair("BRASIL", "BR"));
  common_names_.insert(std::make_pair("DEUTSCHLAND", "DE"));
}

CountryNames::~CountryNames() {
  STLDeleteContainerPairSecondPointers(collators_.begin(),
                                       collators_.end());
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
  const icu::Collator* collator = GetCollatorForLocale(locale);
  int32_t buffer_size = 1000;
  scoped_array<uint8_t> buffer(new uint8_t[buffer_size]);

  for (CountryDataMap::Iterator it = CountryDataMap::Begin();
       it != CountryDataMap::End();
       ++it) {
    const std::string& country_code = it->first;
    string16 country_name = l10n_util::GetDisplayNameForCountry(country_code,
                                                                locale);
    std::string sort_key = GetSortKey(*collator,
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

  icu::Collator* collator = GetCollatorForLocale(locale);

  // As recommended[1] by ICU, initialize the buffer size to four times the
  // source string length.
  // [1] http://userguide.icu-project.org/collation/api#TOC-Examples
  int32_t buffer_size = country_name.size() * 4;
  scoped_array<uint8_t> buffer(new uint8_t[buffer_size]);
  std::string sort_key = GetSortKey(*collator,
                                    country_name,
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

icu::Collator* CountryNames::GetCollatorForLocale(const std::string& locale) {
  if (!collators_.count(locale)) {
    icu::Locale icu_locale(locale.c_str());
    UErrorCode ignored = U_ZERO_ERROR;
    icu::Collator* collator(icu::Collator::createInstance(icu_locale, ignored));

    // Compare case-insensitively and ignoring punctuation.
    ignored = U_ZERO_ERROR;
    collator->setAttribute(UCOL_STRENGTH, UCOL_SECONDARY, ignored);
    ignored = U_ZERO_ERROR;
    collator->setAttribute(UCOL_ALTERNATE_HANDLING, UCOL_SHIFTED, ignored);

    collators_.insert(std::make_pair(locale, collator));
  }

  return collators_[locale];
}

const std::string CountryNames::GetSortKey(const icu::Collator& collator,
                                           const string16& str,
                                           scoped_array<uint8_t>* buffer,
                                           int32_t* buffer_size) const {
  DCHECK(buffer);
  DCHECK(buffer_size);

  icu::UnicodeString icu_str(str.c_str(), str.length());
  int32_t expected_size = collator.getSortKey(icu_str, buffer->get(),
                                              *buffer_size);
  if (expected_size > *buffer_size) {
    // If there wasn't enough space, grow the buffer and try again.
    *buffer_size = expected_size;
    buffer->reset(new uint8_t[*buffer_size]);
    DCHECK(buffer->get());

    expected_size = collator.getSortKey(icu_str, buffer->get(), *buffer_size);
    DCHECK_EQ(*buffer_size, expected_size);
  }

  return std::string(reinterpret_cast<const char*>(buffer->get()));
}

}  // namespace

AutofillCountry::AutofillCountry(const std::string& country_code,
                                 const std::string& locale) {
  const CountryDataMap::Iterator result = CountryDataMap::Find(country_code);
  DCHECK(result != CountryDataMap::End());
  const CountryData& data = result->second;

  country_code_ = country_code;
  name_ = l10n_util::GetDisplayNameForCountry(country_code, locale);
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
  return CountryNames::GetInstance()->ApplicationLocale();
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
