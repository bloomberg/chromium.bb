// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/template_url_prepopulate_data.h"

#if defined(OS_POSIX) && !defined(OS_MACOSX)
#include <locale.h>
#endif

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_vector.h"
#include "base/prefs/pref_service.h"
#include "base/stl_util.h"
#include "base/string16.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/prefs/pref_registry_syncable.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/prepopulated_engines.h"
#include "chrome/browser/search_engines/search_engine_type.h"
#include "chrome/browser/search_engines/search_terms_data.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_WIN)
#undef IN  // On Windows, windef.h defines this, which screws up "India" cases.
#elif defined(OS_MACOSX)
#include "base/mac/scoped_cftyperef.h"
#endif

namespace TemplateURLPrepopulateData {

// NOTE: You should probably not change the data in this file without changing
// |kCurrentDataVersion| in prepopulated_engines.json. See comments in
// GetDataVersion() below!

// Lists of engines per country ////////////////////////////////////////////////

// Put these in order with most interesting/important first.  The default will
// be the first engine.

// Default (for countries with no better engine set)
const PrepopulatedEngine* engines_default[] =
    { &google, &yahoo, &bing, };

// United Arab Emirates
const PrepopulatedEngine* engines_AE[] =
    { &google, &yahoo, &bing_en_XA, &bing_ar_XA, &araby, &maktoob, };

// Albania
const PrepopulatedEngine* engines_AL[] =
    { &google, &yahoo, &bing_en_XA, };

// Argentina
const PrepopulatedEngine* engines_AR[] =
    { &google, &yahoo_ar, &bing_es_AR, &terra_ar, &altavista_ar, };

// Austria
const PrepopulatedEngine* engines_AT[] =
    { &google, &yahoo_at, &bing_de_AT, };

// Australia
const PrepopulatedEngine* engines_AU[] =
    { &google, &bing_en_AU, &yahoo_au, };

// Bosnia and Herzegovina
const PrepopulatedEngine* engines_BA[] =
    { &google, &yahoo, &bing, };

// Belgium
const PrepopulatedEngine* engines_BE[] =
    { &google, &bing_nl_BE, &yahoo, &bing_fr_BE, };

// Bulgaria
const PrepopulatedEngine* engines_BG[] =
    { &google, &bing_bg_BG, &yahoo, &diri, &jabse, };

// Bahrain
const PrepopulatedEngine* engines_BH[] =
    { &google, &yahoo, &bing_en_XA, &bing_ar_XA, };

// Burundi
const PrepopulatedEngine* engines_BI[] =
    { &google, &yahoo, &bing, };

// Brunei
const PrepopulatedEngine* engines_BN[] =
    { &google, &yahoo_malaysia, &bing_en_MY, };

// Bolivia
const PrepopulatedEngine* engines_BO[] =
    { &google, &altavista, &bing_es_XL, &yahoo, };

// Brazil
const PrepopulatedEngine* engines_BR[] =
    { &google, &bing_pt_BR, &yahoo_br, &uol, };

// Belarus
const PrepopulatedEngine* engines_BY[] =
    { &google, &tut, &yandex_ru, &rambler, &yahoo, };

// Belize
const PrepopulatedEngine* engines_BZ[] =
    { &google, &yahoo, &bing, &aol, };

// Canada
const PrepopulatedEngine* engines_CA[] =
    { &google, &yahoo_ca, &yahoo_qc, &bing_en_CA, &bing_fr_CA, };

// Switzerland
const PrepopulatedEngine* engines_CH[] =
    { &google, &yahoo_ch, &bing_de_CH, &bing_fr_CH, &search_de_CH,
      &search_fr_CH, };

// Chile
const PrepopulatedEngine* engines_CL[] =
    { &google, &yahoo_cl, &bing_es_CL, };

// China
const PrepopulatedEngine* engines_CN[] =
    { &google, &baidu, &yahoo_cn, &bing_zh_CN, };

// Colombia
const PrepopulatedEngine* engines_CO[] =
    { &google, &bing_es_XL, &yahoo_co, };

// Costa Rica
const PrepopulatedEngine* engines_CR[] =
    { &google, &bing_es_XL, &yahoo, };

// Czech Republic
const PrepopulatedEngine* engines_CZ[] =
    { &google, &seznam, &bing_cs_CZ, &centrum_cz, &atlas_cz, };

// Germany
const PrepopulatedEngine* engines_DE[] =
    { &google, &ask_de, &bing_de_DE, &yahoo_de };

// Denmark
const PrepopulatedEngine* engines_DK[] =
    { &google, &bing_da_DK, &yahoo_dk, };

// Dominican Republic
const PrepopulatedEngine* engines_DO[] =
    { &google, &bing_es_XL, &yahoo, };

// Algeria
const PrepopulatedEngine* engines_DZ[] =
    { &google, &bing_en_XA, &yahoo, &bing_ar_XA, &maktoob, };

// Ecuador
const PrepopulatedEngine* engines_EC[] =
    { &google, &bing_es_XL, &yahoo, };

// Estonia
const PrepopulatedEngine* engines_EE[] =
    { &google, &bing_et_EE, &neti, &yahoo, };

// Egypt
const PrepopulatedEngine* engines_EG[] =
    { &google, &yahoo, &bing_en_XA, &bing_ar_XA, &masrawy, };

// Spain
const PrepopulatedEngine* engines_ES[] =
    { &google, &ask_es, &bing_es_ES, &yahoo_es, &terra_es, &hispavista, };

// Faroe Islands
const PrepopulatedEngine* engines_FO[] =
    { &google, &bing_da_DK, &yahoo_dk, &jubii };

// Finland
const PrepopulatedEngine* engines_FI[] =
    { &google, &bing_fi_FI, &yahoo_fi, &eniro_fi, &fonecta_02_fi, };

// France
const PrepopulatedEngine* engines_FR[] =
    { &google, &yahoo_fr, &bing_fr_FR, };

// United Kingdom
const PrepopulatedEngine* engines_GB[] =
    { &google, &ask_uk, &yahoo_uk, &bing_en_GB, };

// Greece
const PrepopulatedEngine* engines_GR[] =
    { &google, &yahoo, &in, &bing_el_GR };

// Guatemala
const PrepopulatedEngine* engines_GT[] =
    { &google, &bing_es_XL, &yahoo, &ask_es, };

// Hong Kong
const PrepopulatedEngine* engines_HK[] =
    { &google, &yahoo_hk, &bing_zh_HK, &baidu, };

// Honduras
const PrepopulatedEngine* engines_HN[] =
    { &google, &bing_es_XL, &yahoo, &ask_es, };

// Croatia
const PrepopulatedEngine* engines_HR[] =
    { &google, &yahoo, &bing_hr_HR, };

// Hungary
const PrepopulatedEngine* engines_HU[] =
    { &google, &ok, &bing_hu_HU, };

// Indonesia
const PrepopulatedEngine* engines_ID[] =
    { &google, &yahoo_id, &bing_en_ID, };

// Ireland
const PrepopulatedEngine* engines_IE[] =
    { &google, &yahoo_uk, &bing_en_IE, };

// Israel
const PrepopulatedEngine* engines_IL[] =
    { &google, &walla, &bing_he_IL, };

// India
const PrepopulatedEngine* engines_IN[] =
    { &google, &yahoo_in, &bing_en_IN, &rediff, &guruji, };

// Iraq
const PrepopulatedEngine* engines_IQ[] =
    { &google, &yahoo, &bing_en_XA, &bing_ar_XA, &maktoob, &ask, };

// Iran
const PrepopulatedEngine* engines_IR[] =
    { &google, &yahoo, };

// Iceland
const PrepopulatedEngine* engines_IS[] =
    { &google, &yahoo, &bing, &leit, };

// Italy
const PrepopulatedEngine* engines_IT[] =
    { &google, &ask_it, &virgilio, &bing_it_IT, &yahoo_it, &libero, };

// Jamaica
const PrepopulatedEngine* engines_JM[] =
    { &google, &yahoo, &bing, };

// Jordan
const PrepopulatedEngine* engines_JO[] =
    { &google, &yahoo, &bing_en_XA, &bing_ar_XA, &maktoob, &araby, };

// Japan
const PrepopulatedEngine* engines_JP[] =
    { &google, &yahoo_jp, &bing_ja_JP, &goo, };

// Kenya
const PrepopulatedEngine* engines_KE[] =
    { &google, &yahoo, &bing, };

// Kuwait
const PrepopulatedEngine* engines_KW[] =
    { &google, &bing_en_XA, &yahoo, &bing_ar_XA, &maktoob, &araby, };

// South Korea
const PrepopulatedEngine* engines_KR[] =
    { &google, &naver, &daum, &yahoo_kr, &nate, };

// Kazakhstan
const PrepopulatedEngine* engines_KZ[] =
    { &google, &rambler, &yandex_ru, &nur_kz, };

// Lebanon
const PrepopulatedEngine* engines_LB[] =
    { &google, &yahoo, &bing_en_XA, &bing_ar_XA, &maktoob, &araby, };

// Liechtenstein
const PrepopulatedEngine* engines_LI[] =
    { &google, &bing_de_DE, &yahoo_de, };

// Lithuania
const PrepopulatedEngine* engines_LT[] =
    { &google, &delfi_lt, &yahoo, &bing_lt_LT, };

// Luxembourg
const PrepopulatedEngine* engines_LU[] =
    { &google, &bing_fr_FR, &yahoo_fr, };

// Latvia
const PrepopulatedEngine* engines_LV[] =
    { &google, &bing, &yandex_ru, &yahoo, &latne, };

// Libya
const PrepopulatedEngine* engines_LY[] =
    { &google, &yahoo, &bing_en_XA, &bing_ar_XA, &maktoob, &ask, };

// Morocco
const PrepopulatedEngine* engines_MA[] =
    { &google, &bing_en_XA, &yahoo, &bing_ar_XA, };

// Monaco
const PrepopulatedEngine* engines_MC[] =
    { &google, &bing_fr_FR, &yahoo_fr, };

// Moldova
const PrepopulatedEngine* engines_MD[] =
    { &google, &yandex_ru, &yahoo, &bing, };

// Montenegro
const PrepopulatedEngine* engines_ME[] =
    { &google, &yahoo, &bing };

// Macedonia
const PrepopulatedEngine* engines_MK[] =
    { &google, &yahoo, &bing, };

// Mexico
const PrepopulatedEngine* engines_MX[] =
    { &google, &bing_es_MX, &yahoo_mx, };

// Malaysia
const PrepopulatedEngine* engines_MY[] =
    { &google, &yahoo_malaysia, &bing_en_MY, };

// Nicaragua
const PrepopulatedEngine* engines_NI[] =
    { &google, &bing_es_XL, &yahoo, &ask_es, };

// Netherlands
const PrepopulatedEngine* engines_NL[] =
    { &google, &bing_nl_NL, &yahoo_nl, &ask_nl, };

// Norway
const PrepopulatedEngine* engines_NO[] =
    { &google, &bing_nb_NO, &abcsok, &yahoo_no, &kvasir, };

// New Zealand
const PrepopulatedEngine* engines_NZ[] =
    { &google, &yahoo_nz, &bing_en_NZ, };

// Oman
const PrepopulatedEngine* engines_OM[] =
    { &google, &yahoo, &bing_en_XA, &bing_ar_XA, };

// Panama
const PrepopulatedEngine* engines_PA[] =
    { &google, &bing_es_XL, &yahoo, &ask_es, };

// Peru
const PrepopulatedEngine* engines_PE[] =
    { &google, &bing_es_XL, &yahoo_pe, };

// Philippines
const PrepopulatedEngine* engines_PH[] =
    { &google, &yahoo_ph, &bing_en_PH, };

// Pakistan
const PrepopulatedEngine* engines_PK[] =
    { &google, &yahoo, &bing, };

// Puerto Rico
const PrepopulatedEngine* engines_PR[] =
    { &google, &bing_es_XL, &yahoo, &ask_es, };

// Poland
const PrepopulatedEngine* engines_PL[] =
    { &google, &bing_pl_PL, &netsprint, &yahoo_uk, &onet, &wp,  };

// Portugal
const PrepopulatedEngine* engines_PT[] =
    { &google, &sapo, &bing_pt_PT, &yahoo, };

// Paraguay
const PrepopulatedEngine* engines_PY[] =
    { &google, &bing_es_XL, &yahoo, };

// Qatar
const PrepopulatedEngine* engines_QA[] =
    { &google, &yahoo, &bing_en_XA, &bing_ar_XA, &maktoob, &araby };

// Romania
const PrepopulatedEngine* engines_RO[] =
    { &google, &yahoo_uk, &bing_ro_RO, };

// Serbia
const PrepopulatedEngine* engines_RS[] =
    { &google, &pogodak_rs, &bing, };

// Russia
const PrepopulatedEngine* engines_RU[] =
    { &google, &yandex_ru, &mail_ru, &tut, &rambler, &bing_ru_RU, };

// Rwanda
const PrepopulatedEngine* engines_RW[] =
    { &google, &yahoo, &bing, };

// Saudi Arabia
const PrepopulatedEngine* engines_SA[] =
    { &google, &yahoo, &bing_en_XA, &bing_ar_XA, };

// Sweden
const PrepopulatedEngine* engines_SE[] =
    { &google, &bing_sv_SE, &yahoo_se, &altavista_se, &eniro_se };

// Singapore
const PrepopulatedEngine* engines_SG[] =
    { &google, &yahoo_sg, &bing_en_SG, &rednano, };

// Slovenia
const PrepopulatedEngine* engines_SI[] =
    { &google, &najdi, &yahoo, &bing_sl_SI, };

// Slovakia
const PrepopulatedEngine* engines_SK[] =
    { &google, &zoznam, &bing_sk_SK, &atlas_sk, &centrum_sk };

// El Salvador
const PrepopulatedEngine* engines_SV[] =
    { &google, &bing_es_XL, &yahoo, };

// Syria
const PrepopulatedEngine* engines_SY[] =
    { &google, &bing_en_XA, &yahoo, &bing_ar_XA, &maktoob, &yamli, };

// Thailand
const PrepopulatedEngine* engines_TH[] =
    { &google, &sanook, &yahoo_th, &bing_th_TH, };

// Tunisia
const PrepopulatedEngine* engines_TN[] =
    { &google, &bing_en_XA, &yahoo, &bing_ar_XA, &maktoob, &yamli };

// Turkey
const PrepopulatedEngine* engines_TR[] =
    { &google, &bing_tr_TR, &yahoo, &mynet, };

// Trinidad and Tobago
const PrepopulatedEngine* engines_TT[] =
    { &google, &bing, &yahoo, &aol, };

// Taiwan
const PrepopulatedEngine* engines_TW[] =
    { &google, &yahoo_tw, &bing_zh_TW, };

// Tanzania
const PrepopulatedEngine* engines_TZ[] =
    { &google, &yahoo, &bing, };

// Ukraine
const PrepopulatedEngine* engines_UA[] =
    { &google, &yandex_ua, &mail_ru, &rambler, };

// United States
const PrepopulatedEngine* engines_US[] =
    { &google, &yahoo, &bing_en_US, };

// Uruguay
const PrepopulatedEngine* engines_UY[] =
    { &google, &bing_es_XL, &yahoo, };

// Venezuela
const PrepopulatedEngine* engines_VE[] =
    { &google, &bing_es_XL, &yahoo_ve, };

// Vietnam
const PrepopulatedEngine* engines_VN[] =
    { &google, &yahoo_vn, };

// Yemen
const PrepopulatedEngine* engines_YE[] =
    { &google, &yahoo, &bing_en_XA, &bing_ar_XA, &maktoob, &araby, };

// South Africa
const PrepopulatedEngine* engines_ZA[] =
    { &google, &yahoo, &bing_en_ZA, };

// Zimbabwe
const PrepopulatedEngine* engines_ZW[] =
    { &google, &yahoo, &bing, };


// A list of all the engines that we know about.
const PrepopulatedEngine* kAllEngines[] =
    { // Prepopulated engines:
      &abcsok, &altavista, &altavista_ar, &altavista_se, &aol, &araby, &ask,
      &ask_de, &ask_es, &ask_it, &ask_nl, &ask_uk, &atlas_cz, &atlas_sk, &baidu,
      &bing, &bing_ar_XA, &bing_bg_BG, &bing_cs_CZ, &bing_da_DK, &bing_de_AT,
      &bing_de_CH, &bing_de_DE, &bing_el_GR, &bing_en_AU, &bing_en_CA,
      &bing_en_GB, &bing_en_ID, &bing_en_IE, &bing_en_IN, &bing_en_MY,
      &bing_en_NZ, &bing_en_PH, &bing_en_SG, &bing_en_US, &bing_en_XA,
      &bing_en_ZA, &bing_es_AR, &bing_es_CL, &bing_es_ES, &bing_es_MX,
      &bing_es_XL, &bing_et_EE, &bing_fi_FI, &bing_fr_BE, &bing_fr_CA,
      &bing_fr_CH, &bing_fr_FR, &bing_he_IL, &bing_hr_HR, &bing_hu_HU,
      &bing_it_IT, &bing_ja_JP, &bing_ko_KR, &bing_lt_LT, &bing_lv_LV,
      &bing_nb_NO, &bing_nl_BE, &bing_nl_NL, &bing_pl_PL, &bing_pt_BR,
      &bing_pt_PT, &bing_ro_RO, &bing_ru_RU, &bing_sl_SI, &bing_sk_SK,
      &bing_sv_SE, &bing_th_TH, &bing_tr_TR, &bing_uk_UA, &bing_zh_CN,
      &bing_zh_HK, &bing_zh_TW, &centrum_cz, &centrum_sk, &daum, &delfi_lt,
      &delfi_lv, &diri, &eniro_fi, &eniro_se, &fonecta_02_fi, &goo, &google,
      &guruji, &hispavista, &in, &jabse, &jubii, &kvasir, &latne, &leit,
      &libero, &mail_ru, &maktoob, &masrawy, &mynet, &najdi, &nate, &naver,
      &neti, &netsprint, &nur_kz, &ok, &onet, &pogodak_rs, &rambler, &rediff,
      &rednano, &sanook, &sapo, &search_de_CH, &search_fr_CH, &seznam,
      &terra_ar, &terra_es, &tut, &uol, &virgilio, &walla, &wp, &yahoo,
      &yahoo_ar, &yahoo_at, &yahoo_au, &yahoo_br, &yahoo_ca, &yahoo_ch,
      &yahoo_cl, &yahoo_cn, &yahoo_co, &yahoo_de, &yahoo_dk, &yahoo_es,
      &yahoo_fi, &yahoo_fr, &yahoo_hk, &yahoo_id, &yahoo_in, &yahoo_it,
      &yahoo_jp, &yahoo_kr, &yahoo_malaysia, &yahoo_mx, &yahoo_nl, &yahoo_no,
      &yahoo_nz, &yahoo_pe, &yahoo_ph, &yahoo_qc, &yahoo_ru, &yahoo_se,
      &yahoo_sg, &yahoo_th, &yahoo_tw, &yahoo_uk, &yahoo_ve, &yahoo_vn, &yamli,
      &yandex_ru, &yandex_ua, &zoznam,
      // UMA-only engines:
      &all_by, &aport, &avg, &avg_i, &conduit, &icq, &meta_ua, &metabot_ru,
      &nigma, &qip, &ukr_net, &webalta, &yandex_tr };


// Geographic mappings /////////////////////////////////////////////////////////

// Please refer to ISO 3166-1 for information about the two-character country
// codes; http://en.wikipedia.org/wiki/ISO_3166-1_alpha-2 is useful. In the
// following (C++) code, we pack the two letters of the country code into an int
// value we call the CountryID.

const int kCountryIDUnknown = -1;

inline int CountryCharsToCountryID(char c1, char c2) {
  return c1 << 8 | c2;
}

int CountryCharsToCountryIDWithUpdate(char c1, char c2) {
  // SPECIAL CASE: In 2003, Yugoslavia renamed itself to Serbia and Montenegro.
  // Serbia and Montenegro dissolved their union in June 2006. Yugoslavia was
  // ISO 'YU' and Serbia and Montenegro were ISO 'CS'. Serbia was subsequently
  // issued 'RS' and Montenegro 'ME'. Windows XP and Mac OS X Leopard still use
  // the value 'YU'. If we get a value of 'YU' or 'CS' we will map it to 'RS'.
  if ((c1 == 'Y' && c2 == 'U') ||
      (c1 == 'C' && c2 == 'S')) {
    c1 = 'R';
    c2 = 'S';
  }

  // SPECIAL CASE: Timor-Leste changed from 'TP' to 'TL' in 2002. Windows XP
  // predates this; we therefore map this value.
  if (c1 == 'T' && c2 == 'P')
    c2 = 'L';

  return CountryCharsToCountryID(c1, c2);
}

#if defined(OS_WIN)

// For reference, a list of GeoIDs can be found at
// http://msdn.microsoft.com/en-us/library/dd374073.aspx .
int GeoIDToCountryID(GEOID geo_id) {
  const int kISOBufferSize = 3;  // Two plus one for the terminator.
  wchar_t isobuf[kISOBufferSize] = { 0 };
  int retval = GetGeoInfo(geo_id, GEO_ISO2, isobuf, kISOBufferSize, 0);

  if (retval == kISOBufferSize &&
      !(isobuf[0] == L'X' && isobuf[1] == L'X'))
    return CountryCharsToCountryIDWithUpdate(static_cast<char>(isobuf[0]),
                                             static_cast<char>(isobuf[1]));

  // Various locations have ISO codes that Windows does not return.
  switch (geo_id) {
    case 0x144:   // Guernsey
      return CountryCharsToCountryID('G', 'G');
    case 0x148:   // Jersey
      return CountryCharsToCountryID('J', 'E');
    case 0x3B16:  // Isle of Man
      return CountryCharsToCountryID('I', 'M');

    // 'UM' (U.S. Minor Outlying Islands)
    case 0x7F:    // Johnston Atoll
    case 0x102:   // Wake Island
    case 0x131:   // Baker Island
    case 0x146:   // Howland Island
    case 0x147:   // Jarvis Island
    case 0x149:   // Kingman Reef
    case 0x152:   // Palmyra Atoll
    case 0x52FA:  // Midway Islands
      return CountryCharsToCountryID('U', 'M');

    // 'SH' (Saint Helena)
    case 0x12F:  // Ascension Island
    case 0x15C:  // Tristan da Cunha
      return CountryCharsToCountryID('S', 'H');

    // 'IO' (British Indian Ocean Territory)
    case 0x13A:  // Diego Garcia
      return CountryCharsToCountryID('I', 'O');

    // Other cases where there is no ISO country code; we assign countries that
    // can serve as reasonable defaults.
    case 0x154:  // Rota Island
    case 0x155:  // Saipan
    case 0x15A:  // Tinian Island
      return CountryCharsToCountryID('U', 'S');
    case 0x134:  // Channel Islands
      return CountryCharsToCountryID('G', 'B');
    case 0x143:  // Guantanamo Bay
    default:
      return kCountryIDUnknown;
  }
}

int GetCurrentCountryID() {
  GEOID geo_id = GetUserGeoID(GEOCLASS_NATION);

  return GeoIDToCountryID(geo_id);
}

#elif defined(OS_MACOSX)

int GetCurrentCountryID() {
  base::mac::ScopedCFTypeRef<CFLocaleRef> locale(CFLocaleCopyCurrent());
  CFStringRef country = (CFStringRef)CFLocaleGetValue(locale.get(),
                                                      kCFLocaleCountryCode);
  if (!country)
    return kCountryIDUnknown;

  UniChar isobuf[2];
  CFRange char_range = CFRangeMake(0, 2);
  CFStringGetCharacters(country, char_range, isobuf);

  return CountryCharsToCountryIDWithUpdate(static_cast<char>(isobuf[0]),
                                           static_cast<char>(isobuf[1]));
}

#elif defined(OS_ANDROID)

// Initialized by InitCountryCode().
int g_country_code_at_install = kCountryIDUnknown;

int GetCurrentCountryID() {
  return g_country_code_at_install;
}

void InitCountryCode(const std::string& country_code) {
  if (country_code.size() != 2) {
    DLOG(ERROR) << "Invalid country code: " << country_code;
    g_country_code_at_install = kCountryIDUnknown;
  } else {
    g_country_code_at_install =
        CountryCharsToCountryIDWithUpdate(country_code[0], country_code[1]);
  }
}

#elif defined(OS_POSIX)

int GetCurrentCountryID() {
  const char* locale = setlocale(LC_MESSAGES, NULL);

  if (!locale)
    return kCountryIDUnknown;

  // The format of a locale name is:
  // language[_territory][.codeset][@modifier], where territory is an ISO 3166
  // country code, which is what we want.
  std::string locale_str(locale);
  size_t begin = locale_str.find('_');
  if (begin == std::string::npos || locale_str.size() - begin < 3)
    return kCountryIDUnknown;

  ++begin;
  size_t end = locale_str.find_first_of(".@", begin);
  if (end == std::string::npos)
    end = locale_str.size();

  // The territory part must contain exactly two characters.
  if (end - begin == 2) {
    return CountryCharsToCountryIDWithUpdate(
        base::ToUpperASCII(locale_str[begin]),
        base::ToUpperASCII(locale_str[begin + 1]));
  }

  return kCountryIDUnknown;
}

#endif  // OS_*

int GetCountryIDFromPrefs(PrefService* prefs) {
  // See if the user overrode the country on the command line.
  const std::string country(
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kCountry));
  if (country.length() == 2)
    return CountryCharsToCountryIDWithUpdate(country[0], country[1]);

