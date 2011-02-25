// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/select_control_handler.h"

#include <string>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/form_group.h"
#include "webkit/glue/form_field.h"

namespace {

// TODO(jhawkins): Add more states/provinces.  See http://crbug.com/45039.

class State {
 public:
  const char* name;
  const char* abbreviation;

  static const State all_states[];

  static string16 Abbreviation(const string16& name);
  static string16 FullName(const string16& abbreviation);
};

const State State::all_states[] = {
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
  { "west Virginia", "wv" },
  { "wisconsin", "wi" },
  { "wyoming", "wy" },
  { NULL, NULL }
};

string16 State::Abbreviation(const string16& name) {
  for (const State *s = all_states ; s->name ; ++s)
    if (LowerCaseEqualsASCII(name, s->name))
      return ASCIIToUTF16(s->abbreviation);
  return string16();
}

string16 State::FullName(const string16& abbreviation) {
  for (const State *s = all_states ; s->name ; ++s)
    if (LowerCaseEqualsASCII(abbreviation, s->abbreviation))
      return ASCIIToUTF16(s->name);
  return string16();
}

// Holds valid country names, their 2 letter abbreviation, and maps from
// the former to the latter
class Country {
 public:
  const char* name;
  const char* abbreviation;

  static const Country all_countries[];