  // Cache first run Country ID value in prefs, and use it afterwards.  This
  // ensures that just because the user moves around, we won't automatically
  // make major changes to their available search providers, which would feel
  // surprising.
  if (!prefs)
    return GetCurrentCountryID();

  int new_country_id = GetCurrentCountryID();
#if defined(OS_WIN)
  // Migrate the old platform-specific value if it's present.
  if (prefs->HasPrefPath(prefs::kGeoIDAtInstall)) {
    int geo_id = prefs->GetInteger(prefs::kGeoIDAtInstall);
    prefs->ClearPref(prefs::kGeoIDAtInstall);
    new_country_id = GeoIDToCountryID(geo_id);
  }
#endif

  if (!prefs->HasPrefPath(prefs::kCountryIDAtInstall))
    prefs->SetInteger(prefs::kCountryIDAtInstall, new_country_id);

  return prefs->GetInteger(prefs::kCountryIDAtInstall);
}

void GetPrepopulationSetFromCountryID(PrefService* prefs,
                                      const PrepopulatedEngine*** engines,
                                      size_t* num_engines) {
  // NOTE: This function should ALWAYS set its outparams.

  // If you add a new country make sure to update the unit test for coverage.
  switch (GetCountryIDFromPrefs(prefs)) {

#define CHAR_A 'A'
#define CHAR_B 'B'
#define CHAR_C 'C'
#define CHAR_D 'D'
#define CHAR_E 'E'
#define CHAR_F 'F'
#define CHAR_G 'G'
#define CHAR_H 'H'
#define CHAR_I 'I'
#define CHAR_J 'J'
#define CHAR_K 'K'
#define CHAR_L 'L'
#define CHAR_M 'M'
#define CHAR_N 'N'
#define CHAR_O 'O'
#define CHAR_P 'P'
#define CHAR_Q 'Q'
#define CHAR_R 'R'
#define CHAR_S 'S'
#define CHAR_T 'T'
#define CHAR_U 'U'
#define CHAR_V 'V'
#define CHAR_W 'W'
#define CHAR_X 'X'
#define CHAR_Y 'Y'
#define CHAR_Z 'Z'
#define CHAR(ch) CHAR_##ch
#define CODE_TO_ID(code1, code2)\
    (CHAR(code1) << 8 | CHAR(code2))

#define UNHANDLED_COUNTRY(code1, code2)\
    case CODE_TO_ID(code1, code2):
#define END_UNHANDLED_COUNTRIES(code1, code2)\
      *engines = engines_##code1##code2;\
      *num_engines = arraysize(engines_##code1##code2);\
      return;
#define DECLARE_COUNTRY(code1, code2)\
    UNHANDLED_COUNTRY(code1, code2)\
    END_UNHANDLED_COUNTRIES(code1, code2)

    // Countries with their own, dedicated engine set.
    DECLARE_COUNTRY(A, E)  // United Arab Emirates
    DECLARE_COUNTRY(A, L)  // Albania
    DECLARE_COUNTRY(A, R)  // Argentina
    DECLARE_COUNTRY(A, T)  // Austria
    DECLARE_COUNTRY(A, U)  // Australia
    DECLARE_COUNTRY(B, A)  // Bosnia and Herzegovina
    DECLARE_COUNTRY(B, E)  // Belgium
    DECLARE_COUNTRY(B, G)  // Bulgaria
    DECLARE_COUNTRY(B, H)  // Bahrain
    DECLARE_COUNTRY(B, I)  // Burundi
    DECLARE_COUNTRY(B, N)  // Brunei
    DECLARE_COUNTRY(B, O)  // Bolivia
    DECLARE_COUNTRY(B, R)  // Brazil
    DECLARE_COUNTRY(B, Y)  // Belarus
    DECLARE_COUNTRY(B, Z)  // Belize
    DECLARE_COUNTRY(C, A)  // Canada
    DECLARE_COUNTRY(C, H)  // Switzerland
    DECLARE_COUNTRY(C, L)  // Chile
    DECLARE_COUNTRY(C, N)  // China
    DECLARE_COUNTRY(C, O)  // Colombia
    DECLARE_COUNTRY(C, R)  // Costa Rica
    DECLARE_COUNTRY(C, Z)  // Czech Republic
    DECLARE_COUNTRY(D, E)  // Germany
    DECLARE_COUNTRY(D, K)  // Denmark
    DECLARE_COUNTRY(D, O)  // Dominican Republic
    DECLARE_COUNTRY(D, Z)  // Algeria
    DECLARE_COUNTRY(E, C)  // Ecuador
    DECLARE_COUNTRY(E, E)  // Estonia
    DECLARE_COUNTRY(E, G)  // Egypt
    DECLARE_COUNTRY(E, S)  // Spain
    DECLARE_COUNTRY(F, I)  // Finland
    DECLARE_COUNTRY(F, O)  // Faroe Islands
    DECLARE_COUNTRY(F, R)  // France
    DECLARE_COUNTRY(G, B)  // United Kingdom
    DECLARE_COUNTRY(G, R)  // Greece
    DECLARE_COUNTRY(G, T)  // Guatemala
    DECLARE_COUNTRY(H, K)  // Hong Kong
    DECLARE_COUNTRY(H, N)  // Honduras
    DECLARE_COUNTRY(H, R)  // Croatia
    DECLARE_COUNTRY(H, U)  // Hungary
    DECLARE_COUNTRY(I, D)  // Indonesia
    DECLARE_COUNTRY(I, E)  // Ireland
    DECLARE_COUNTRY(I, L)  // Israel
    DECLARE_COUNTRY(I, N)  // India
    DECLARE_COUNTRY(I, Q)  // Iraq
    DECLARE_COUNTRY(I, R)  // Iran
    DECLARE_COUNTRY(I, S)  // Iceland
    DECLARE_COUNTRY(I, T)  // Italy
    DECLARE_COUNTRY(J, M)  // Jamaica
    DECLARE_COUNTRY(J, O)  // Jordan
    DECLARE_COUNTRY(J, P)  // Japan
    DECLARE_COUNTRY(K, E)  // Kenya
    DECLARE_COUNTRY(K, R)  // South Korea
    DECLARE_COUNTRY(K, W)  // Kuwait
    DECLARE_COUNTRY(K, Z)  // Kazakhstan
    DECLARE_COUNTRY(L, B)  // Lebanon
    DECLARE_COUNTRY(L, I)  // Liechtenstein
    DECLARE_COUNTRY(L, T)  // Lithuania
    DECLARE_COUNTRY(L, U)  // Luxembourg
    DECLARE_COUNTRY(L, V)  // Latvia
    DECLARE_COUNTRY(L, Y)  // Libya
    DECLARE_COUNTRY(M, A)  // Morocco
    DECLARE_COUNTRY(M, C)  // Monaco
    DECLARE_COUNTRY(M, D)  // Moldova
    DECLARE_COUNTRY(M, E)  // Montenegro
    DECLARE_COUNTRY(M, K)  // Macedonia
    DECLARE_COUNTRY(M, X)  // Mexico
    DECLARE_COUNTRY(M, Y)  // Malaysia
    DECLARE_COUNTRY(N, I)  // Nicaragua
    DECLARE_COUNTRY(N, L)  // Netherlands
    DECLARE_COUNTRY(N, O)  // Norway
    DECLARE_COUNTRY(N, Z)  // New Zealand
    DECLARE_COUNTRY(O, M)  // Oman
    DECLARE_COUNTRY(P, A)  // Panama
    DECLARE_COUNTRY(P, E)  // Peru
    DECLARE_COUNTRY(P, H)  // Philippines
    DECLARE_COUNTRY(P, K)  // Pakistan
    DECLARE_COUNTRY(P, L)  // Poland
    DECLARE_COUNTRY(P, R)  // Puerto Rico
    DECLARE_COUNTRY(P, T)  // Portugal
    DECLARE_COUNTRY(P, Y)  // Paraguay
    DECLARE_COUNTRY(Q, A)  // Qatar
    DECLARE_COUNTRY(R, O)  // Romania
    DECLARE_COUNTRY(R, S)  // Serbia
    DECLARE_COUNTRY(R, U)  // Russia
    DECLARE_COUNTRY(R, W)  // Rwanda
    DECLARE_COUNTRY(S, A)  // Saudi Arabia
    DECLARE_COUNTRY(S, E)  // Sweden
    DECLARE_COUNTRY(S, G)  // Singapore
    DECLARE_COUNTRY(S, I)  // Slovenia
    DECLARE_COUNTRY(S, K)  // Slovakia
    DECLARE_COUNTRY(S, V)  // El Salvador
    DECLARE_COUNTRY(S, Y)  // Syria
    DECLARE_COUNTRY(T, H)  // Thailand
    DECLARE_COUNTRY(T, N)  // Tunisia
    DECLARE_COUNTRY(T, R)  // Turkey
    DECLARE_COUNTRY(T, T)  // Trinidad and Tobago
    DECLARE_COUNTRY(T, W)  // Taiwan
    DECLARE_COUNTRY(T, Z)  // Tanzania
    DECLARE_COUNTRY(U, A)  // Ukraine
    DECLARE_COUNTRY(U, S)  // United States
    DECLARE_COUNTRY(U, Y)  // Uruguay
    DECLARE_COUNTRY(V, E)  // Venezuela
    DECLARE_COUNTRY(V, N)  // Vietnam
    DECLARE_COUNTRY(Y, E)  // Yemen
    DECLARE_COUNTRY(Z, A)  // South Africa
    DECLARE_COUNTRY(Z, W)  // Zimbabwe

    // Countries using the "Australia" engine set.
    UNHANDLED_COUNTRY(C, C)  // Cocos Islands
    UNHANDLED_COUNTRY(C, X)  // Christmas Island
    UNHANDLED_COUNTRY(H, M)  // Heard Island and McDonald Islands
    UNHANDLED_COUNTRY(N, F)  // Norfolk Island
    END_UNHANDLED_COUNTRIES(A, U)

    // Countries using the "China" engine set.
    UNHANDLED_COUNTRY(M, O)  // Macao
    END_UNHANDLED_COUNTRIES(C, N)

    // Countries using the "Denmark" engine set.
    UNHANDLED_COUNTRY(G, L)  // Greenland
    END_UNHANDLED_COUNTRIES(D, K)

    // Countries using the "Spain" engine set.
    UNHANDLED_COUNTRY(A, D)  // Andorra
    END_UNHANDLED_COUNTRIES(E, S)

    // Countries using the "Finland" engine set.
    UNHANDLED_COUNTRY(A, X)  // Aland Islands
    END_UNHANDLED_COUNTRIES(F, I)

    // Countries using the "France" engine set.
    UNHANDLED_COUNTRY(B, F)  // Burkina Faso
    UNHANDLED_COUNTRY(B, J)  // Benin
    UNHANDLED_COUNTRY(C, D)  // Congo - Kinshasa
    UNHANDLED_COUNTRY(C, F)  // Central African Republic
    UNHANDLED_COUNTRY(C, G)  // Congo - Brazzaville
    UNHANDLED_COUNTRY(C, I)  // Ivory Coast
    UNHANDLED_COUNTRY(C, M)  // Cameroon
    UNHANDLED_COUNTRY(D, J)  // Djibouti
    UNHANDLED_COUNTRY(G, A)  // Gabon
    UNHANDLED_COUNTRY(G, F)  // French Guiana
    UNHANDLED_COUNTRY(G, N)  // Guinea
    UNHANDLED_COUNTRY(G, P)  // Guadeloupe
    UNHANDLED_COUNTRY(H, T)  // Haiti
#if defined(OS_WIN)
    UNHANDLED_COUNTRY(I, P)  // Clipperton Island ('IP' is an WinXP-ism; ISO
                             //                    includes it with France)
#endif
    UNHANDLED_COUNTRY(M, L)  // Mali
    UNHANDLED_COUNTRY(M, Q)  // Martinique
    UNHANDLED_COUNTRY(N, C)  // New Caledonia
    UNHANDLED_COUNTRY(N, E)  // Niger
    UNHANDLED_COUNTRY(P, F)  // French Polynesia
    UNHANDLED_COUNTRY(P, M)  // Saint Pierre and Miquelon
    UNHANDLED_COUNTRY(R, E)  // Reunion
    UNHANDLED_COUNTRY(S, N)  // Senegal
    UNHANDLED_COUNTRY(T, D)  // Chad
    UNHANDLED_COUNTRY(T, F)  // French Southern Territories
    UNHANDLED_COUNTRY(T, G)  // Togo
    UNHANDLED_COUNTRY(W, F)  // Wallis and Futuna
    UNHANDLED_COUNTRY(Y, T)  // Mayotte
    END_UNHANDLED_COUNTRIES(F, R)

    // Countries using the "Greece" engine set.
    UNHANDLED_COUNTRY(C, Y)  // Cyprus
    END_UNHANDLED_COUNTRIES(G, R)

    // Countries using the "Italy" engine set.
    UNHANDLED_COUNTRY(S, M)  // San Marino
    UNHANDLED_COUNTRY(V, A)  // Vatican
    END_UNHANDLED_COUNTRIES(I, T)

    // Countries using the "Morocco" engine set.
    UNHANDLED_COUNTRY(E, H)  // Western Sahara
    END_UNHANDLED_COUNTRIES(M, A)

    // Countries using the "Netherlands" engine set.
    UNHANDLED_COUNTRY(A, N)  // Netherlands Antilles
    UNHANDLED_COUNTRY(A, W)  // Aruba
    END_UNHANDLED_COUNTRIES(N, L)

    // Countries using the "Norway" engine set.
    UNHANDLED_COUNTRY(B, V)  // Bouvet Island
    UNHANDLED_COUNTRY(S, J)  // Svalbard and Jan Mayen
    END_UNHANDLED_COUNTRIES(N, O)

    // Countries using the "New Zealand" engine set.
    UNHANDLED_COUNTRY(C, K)  // Cook Islands
    UNHANDLED_COUNTRY(N, U)  // Niue
    UNHANDLED_COUNTRY(T, K)  // Tokelau
    END_UNHANDLED_COUNTRIES(N, Z)

    // Countries using the "Portugal" engine set.
    UNHANDLED_COUNTRY(C, V)  // Cape Verde
    UNHANDLED_COUNTRY(G, W)  // Guinea-Bissau
    UNHANDLED_COUNTRY(M, Z)  // Mozambique
    UNHANDLED_COUNTRY(S, T)  // Sao Tome and Principe
    UNHANDLED_COUNTRY(T, L)  // Timor-Leste
    END_UNHANDLED_COUNTRIES(P, T)

    // Countries using the "Russia" engine set.
    UNHANDLED_COUNTRY(A, M)  // Armenia
    UNHANDLED_COUNTRY(A, Z)  // Azerbaijan
    UNHANDLED_COUNTRY(K, G)  // Kyrgyzstan
    UNHANDLED_COUNTRY(T, J)  // Tajikistan
    UNHANDLED_COUNTRY(T, M)  // Turkmenistan
    UNHANDLED_COUNTRY(U, Z)  // Uzbekistan
    END_UNHANDLED_COUNTRIES(R, U)

    // Countries using the "Saudi Arabia" engine set.
    UNHANDLED_COUNTRY(M, R)  // Mauritania
    UNHANDLED_COUNTRY(P, S)  // Palestinian Territory
    UNHANDLED_COUNTRY(S, D)  // Sudan
    END_UNHANDLED_COUNTRIES(S, A)

    // Countries using the "United Kingdom" engine set.
    UNHANDLED_COUNTRY(B, M)  // Bermuda
    UNHANDLED_COUNTRY(F, K)  // Falkland Islands
    UNHANDLED_COUNTRY(G, G)  // Guernsey
    UNHANDLED_COUNTRY(G, I)  // Gibraltar
    UNHANDLED_COUNTRY(G, S)  // South Georgia and the South Sandwich
                             //   Islands
    UNHANDLED_COUNTRY(I, M)  // Isle of Man
    UNHANDLED_COUNTRY(I, O)  // British Indian Ocean Territory
    UNHANDLED_COUNTRY(J, E)  // Jersey
    UNHANDLED_COUNTRY(K, Y)  // Cayman Islands
    UNHANDLED_COUNTRY(M, S)  // Montserrat
    UNHANDLED_COUNTRY(M, T)  // Malta
    UNHANDLED_COUNTRY(P, N)  // Pitcairn Islands
    UNHANDLED_COUNTRY(S, H)  // Saint Helena, Ascension Island, and Tristan da
                             //   Cunha
    UNHANDLED_COUNTRY(T, C)  // Turks and Caicos Islands
    UNHANDLED_COUNTRY(V, G)  // British Virgin Islands
    END_UNHANDLED_COUNTRIES(G, B)

    // Countries using the "United States" engine set.
    UNHANDLED_COUNTRY(A, S)  // American Samoa
    UNHANDLED_COUNTRY(G, U)  // Guam
    UNHANDLED_COUNTRY(M, P)  // Northern Mariana Islands
    UNHANDLED_COUNTRY(U, M)  // U.S. Minor Outlying Islands
    UNHANDLED_COUNTRY(V, I)  // U.S. Virgin Islands
    END_UNHANDLED_COUNTRIES(U, S)

    // Countries using the "default" engine set.
    UNHANDLED_COUNTRY(A, F)  // Afghanistan
    UNHANDLED_COUNTRY(A, G)  // Antigua and Barbuda
    UNHANDLED_COUNTRY(A, I)  // Anguilla
    UNHANDLED_COUNTRY(A, O)  // Angola
    UNHANDLED_COUNTRY(A, Q)  // Antarctica
    UNHANDLED_COUNTRY(B, B)  // Barbados
    UNHANDLED_COUNTRY(B, D)  // Bangladesh
    UNHANDLED_COUNTRY(B, S)  // Bahamas
    UNHANDLED_COUNTRY(B, T)  // Bhutan
    UNHANDLED_COUNTRY(B, W)  // Botswana
    UNHANDLED_COUNTRY(C, U)  // Cuba
    UNHANDLED_COUNTRY(D, M)  // Dominica
    UNHANDLED_COUNTRY(E, R)  // Eritrea
    UNHANDLED_COUNTRY(E, T)  // Ethiopia
    UNHANDLED_COUNTRY(F, J)  // Fiji
    UNHANDLED_COUNTRY(F, M)  // Micronesia
    UNHANDLED_COUNTRY(G, D)  // Grenada
    UNHANDLED_COUNTRY(G, E)  // Georgia
    UNHANDLED_COUNTRY(G, H)  // Ghana
    UNHANDLED_COUNTRY(G, M)  // Gambia
    UNHANDLED_COUNTRY(G, Q)  // Equatorial Guinea
    UNHANDLED_COUNTRY(G, Y)  // Guyana
    UNHANDLED_COUNTRY(K, H)  // Cambodia
    UNHANDLED_COUNTRY(K, I)  // Kiribati
    UNHANDLED_COUNTRY(K, M)  // Comoros
    UNHANDLED_COUNTRY(K, N)  // Saint Kitts and Nevis
    UNHANDLED_COUNTRY(K, P)  // North Korea
    UNHANDLED_COUNTRY(L, A)  // Laos
    UNHANDLED_COUNTRY(L, C)  // Saint Lucia
    UNHANDLED_COUNTRY(L, K)  // Sri Lanka
    UNHANDLED_COUNTRY(L, R)  // Liberia
    UNHANDLED_COUNTRY(L, S)  // Lesotho
    UNHANDLED_COUNTRY(M, G)  // Madagascar
    UNHANDLED_COUNTRY(M, H)  // Marshall Islands
    UNHANDLED_COUNTRY(M, M)  // Myanmar
    UNHANDLED_COUNTRY(M, N)  // Mongolia
    UNHANDLED_COUNTRY(M, U)  // Mauritius
    UNHANDLED_COUNTRY(M, V)  // Maldives
    UNHANDLED_COUNTRY(M, W)  // Malawi
    UNHANDLED_COUNTRY(N, A)  // Namibia
    UNHANDLED_COUNTRY(N, G)  // Nigeria
    UNHANDLED_COUNTRY(N, P)  // Nepal
    UNHANDLED_COUNTRY(N, R)  // Nauru
    UNHANDLED_COUNTRY(P, G)  // Papua New Guinea
    UNHANDLED_COUNTRY(P, W)  // Palau
    UNHANDLED_COUNTRY(S, B)  // Solomon Islands
    UNHANDLED_COUNTRY(S, C)  // Seychelles
    UNHANDLED_COUNTRY(S, L)  // Sierra Leone
    UNHANDLED_COUNTRY(S, O)  // Somalia
    UNHANDLED_COUNTRY(S, R)  // Suriname
    UNHANDLED_COUNTRY(S, Z)  // Swaziland
    UNHANDLED_COUNTRY(T, O)  // Tonga
    UNHANDLED_COUNTRY(T, V)  // Tuvalu
    UNHANDLED_COUNTRY(U, G)  // Uganda
    UNHANDLED_COUNTRY(V, C)  // Saint Vincent and the Grenadines
    UNHANDLED_COUNTRY(V, U)  // Vanuatu
    UNHANDLED_COUNTRY(W, S)  // Samoa
    UNHANDLED_COUNTRY(Z, M)  // Zambia
    case kCountryIDUnknown:
    default:                // Unhandled location
    END_UNHANDLED_COUNTRIES(def, ault)
  }
}


// Logo URLs ///////////////////////////////////////////////////////////////////

struct LogoURLs {
  const char* const logo_100_percent_url;
  const char* const logo_200_percent_url;
};

const LogoURLs google_logos = {
  "https://www.google.com/images/chrome_search/google_logo.png",
  "https://www.google.com/images/chrome_search/google_logo_2x.png",
};


////////////////////////////////////////////////////////////////////////////////

void RegisterUserPrefs(PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(prefs::kCountryIDAtInstall,
                                kCountryIDUnknown,
                                PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterListPref(prefs::kSearchProviderOverrides,
                             PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterIntegerPref(prefs::kSearchProviderOverridesVersion,
                                -1,
                                PrefRegistrySyncable::UNSYNCABLE_PREF);
  // Obsolete pref, for migration.
  registry->RegisterIntegerPref(prefs::kGeoIDAtInstall,
                                -1,
                                PrefRegistrySyncable::UNSYNCABLE_PREF);
}

int GetDataVersion(PrefService* prefs) {
  // Allow tests to override the local version.
  return (prefs && prefs->HasPrefPath(prefs::kSearchProviderOverridesVersion)) ?
      prefs->GetInteger(prefs::kSearchProviderOverridesVersion) :
      kCurrentDataVersion;
}

TemplateURL* MakePrepopulatedTemplateURL(
    Profile* profile,
    const string16& name,
    const string16& keyword,
    const base::StringPiece& search_url,
    const base::StringPiece& suggest_url,
    const base::StringPiece& instant_url,
    const base::StringPiece& favicon_url,
    const base::StringPiece& encoding,
    const ListValue& alternate_urls,
    const base::StringPiece& search_terms_replacement_key,
    int id) {

  TemplateURLData data;

  data.short_name = name;
  data.SetKeyword(keyword);
  data.SetURL(search_url.as_string());
  data.suggestions_url = suggest_url.as_string();
  data.instant_url = instant_url.as_string();
  data.favicon_url = GURL(favicon_url.as_string());
  data.show_in_default_list = true;
  data.safe_for_autoreplace = true;
  data.input_encodings.push_back(encoding.as_string());
  data.date_created = base::Time();
  data.last_modified = base::Time();
  data.prepopulate_id = id;
  for (size_t i = 0; i < alternate_urls.GetSize(); ++i) {
    std::string alternate_url;
    alternate_urls.GetString(i, &alternate_url);
    DCHECK(!alternate_url.empty());
    data.alternate_urls.push_back(alternate_url);
  }
  data.search_terms_replacement_key = search_terms_replacement_key.as_string();
  return new TemplateURL(profile, data);
}

void GetPrepopulatedTemplateFromPrefs(Profile* profile,
                                      std::vector<TemplateURL*>* t_urls) {
  if (!profile)
    return;

  const ListValue* list =
      profile->GetPrefs()->GetList(prefs::kSearchProviderOverrides);
  if (!list)
    return;

  size_t num_engines = list->GetSize();
  for (size_t i = 0; i != num_engines; ++i) {
    const DictionaryValue* engine;
    string16 name;
    string16 keyword;
    std::string search_url;
    std::string favicon_url;
    std::string encoding;
    int id;
    // The following fields are required for each search engine configuration.
    if (list->GetDictionary(i, &engine) &&
        engine->GetString("name", &name) && !name.empty() &&
        engine->GetString("keyword", &keyword) && !keyword.empty() &&
        engine->GetString("search_url", &search_url) && !search_url.empty() &&
        engine->GetString("favicon_url", &favicon_url) &&
        !favicon_url.empty() &&
        engine->GetString("encoding", &encoding) && !encoding.empty() &&
        engine->GetInteger("id", &id)) {
      // These fields are optional.
      std::string suggest_url;
      std::string instant_url;
      ListValue empty_list;
      const ListValue* alternate_urls = &empty_list;
      std::string search_terms_replacement_key;
      engine->GetString("suggest_url", &suggest_url);
      engine->GetString("instant_url", &instant_url);
      engine->GetList("alternate_urls", &alternate_urls);
      engine->GetString("search_terms_replacement_key",
          &search_terms_replacement_key);
      t_urls->push_back(MakePrepopulatedTemplateURL(profile, name, keyword,
          search_url, suggest_url, instant_url, favicon_url, encoding,
          *alternate_urls, search_terms_replacement_key, id));
    }
  }
}

// The caller owns the returned TemplateURL.
TemplateURL* MakePrepopulatedTemplateURLFromPrepopulateEngine(
    Profile* profile,
    const PrepopulatedEngine& engine) {

  ListValue alternate_urls;
  if (engine.alternate_urls) {
    for (size_t i = 0; i < engine.alternate_urls_size; ++i)
      alternate_urls.AppendString(std::string(engine.alternate_urls[i]));
  }

  return MakePrepopulatedTemplateURL(profile, base::WideToUTF16(engine.name),
      base::WideToUTF16(engine.keyword), engine.search_url, engine.suggest_url,
      engine.instant_url, engine.favicon_url, engine.encoding, alternate_urls,
      engine.search_terms_replacement_key, engine.id);
}

void GetPrepopulatedEngines(Profile* profile,
                            std::vector<TemplateURL*>* t_urls,
                            size_t* default_search_provider_index) {
  // If there is a set of search engines in the preferences file, it overrides
  // the built-in set.
  *default_search_provider_index = 0;
  GetPrepopulatedTemplateFromPrefs(profile, t_urls);
  if (!t_urls->empty())
    return;

  const PrepopulatedEngine** engines;
  size_t num_engines;
  GetPrepopulationSetFromCountryID(profile ? profile->GetPrefs() : NULL,
                                   &engines, &num_engines);
  for (size_t i = 0; i != num_engines; ++i) {
    t_urls->push_back(
        MakePrepopulatedTemplateURLFromPrepopulateEngine(profile, *engines[i]));
  }
}

TemplateURL* GetPrepopulatedDefaultSearch(Profile* profile) {
  TemplateURL* default_search_provider = NULL;
  ScopedVector<TemplateURL> loaded_urls;
  size_t default_search_index;
  // This could be more efficient.  We are loading all the URLs to only keep
  // the first one.
  GetPrepopulatedEngines(profile, &loaded_urls.get(), &default_search_index);
  if (default_search_index < loaded_urls.size()) {
    default_search_provider = loaded_urls[default_search_index];
    loaded_urls.weak_erase(loaded_urls.begin() + default_search_index);
  }
  return default_search_provider;
}

SearchEngineType GetEngineType(const std::string& url) {
  // Restricted to UI thread because ReplaceSearchTerms() is so restricted.
  using content::BrowserThread;
  DCHECK(!BrowserThread::IsWellKnownThread(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::UI));

  // We may get a valid URL, or we may get the Google prepopulate URL which
  // can't be converted directly to a GURL.  To handle the latter, we first
  // construct a TemplateURL from the provided |url|, then call
  // ReplaceSearchTerms().  This should return a valid URL even when the input
  // has Google base URLs.
  TemplateURLData data;
  data.SetURL(url);
  TemplateURL turl(NULL, data);
  GURL as_gurl(turl.url_ref().ReplaceSearchTerms(
      TemplateURLRef::SearchTermsArgs(ASCIIToUTF16("x"))));
  if (!as_gurl.is_valid())
    return SEARCH_ENGINE_OTHER;

  // Check using origins, in order to more aggressively match search engine
  // types for data imported from other browsers.
  //
  // First special-case Google, because the prepopulate URL for it will not
  // convert to a GURL and thus won't have an origin.  Instead see if the
  // incoming URL's host is "[*.]google.<TLD>".
  if (google_util::IsGoogleHostname(as_gurl.host(),
                                    google_util::DISALLOW_SUBDOMAIN))
    return google.type;

  // Now check the rest of the prepopulate data.
  GURL origin(as_gurl.GetOrigin());
  for (size_t i = 0; i < arraysize(kAllEngines); ++i) {
    GURL engine_url(kAllEngines[i]->search_url);
    if (engine_url.is_valid() && (origin == engine_url.GetOrigin()))
      return kAllEngines[i]->type;
  }

  return SEARCH_ENGINE_OTHER;
}

GURL GetLogoURL(const TemplateURL& template_url, LogoSize size) {
  if (GetEngineType(template_url.url()) == SEARCH_ENGINE_GOOGLE) {
    return GURL((size == LOGO_200_PERCENT) ?
                google_logos.logo_200_percent_url :
                google_logos.logo_100_percent_url);
  }
  return GURL();
}

}  // namespace TemplateURLPrepopulateData