  static string16 Abbreviation(const string16 &name);
  static string16 FullName(const string16& abbreviation);
};

// A list of all English country names and code elements. ISO 3166.
const Country Country::all_countries[] = {
  { "united states", "us" },
  { "afghanistan", "af" },
  { "aland islands", "ax" },
  { "albania", "al" },
  { "algeria", "dz" },
  { "american samoa", "as" },
  { "andorra", "ad" },
  { "angola", "ao" },
  { "anguilla", "ai" },
  { "antarctica", "aq" },
  { "antigua and barbuda", "ag" },
  { "argentina", "ar" },
  { "armenia", "am" },
  { "aruba", "aw" },
  { "australia", "au" },
  { "austria", "at" },
  { "azerbaijan", "az" },
  { "bahamas", "bs" },
  { "bahrain", "bh" },
  { "bangladesh", "bd" },
  { "barbados", "bb" },
  { "belarus", "by" },
  { "belgium", "be" },
  { "belize", "bz" },
  { "benin", "bj" },
  { "bermuda", "bm" },
  { "bhutan", "bt" },
  { "bolivia", "bo" },
  { "bosnia and herzegovina", "ba" },
  { "botswana", "bw" },
  { "bouvet island", "bv" },
  { "brazil", "br" },
  { "british indian ocean territory", "io" },
  { "brunei darussalam", "bn" },
  { "bulgaria", "bg" },
  { "burkina faso", "bf" },
  { "burundi", "bi" },
  { "cambodia", "kh" },
  { "cameroon", "cm" },
  { "canada", "ca" },
  { "cape verde", "cv" },
  { "cayman islands", "ky" },
  { "central african republic", "cf" },
  { "chad", "td" },
  { "chile", "cl" },
  { "china", "cn" },
  { "christmas island", "cx" },
  { "cocos (keeling) islands", "cc" },
  { "colombia", "co" },
  { "comoros", "km" },
  { "congo", "cg" },
  { "congo (dem. rep.)", "cd" },
  { "cook islands", "ck" },
  { "costa rica", "cr" },
  { "cote d'ivoire", "ci" },
  { "croatia", "hr" },
  { "cuba", "cu" },
  { "cyprus", "cy" },
  { "czech republic", "cz" },
  { "denmark", "dk" },
  { "djibouti", "dj" },
  { "dominica", "dm" },
  { "dominican republic", "do" },
  { "ecuador", "ec" },
  { "egypt", "eg" },
  { "el salvador", "sv" },
  { "equatorial guinea", "gq" },
  { "eritrea", "er" },
  { "estonia", "ee" },
  { "ethiopia", "et" },
  { "falkland islands (malvinas)", "fk" },
  { "faroe islands", "fo" },
  { "fiji", "fj" },
  { "finland", "fi" },
  { "france", "fr" },
  { "french guiana", "gf" },
  { "french polynesia", "pf" },
  { "gabon", "ga" },
  { "gambia", "gm" },
  { "georgia", "ge" },
  { "germany", "de" },
  { "ghana", "gh" },
  { "gibraltar", "gi" },
  { "greece", "gr" },
  { "greenland", "gl" },
  { "grenada", "gd" },
  { "guadeloupe", "gp" },
  { "guam", "gu" },
  { "guatemala", "gt" },
  { "guernsey", "cg" },
  { "guinea", "gn" },
  { "guinea-bissau", "gw" },
  { "guyana", "gy" },
  { "haiti", "ht" },
  { "heard island and mcdonald islands", "hm" },
  { "holy see (vatica city state)", "va" },
  { "honduras", "hn" },
  { "hong kong", "hk" },
  { "hungary", "hu" },
  { "iceland", "is" },
  { "india", "in" },
  { "indonesia", "id" },
  { "iran", "ir" },
  { "iraq", "iq" },
  { "ireland", "ie" },
  { "isle of man", "im" },
  { "israel", "il" },
  { "italy", "it" },
  { "ivory coast", "ci" },
  { "jamaica", "jm" },
  { "japan", "jp" },
  { "jersey", "je" },
  { "jordan", "jo" },
  { "kazakhstan", "kz" },
  { "kenya", "ke" },
  { "kiribati", "ki" },
  { "korea (north)", "kp" },
  { "korea (south)", "kr" },
  { "kuwait", "kw" },
  { "kyrgyzstan", "kg" },
  { "laos", "la" },
  { "latvia", "lv" },
  { "lebanon", "lb" },
  { "lesotho", "ls" },
  { "liberia", "lr" },
  { "libya", "ly" },
  { "liechtenstein", "li" },
  { "lithuania", "lt" },
  { "luxembourg", "lu" },
  { "macao", "mo" },
  { "macedonia", "mk" },
  { "madagascar", "mg" },
  { "malawi", "mw" },
  { "malaysia", "my" },
  { "maldives", "mv" },
  { "mali", "ml" },
  { "malta", "mt" },
  { "marshall islands", "mh" },
  { "martinique", "mq" },
  { "mauritania", "mr" },
  { "mauritius", "mu" },
  { "mayotte", "yt" },
  { "mexico", "mx" },
  { "micronesia", "fm" },
  { "moldova", "md" },
  { "monaco", "mc" },
  { "mongolia", "mn" },
  { "montserrat", "ms" },
  { "morocco", "ma" },
  { "mozambique", "mz" },
  { "myanmar", "mm" },
  { "namibia", "na" },
  { "nepal", "np" },
  { "netherlands", "nl" },
  { "netherlands antilles", "an" },
  { "new caledonia", "nc" },
  { "new zealand", "nz" },
  { "nicaragua", "ni" },
  { "niger", "ne" },
  { "nigeria", "ng" },
  { "niue", "nu" },
  { "norfolk island", "nf" },
  { "northern mariana islands", "mp" },
  { "norway", "no" },
  { "oman", "om" },
  { "pakistan", "pk" },
  { "palau", "pw" },
  { "palestine", "ps" },
  { "panama", "pa" },
  { "papua new guinea", "pg" },
  { "paraguay", "py" },
  { "peru", "pe" },
  { "philippines", "ph" },
  { "pitcairn", "pn" },
  { "poland", "pl" },
  { "portugal", "pt" },
  { "puerto rico", "pr" },
  { "qatar", "qa" },
  { "reunion", "re" },
  { "romania", "ro" },
  { "russia", "ru" },
  { "rwanda", "rw" },
  { "saint helena", "sh" },
  { "saint kitts and nevis", "kn" },
  { "saint lucia", "lc" },
  { "saint pierre and miquelon", "pm" },
  { "saint vincent and the grenadines", "vc" },
  { "samoa", "ws" },
  { "san marino", "sm" },
  { "sao tome and principe", "st" },
  { "saudi arabia", "sa" },
  { "senegal", "sn" },
  { "serbia and montenegro", "cs" },
  { "seychelles", "sc" },
  { "sierra leone", "sl" },
  { "singapore", "sg" },
  { "slovakia", "sk" },
  { "slovenia", "si" },
  { "solomon islands", "sb" },
  { "somalia", "so" },
  { "south africa", "za" },
  { "south georgia and the south sandwich islands", "gs" },
  { "spain", "es" },
  { "sri lanka", "lk" },
  { "sudan", "sd" },
  { "suriname", "sr" },
  { "svalbard and jan mayen", "sj" },
  { "swaziland", "sz" },
  { "sweden", "se" },
  { "switzerland", "ch" },
  { "syria", "sy" },
  { "taiwan", "tw" },
  { "tajikistan", "tj" },
  { "tanzania", "tz" },
  { "thailand", "th" },
  { "timor-leste", "tl" },
  { "togo", "tg" },
  { "tokelau", "tk" },
  { "tonga", "to" },
  { "trinidad and tobago", "tt" },
  { "tunisia", "tn" },
  { "turkey", "tr" },
  { "turkmenistan", "tm" },
  { "turks and caicos islands", "tc" },
  { "tuvalu", "tv" },
  { "uganda", "ug" },
  { "ukraine", "ua" },
  { "united arab emirates", "ae" },
  { "united kingdom", "gb" },
  { "u.s. minor outlying islands", "um" },
  { "uruguay", "uy" },
  { "uzbekistan", "uz" },
  { "vanuatu", "vu" },
  { "venezuela", "ve" },
  { "vietnam", "vn" },
  { "virgin islands, british", "vg" },
  { "virgin islands, u.s.", "vi" },
  { "wallis and futuna", "wf" },
  { "western sahara", "eh" },
  { "yemen", "ye" },
  { "zambia", "zm" },
  { "zimbabwe", "zw" },
  { NULL, NULL }
};

string16 Country::Abbreviation(const string16& name) {
  for (const Country *s = all_countries ; s->name ; ++s)
    if (LowerCaseEqualsASCII(name, s->name))
      return ASCIIToUTF16(s->abbreviation);
  return string16();
}

string16 Country::FullName(const string16& abbreviation) {
  for (const Country *s = all_countries ; s->name ; ++s)
    if (LowerCaseEqualsASCII(abbreviation, s->abbreviation))
      return ASCIIToUTF16(s->name);
  return string16();
}

const char* const kMonthsAbbreviated[] = {
  NULL,  // Padding so index 1 = month 1 = January.
  "Jan", "Feb", "Mar", "Apr", "May", "Jun",
  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
};

const char* const kMonthsFull[] = {
  NULL,  // Padding so index 1 = month 1 = January.
  "January", "February", "March", "April", "May", "June",
  "July", "August", "September", "October", "November", "December",
};

const char* const kMonthsNumeric[] = {
  NULL,  // Padding so index 1 = month 1 = January.
  "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12",
};

// Returns true if the value was successfully set, meaning |value| was found in
// the list of select options in |field|.
bool SetSelectControlValue(const string16& value,
                           webkit_glue::FormField* field) {
  string16 value_lowercase = StringToLowerASCII(value);

  for (std::vector<string16>::const_iterator iter =
           field->option_strings().begin();
       iter != field->option_strings().end(); ++iter) {
    if (value_lowercase == StringToLowerASCII(*iter)) {
      field->set_value(*iter);
      return true;
    }
  }

  return false;
}

bool FillStateSelectControl(const string16& value,
                            webkit_glue::FormField* field) {
  if (value.empty())
    return false;

  string16 abbrev, full;
  if (value.size() < 4U) {
    abbrev = value;
    full = State::FullName(value);
  } else {
    abbrev = State::Abbreviation(value);
    full = value;
  }

  // Try the abbreviation name first.
  if (!abbrev.empty() && SetSelectControlValue(abbrev, field))
    return true;

  if (full.empty())
    return false;

  return SetSelectControlValue(full, field);
}

bool FillCountrySelectControl(const string16& value,
                              webkit_glue::FormField* field) {
  if (value.empty())
    return false;

  string16 abbrev, full;
  if (value.size() < 4U) {
    abbrev = value;
    full = Country::FullName(value);
  } else {
    abbrev = Country::Abbreviation(value);
    full = value;
  }

  // Try the abbreviation name first.
  if (!abbrev.empty() && SetSelectControlValue(abbrev, field))
    return true;

  if (full.empty())
    return false;

  return SetSelectControlValue(full, field);
}

bool FillExpirationMonthSelectControl(const string16& value,
                                      webkit_glue::FormField* field) {
  if (value.empty())
    return false;

  if (SetSelectControlValue(value, field))
    return true;

  int index = 0;
  if (!base::StringToInt(value, &index) ||
      index <= 0 ||
      static_cast<size_t>(index) >= arraysize(kMonthsFull))
    return false;

  bool filled =
      SetSelectControlValue(ASCIIToUTF16(kMonthsAbbreviated[index]), field) ||
      SetSelectControlValue(ASCIIToUTF16(kMonthsFull[index]), field) ||
      SetSelectControlValue(ASCIIToUTF16(kMonthsNumeric[index]), field);
  return filled;
}

}  // namespace

namespace autofill {

void FillSelectControl(const FormGroup& form_group,
                       AutoFillType type,
                       webkit_glue::FormField* field) {
  DCHECK(field);
  DCHECK_EQ(ASCIIToUTF16("select-one"), field->form_control_type());

  string16 value;
  string16 field_text = form_group.GetFieldText(type);
  for (size_t i = 0; i < field->option_strings().size(); ++i) {
    if (field_text == field->option_strings()[i]) {
      // An exact match, use it.
      value = field_text;
      break;
    }

    if (StringToLowerASCII(field->option_strings()[i]) ==
        StringToLowerASCII(field_text)) {
      // A match, but not in the same case. Save it in case an exact match is
      // not found.
      value = field->option_strings()[i];
    }
  }

  if (!value.empty()) {
    field->set_value(value);
    return;
  }

  if (type.field_type() == ADDRESS_HOME_STATE ||
      type.field_type() == ADDRESS_BILLING_STATE) {
    FillStateSelectControl(field_text, field);
  } else if (type.field_type() == ADDRESS_HOME_COUNTRY ||
             type.field_type() == ADDRESS_BILLING_COUNTRY) {
    FillCountrySelectControl(field_text, field);
  } else if (type.field_type() == CREDIT_CARD_EXP_MONTH) {
    FillExpirationMonthSelectControl(field_text, field);
  }

  return;
}

}  // namespace autofill
