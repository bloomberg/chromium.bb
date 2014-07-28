// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/input_method_util.h"

#include <algorithm>
#include <functional>
#include <map>
#include <utility>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chromeos/ime/component_extension_ime_manager.h"
#include "chromeos/ime/extension_ime_util.h"
// For SetHardwareKeyboardLayoutForTesting.
#include "chromeos/ime/fake_input_method_delegate.h"
#include "chromeos/ime/input_method_delegate.h"
#include "chromeos/ime/input_method_whitelist.h"
// TODO(nona): move this header from this file.
#include "grit/generated_resources.h"

namespace {

// A mapping from an input method id to a string for the language indicator. The
// mapping is necessary since some input methods belong to the same language.
// For example, both "xkb:us::eng" and "xkb:us:dvorak:eng" are for US English.
const struct {
  const char* engine_id;
  const char* indicator_text;
} kMappingFromIdToIndicatorText[] = {
  // To distinguish from "xkb:jp::jpn"
  // TODO(nona): Make following variables configurable. http://crbug.com/232260.
  { "nacl_mozc_us", "\xe3\x81\x82" },
  { "nacl_mozc_jp", "\xe3\x81\x82" },
  // For simplified Chinese input methods
  { "zh-t-i0-pinyin", "\xe6\x8b\xbc" },  // U+62FC
  { "zh-t-i0-wubi-1986", "\xe4\xba\x94" }, // U+4E94
  // For traditional Chinese input methods
  { "zh-hant-t-i0-und", "\xE6\xB3\xA8" },  // U+9177
  { "zh-hant-t-i0-cangjie-1987", "\xe5\x80\x89" },  // U+5009
  { "zh-hant-t-i0-cangjie-1987-x-m0-simplified", "\xe9\x80\x9f" },  // U+901F
  // For Hangul input method.
  { "hangul_2set", "\xed\x95\x9c" },  // U+D55C
  { "hangul_3set390", "\xed\x95\x9c" },  // U+D55C
  { "hangul_3setfinal", "\xed\x95\x9c" },  // U+D55C
  { "hangul_3setnoshift", "\xed\x95\x9c" },  // U+D55C
  { "hangul_romaja", "\xed\x95\x9c" },  // U+D55C
  { extension_misc::kBrailleImeEngineId,
    // U+2803 U+2817 U+2807 (Unicode braille patterns for the letters 'brl' in
    // English (and many other) braille codes.
    "\xe2\xa0\x83\xe2\xa0\x97\xe2\xa0\x87" },
};

const size_t kMappingFromIdToIndicatorTextLen =
    ARRAYSIZE_UNSAFE(kMappingFromIdToIndicatorText);

// A mapping from an input method id to a resource id for a
// medium length language indicator.
// For those languages that want to display a slightly longer text in the
// "Your input method has changed to..." bubble than in the status tray.
// If an entry is not found in this table the short name is used.
const struct {
  const char* engine_id;
  const int resource_id;
} kMappingImeIdToMediumLenNameResourceId[] = {
  { "hangul_2set", IDS_LANGUAGES_MEDIUM_LEN_NAME_KOREAN },
  { "hangul_3set390", IDS_LANGUAGES_MEDIUM_LEN_NAME_KOREAN },
  { "hangul_3setfinal", IDS_LANGUAGES_MEDIUM_LEN_NAME_KOREAN },
  { "hangul_3setnoshift", IDS_LANGUAGES_MEDIUM_LEN_NAME_KOREAN },
  { "hangul_3setromaja", IDS_LANGUAGES_MEDIUM_LEN_NAME_KOREAN },
  { "zh-t-i0-pinyin", IDS_LANGUAGES_MEDIUM_LEN_NAME_CHINESE_SIMPLIFIED},
  { "zh-t-i0-wubi-1986", IDS_LANGUAGES_MEDIUM_LEN_NAME_CHINESE_SIMPLIFIED },
  { "zh-hant-t-i0-und", IDS_LANGUAGES_MEDIUM_LEN_NAME_CHINESE_TRADITIONAL },
  { "zh-hant-t-i0-cangjie-1987",
    IDS_LANGUAGES_MEDIUM_LEN_NAME_CHINESE_TRADITIONAL },
  { "zh-hant-t-i0-cangjie-1987-x-m0-simplified",
    IDS_LANGUAGES_MEDIUM_LEN_NAME_CHINESE_TRADITIONAL },
  { extension_misc::kBrailleImeEngineId,
    IDS_LANGUAGES_MEDIUM_LEN_NAME_BRAILLE },
};
const size_t kMappingImeIdToMediumLenNameResourceIdLen =
    ARRAYSIZE_UNSAFE(kMappingImeIdToMediumLenNameResourceId);

// Due to asynchronous initialization of component extension manager,
// GetFirstLogingInputMethodIds may miss component extension IMEs. To enable
// component extension IME as the first loging input method, we have to prepare
// component extension IME IDs.
const struct {
  const char* locale;
  const char* layout;
  const char* engine_id;
} kDefaultInputMethodRecommendation[] = {
  { "ja", "us", "nacl_mozc_us" },
  { "ja", "jp", "nacl_mozc_jp" },
  { "zh-CN", "us", "zh-t-i0-pinyin" },
  { "zh-TW", "us", "zh-hant-t-i0-und" },
  { "th", "us", "vkd_th" },
  { "vi", "us", "vkd_vi_tcvn" },
};

// The map from xkb layout to the indicator text.
// Refer to crbug.com/349829.
const char* const kXkbIndicators[][2] = {{"am", "AM"},
                                         {"be", "BE"},
                                         {"bg", "BG"},
                                         {"bg(phonetic)", "BG"},
                                         {"br", "BR"},
                                         {"by", "BY"},
                                         {"ca", "CA"},
                                         {"ca(eng)", "CA"},
                                         {"ca(multix)", "CA"},
                                         {"ch", "CH"},
                                         {"ch(fr)", "CH"},
                                         {"cz", "CZ"},
                                         {"cz(qwerty)", "CS"},
                                         {"de", "DE"},
                                         {"de(neo)", "NEO"},
                                         {"dk", "DK"},
                                         {"ee", "EE"},
                                         {"es", "ES"},
                                         {"es(cat)", "CAS"},
                                         {"fi", "FI"},
                                         {"fr", "FR"},
                                         {"gb(dvorak)", "DV"},
                                         {"gb(extd)", "GB"},
                                         {"ge", "GE"},
                                         {"gr", "GR"},
                                         {"hr", "HR"},
                                         {"hu", "HU"},
                                         {"il", "IL"},
                                         {"is", "IS"},
                                         {"it", "IT"},
                                         {"jp", "JA"},
                                         {"latam", "LA"},
                                         {"lt", "LT"},
                                         {"lv(apostrophe)", "LV"},
                                         {"mn", "MN"},
                                         {"no", "NO"},
                                         {"pl", "PL"},
                                         {"pt", "PT"},
                                         {"ro", "RO"},
                                         {"rs", "RS"},
                                         {"ru", "RU"},
                                         {"ru(phonetic)", "RU"},
                                         {"se", "SE"},
                                         {"si", "SI"},
                                         {"sk", "SK"},
                                         {"tr", "TR"},
                                         {"ua", "UA"},
                                         {"us", "US"},
                                         {"us(altgr-intl)", "EXTD"},
                                         {"us(colemak)", "CO"},
                                         {"us(dvorak)", "DV"},
                                         {"us(intl)", "INTL"}, };

// The extension ID map for migration.
const char* const kExtensionIdMigrationMap[][2] = {
  // Official Japanese IME extension ID.
  {"fpfbhcjppmaeaijcidgiibchfbnhbelj", "gjaehgfemfahhmlgpdfknkhdnemmolop"},
  // Official M17n keyboard extension ID.
  {"habcdindjejkmepknlhkkloncjcpcnbf", "gjaehgfemfahhmlgpdfknkhdnemmolop"},
};

// The engine ID map for migration. This migration is for input method IDs from
// VPD so it's NOT a temporary migration.
const char* const kEngineIdMigrationMap[][2] = {
  {"m17n:", "vkd_"},
  {"ime:zh-t:quick", "zh-hant-t-i0-cangjie-1987-x-m0-simplified"},
  {"ime:ko:hangul", "hangul_2set"},
  {"ime:ko:hangul_2set", "hangul_2set"},
};

const size_t kExtensionIdLen = 32;

const struct EnglishToResouceId {
  const char* english_string_from_ibus;
  int resource_id;
} kEnglishToResourceIdArray[] = {
  // For xkb-layouts.
  { "xkb:am:phonetic:arm", IDS_STATUSBAR_LAYOUT_ARMENIAN_PHONETIC },
  { "xkb:be::fra", IDS_STATUSBAR_LAYOUT_BELGIUM },
  { "xkb:be::ger", IDS_STATUSBAR_LAYOUT_BELGIUM },
  { "xkb:be::nld", IDS_STATUSBAR_LAYOUT_BELGIUM },
  { "xkb:bg::bul", IDS_STATUSBAR_LAYOUT_BULGARIA },
  { "xkb:bg:phonetic:bul", IDS_STATUSBAR_LAYOUT_BULGARIA_PHONETIC },
  { "xkb:br::por", IDS_STATUSBAR_LAYOUT_BRAZIL },
  { "xkb:by::bel", IDS_STATUSBAR_LAYOUT_BELARUSIAN },
  { "xkb:ca::fra", IDS_STATUSBAR_LAYOUT_CANADA },
  { "xkb:ca:eng:eng", IDS_STATUSBAR_LAYOUT_CANADA_ENGLISH },
  { "xkb:ca:multix:fra", IDS_STATUSBAR_LAYOUT_CANADIAN_MULTILINGUAL },
  { "xkb:ch::ger", IDS_STATUSBAR_LAYOUT_SWITZERLAND },
  { "xkb:ch:fr:fra", IDS_STATUSBAR_LAYOUT_SWITZERLAND_FRENCH },
  { "xkb:cz::cze", IDS_STATUSBAR_LAYOUT_CZECHIA },
  { "xkb:cz:qwerty:cze", IDS_STATUSBAR_LAYOUT_CZECHIA_QWERTY },
  { "xkb:de::ger", IDS_STATUSBAR_LAYOUT_GERMANY },
  { "xkb:de:neo:ger", IDS_STATUSBAR_LAYOUT_GERMANY_NEO2 },
  { "xkb:dk::dan", IDS_STATUSBAR_LAYOUT_DENMARK },
  { "xkb:ee::est", IDS_STATUSBAR_LAYOUT_ESTONIA },
  { "xkb:es::spa", IDS_STATUSBAR_LAYOUT_SPAIN },
  { "xkb:es:cat:cat", IDS_STATUSBAR_LAYOUT_SPAIN_CATALAN },
  { "xkb:fi::fin", IDS_STATUSBAR_LAYOUT_FINLAND },
  { "xkb:fr::fra", IDS_STATUSBAR_LAYOUT_FRANCE },
  { "xkb:gb:dvorak:eng", IDS_STATUSBAR_LAYOUT_UNITED_KINGDOM_DVORAK },
  { "xkb:gb:extd:eng", IDS_STATUSBAR_LAYOUT_UNITED_KINGDOM },
  { "xkb:ge::geo", IDS_STATUSBAR_LAYOUT_GEORGIAN },
  { "xkb:gr::gre", IDS_STATUSBAR_LAYOUT_GREECE },
  { "xkb:hr::scr", IDS_STATUSBAR_LAYOUT_CROATIA },
  { "xkb:hu::hun", IDS_STATUSBAR_LAYOUT_HUNGARY },
  { "xkb:ie::ga", IDS_STATUSBAR_LAYOUT_IRISH },
  { "xkb:il::heb", IDS_STATUSBAR_LAYOUT_ISRAEL },
  { "xkb:is::ice", IDS_STATUSBAR_LAYOUT_ICELANDIC },
  { "xkb:it::ita", IDS_STATUSBAR_LAYOUT_ITALY },
  { "xkb:jp::jpn", IDS_STATUSBAR_LAYOUT_JAPAN },
  { "xkb:latam::spa", IDS_STATUSBAR_LAYOUT_LATIN_AMERICAN },
  { "xkb:lt::lit", IDS_STATUSBAR_LAYOUT_LITHUANIA },
  { "xkb:lv:apostrophe:lav", IDS_STATUSBAR_LAYOUT_LATVIA },
  { "xkb:mn::mon", IDS_STATUSBAR_LAYOUT_MONGOLIAN },
  { "xkb:nl::nld", IDS_STATUSBAR_LAYOUT_NETHERLANDS },
  { "xkb:no::nob", IDS_STATUSBAR_LAYOUT_NORWAY },
  { "xkb:pl::pol", IDS_STATUSBAR_LAYOUT_POLAND },
  { "xkb:pt::por", IDS_STATUSBAR_LAYOUT_PORTUGAL },
  { "xkb:ro::rum", IDS_STATUSBAR_LAYOUT_ROMANIA },
  { "xkb:rs::srp", IDS_STATUSBAR_LAYOUT_SERBIA },
  { "xkb:ru::rus", IDS_STATUSBAR_LAYOUT_RUSSIA },
  { "xkb:ru:phonetic:rus", IDS_STATUSBAR_LAYOUT_RUSSIA_PHONETIC },
  { "xkb:se::swe", IDS_STATUSBAR_LAYOUT_SWEDEN },
  { "xkb:si::slv", IDS_STATUSBAR_LAYOUT_SLOVENIA },
  { "xkb:sk::slo", IDS_STATUSBAR_LAYOUT_SLOVAKIA },
  { "xkb:tr::tur", IDS_STATUSBAR_LAYOUT_TURKEY },
  { "xkb:ua::ukr", IDS_STATUSBAR_LAYOUT_UKRAINE },
  { "xkb:us::eng", IDS_STATUSBAR_LAYOUT_USA },
  { "xkb:us::fil", IDS_STATUSBAR_LAYOUT_USA },
  { "xkb:us::ind", IDS_STATUSBAR_LAYOUT_USA },
  { "xkb:us::msa", IDS_STATUSBAR_LAYOUT_USA },
  { "xkb:us:altgr-intl:eng", IDS_STATUSBAR_LAYOUT_USA_EXTENDED },
  { "xkb:us:colemak:eng", IDS_STATUSBAR_LAYOUT_USA_COLEMAK },
  { "xkb:us:dvorak:eng", IDS_STATUSBAR_LAYOUT_USA_DVORAK },
  { "xkb:us:intl:eng", IDS_STATUSBAR_LAYOUT_USA_INTERNATIONAL },
  { "xkb:us:intl:nld", IDS_STATUSBAR_LAYOUT_USA_INTERNATIONAL },
  { "xkb:us:intl:por", IDS_STATUSBAR_LAYOUT_USA_INTERNATIONAL },
};
const size_t kEnglishToResourceIdArraySize =
    arraysize(kEnglishToResourceIdArray);

}  // namespace

namespace chromeos {

namespace input_method {

InputMethodUtil::InputMethodUtil(InputMethodDelegate* delegate)
    : delegate_(delegate) {
  InputMethodDescriptors default_input_methods;
  default_input_methods.push_back(GetFallbackInputMethodDescriptor());
  ResetInputMethods(default_input_methods);

  // Initialize a map from English string to Chrome string resource ID as well.
  for (size_t i = 0; i < kEnglishToResourceIdArraySize; ++i) {
    const EnglishToResouceId& map_entry = kEnglishToResourceIdArray[i];
    const bool result = english_to_resource_id_.insert(std::make_pair(
        map_entry.english_string_from_ibus, map_entry.resource_id)).second;
    DCHECK(result) << "Duplicated string is found: "
                   << map_entry.english_string_from_ibus;
  }

  // Initialize the map from xkb layout to indicator text.
  for (size_t i = 0; i < arraysize(kXkbIndicators); ++i) {
    xkb_layout_to_indicator_[kXkbIndicators[i][0]] = kXkbIndicators[i][1];
  }
}

InputMethodUtil::~InputMethodUtil() {
}

bool InputMethodUtil::TranslateStringInternal(
    const std::string& english_string, base::string16 *out_string) const {
  DCHECK(out_string);
  // |english_string| could be an input method id. So legacy xkb id is required
  // to get the translated string.
  std::string key_string = extension_ime_util::MaybeGetLegacyXkbId(
      english_string);
  HashType::const_iterator iter = english_to_resource_id_.find(key_string);

  if (iter == english_to_resource_id_.end()) {
    // TODO(yusukes): Write Autotest which checks if all display names and all
    // property names for supported input methods are listed in the resource
    // ID array (crosbug.com/4572).
    LOG(ERROR) << "Resource ID is not found for: " << english_string
               << ", " << key_string;
    return false;
  }

  *out_string = delegate_->GetLocalizedString(iter->second);
  return true;
}

base::string16 InputMethodUtil::TranslateString(
    const std::string& english_string) const {
  base::string16 localized_string;
  if (TranslateStringInternal(english_string, &localized_string)) {
    return localized_string;
  }
  return base::UTF8ToUTF16(english_string);
}

bool InputMethodUtil::IsValidInputMethodId(
    const std::string& input_method_id) const {
  // We can't check the component extension is whilelisted or not here because
  // it might not be initialized.
  return GetInputMethodDescriptorFromId(input_method_id) != NULL ||
      extension_ime_util::IsComponentExtensionIME(input_method_id);
}

// static
bool InputMethodUtil::IsKeyboardLayout(const std::string& input_method_id) {
  return StartsWithASCII(input_method_id, "xkb:", false) ||
      extension_ime_util::IsKeyboardLayoutExtension(input_method_id);
}

std::string InputMethodUtil::GetKeyboardLayoutName(
    const std::string& input_method_id) const {
  InputMethodIdToDescriptorMap::const_iterator iter
      = id_to_descriptor_.find(input_method_id);
  return (iter == id_to_descriptor_.end()) ?
      "" : iter->second.GetPreferredKeyboardLayout();
}

std::string InputMethodUtil::GetInputMethodDisplayNameFromId(
    const std::string& input_method_id) const {
  base::string16 display_name;
  if (!extension_ime_util::IsExtensionIME(input_method_id) &&
      TranslateStringInternal(input_method_id, &display_name)) {
    return base::UTF16ToUTF8(display_name);
  }
  // Return an empty string if the display name is not found.
  return "";
}

base::string16 InputMethodUtil::GetInputMethodShortName(
    const InputMethodDescriptor& input_method) const {
  // For the status area, we use two-letter, upper-case language code like
  // "US" and "JP".

  // Use the indicator string if set.
  if (!input_method.indicator().empty()) {
    return base::UTF8ToUTF16(input_method.indicator());
  }

  base::string16 text;
  // Check special cases first.
  for (size_t i = 0; i < kMappingFromIdToIndicatorTextLen; ++i) {
    if (extension_ime_util::GetInputMethodIDByEngineID(
        kMappingFromIdToIndicatorText[i].engine_id) == input_method.id()) {
      text = base::UTF8ToUTF16(kMappingFromIdToIndicatorText[i].indicator_text);
      break;
    }
  }

  // Display the keyboard layout name when using a keyboard layout.
  if (text.empty() && IsKeyboardLayout(input_method.id())) {
    std::map<std::string, std::string>::const_iterator it =
        xkb_layout_to_indicator_.find(GetKeyboardLayoutName(input_method.id()));
    if (it != xkb_layout_to_indicator_.end())
      text = base::UTF8ToUTF16(it->second);
  }

  // TODO(yusukes): Some languages have two or more input methods. For example,
  // Thai has 3, Vietnamese has 4. If these input methods could be activated at
  // the same time, we should do either of the following:
  //   (1) Add mappings to |kMappingFromIdToIndicatorText|
  //   (2) Add suffix (1, 2, ...) to |text| when ambiguous.

  if (text.empty()) {
    const size_t kMaxLanguageNameLen = 2;
    DCHECK(!input_method.language_codes().empty());
    const std::string language_code = input_method.language_codes().at(0);
    text = StringToUpperASCII(base::UTF8ToUTF16(language_code)).substr(
        0, kMaxLanguageNameLen);
  }
  DCHECK(!text.empty()) << input_method.id();
  return text;
}

base::string16 InputMethodUtil::GetInputMethodMediumName(
    const InputMethodDescriptor& input_method) const {
  // For the "Your input method has changed to..." bubble. In most cases
  // it uses the same name as the short name, unless found in a table
  // for medium length names.
  for (size_t i = 0; i < kMappingImeIdToMediumLenNameResourceIdLen; ++i) {
    if (extension_ime_util::GetInputMethodIDByEngineID(
        kMappingImeIdToMediumLenNameResourceId[i].engine_id) ==
        input_method.id()) {
      return delegate_->GetLocalizedString(
          kMappingImeIdToMediumLenNameResourceId[i].resource_id);
    }
  }
  return GetInputMethodShortName(input_method);
}

base::string16 InputMethodUtil::GetInputMethodLongName(
    const InputMethodDescriptor& input_method) const {
  if (!input_method.name().empty() && !IsKeyboardLayout(input_method.id())) {
    // If the descriptor has a name, use it.
    return base::UTF8ToUTF16(input_method.name());
  }

  // We don't show language here.  Name of keyboard layout or input method
  // usually imply (or explicitly include) its language.

  // Special case for German, French and Dutch: these languages have multiple
  // keyboard layouts and share the same layout of keyboard (Belgian). We need
  // to show explicitly the language for the layout. For Arabic, Amharic, and
  // Indic languages: they share "Standard Input Method".
  const base::string16 standard_input_method_text =
      delegate_->GetLocalizedString(
          IDS_OPTIONS_SETTINGS_LANGUAGES_M17N_STANDARD_INPUT_METHOD);
  DCHECK(!input_method.language_codes().empty());
  const std::string language_code = input_method.language_codes().at(0);

  base::string16 text = TranslateString(input_method.id());
  if (text == standard_input_method_text ||
             language_code == "de" ||
             language_code == "fr" ||
             language_code == "nl") {
    const base::string16 language_name = delegate_->GetDisplayLanguageName(
        language_code);

    text = language_name + base::UTF8ToUTF16(" - ") + text;
  }

  DCHECK(!text.empty());
  return text;
}

const InputMethodDescriptor* InputMethodUtil::GetInputMethodDescriptorFromId(
    const std::string& input_method_id) const {
  InputMethodIdToDescriptorMap::const_iterator iter =
      id_to_descriptor_.find(input_method_id);
  if (iter == id_to_descriptor_.end())
    return NULL;
  return &(iter->second);
}

bool InputMethodUtil::GetInputMethodIdsFromLanguageCode(
    const std::string& normalized_language_code,
    InputMethodType type,
    std::vector<std::string>* out_input_method_ids) const {
  return GetInputMethodIdsFromLanguageCodeInternal(
      language_code_to_ids_,
      normalized_language_code, type, out_input_method_ids);
}

bool InputMethodUtil::GetInputMethodIdsFromLanguageCodeInternal(
    const std::multimap<std::string, std::string>& language_code_to_ids,
    const std::string& normalized_language_code,
    InputMethodType type,
    std::vector<std::string>* out_input_method_ids) const {
  DCHECK(out_input_method_ids);
  out_input_method_ids->clear();

  bool result = false;
  std::pair<LanguageCodeToIdsMap::const_iterator,
      LanguageCodeToIdsMap::const_iterator> range =
      language_code_to_ids.equal_range(normalized_language_code);
  for (LanguageCodeToIdsMap::const_iterator iter = range.first;
       iter != range.second; ++iter) {
    const std::string& input_method_id = iter->second;
    if ((type == kAllInputMethods) || IsKeyboardLayout(input_method_id)) {
      out_input_method_ids->push_back(input_method_id);
      result = true;
    }
  }
  if ((type == kAllInputMethods) && !result) {
    DVLOG(1) << "Unknown language code: " << normalized_language_code;
  }
  return result;
}

void InputMethodUtil::GetFirstLoginInputMethodIds(
    const std::string& language_code,
    const InputMethodDescriptor& current_input_method,
    std::vector<std::string>* out_input_method_ids) const {
  out_input_method_ids->clear();

  // First, add the current keyboard layout (one used on the login screen).
  out_input_method_ids->push_back(current_input_method.id());

  const std::string current_layout
      = current_input_method.GetPreferredKeyboardLayout();
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kDefaultInputMethodRecommendation);
       ++i) {
    if (kDefaultInputMethodRecommendation[i].locale == language_code &&
        kDefaultInputMethodRecommendation[i].layout == current_layout) {
      out_input_method_ids->push_back(
          extension_ime_util::GetInputMethodIDByEngineID(
              kDefaultInputMethodRecommendation[i].engine_id));
      return;
    }
  }

  // Second, find the most popular input method associated with the
  // current UI language. The input method IDs returned from
  // GetInputMethodIdsFromLanguageCode() are sorted by popularity, hence
  // our basic strategy is to pick the first one, but it's a bit more
  // complicated as shown below.
  std::string most_popular_id;
  std::vector<std::string> input_method_ids;
  // This returns the input methods sorted by popularity.
  GetInputMethodIdsFromLanguageCode(
      language_code, kAllInputMethods, &input_method_ids);
  for (size_t i = 0; i < input_method_ids.size(); ++i) {
    const std::string& input_method_id = input_method_ids[i];
    // Pick the first one.
    if (most_popular_id.empty())
      most_popular_id = input_method_id;

    // Check if there is one that matches the current keyboard layout, but
    // not the current keyboard itself. This is useful if there are
    // multiple keyboard layout choices for one input method. For
    // instance, Mozc provides three choices: mozc (US keyboard), mozc-jp
    // (JP keyboard), mozc-dv (Dvorak).
    const InputMethodDescriptor* descriptor =
        GetInputMethodDescriptorFromId(input_method_id);
    if (descriptor &&
        descriptor->id() != current_input_method.id() &&
        descriptor->GetPreferredKeyboardLayout() ==
        current_input_method.GetPreferredKeyboardLayout()) {
      most_popular_id = input_method_id;
      break;
    }
  }
  // Add the most popular input method ID, if it's different from the
  // current input method.
  if (most_popular_id != current_input_method.id()) {
    out_input_method_ids->push_back(most_popular_id);
  }
}

void InputMethodUtil::GetLanguageCodesFromInputMethodIds(
    const std::vector<std::string>& input_method_ids,
    std::vector<std::string>* out_language_codes) const {
  out_language_codes->clear();

  for (size_t i = 0; i < input_method_ids.size(); ++i) {
    const std::string& input_method_id = input_method_ids[i];
    const InputMethodDescriptor* input_method =
        GetInputMethodDescriptorFromId(input_method_id);
    if (!input_method) {
      DVLOG(1) << "Unknown input method ID: " << input_method_ids[i];
      continue;
    }
    DCHECK(!input_method->language_codes().empty());
    const std::string language_code = input_method->language_codes().at(0);
    // Add it if it's not already present.
    if (std::count(out_language_codes->begin(), out_language_codes->end(),
                   language_code) == 0) {
      out_language_codes->push_back(language_code);
    }
  }
}

std::string InputMethodUtil::GetLanguageDefaultInputMethodId(
    const std::string& language_code) {
  std::vector<std::string> candidates;
  GetInputMethodIdsFromLanguageCode(
      language_code, input_method::kKeyboardLayoutsOnly, &candidates);
  if (candidates.size())
    return candidates.front();

  return std::string();
}

bool InputMethodUtil::MigrateInputMethods(
    std::vector<std::string>* input_method_ids) {
  bool rewritten = false;
  std::vector<std::string>& ids = *input_method_ids;
  for (size_t i = 0; i < ids.size(); ++i) {
    std::string engine_id = ids[i];
    // Migrates some Engine IDs from VPD.
    for (size_t j = 0; j < arraysize(kEngineIdMigrationMap); ++j) {
      size_t pos = engine_id.find(kEngineIdMigrationMap[j][0]);
      if (pos == 0)
        engine_id.replace(pos, strlen(kEngineIdMigrationMap[j][0]),
                          kExtensionIdMigrationMap[j][1]);
    }
    std::string id =
        extension_ime_util::GetInputMethodIDByEngineID(engine_id);
    // Migrates old ime id's to new ones.
    for (size_t j = 0; j < arraysize(kExtensionIdMigrationMap); ++j) {
      size_t pos = id.find(kExtensionIdMigrationMap[j][0]);
      if (pos != std::string::npos)
        id.replace(pos, kExtensionIdLen, kExtensionIdMigrationMap[j][1]);
      if (id != ids[i]) {
        ids[i] = id;
        rewritten = true;
      }
    }
  }
  if (rewritten) {
    // Removes the duplicates.
    std::vector<std::string> new_ids;
    for (size_t i = 0; i < ids.size(); ++i) {
      if (std::find(new_ids.begin(), new_ids.end(), ids[i]) == new_ids.end())
        new_ids.push_back(ids[i]);
    }
    ids.swap(new_ids);
  }
  return rewritten;
}

void InputMethodUtil::UpdateHardwareLayoutCache() {
  DCHECK(thread_checker_.CalledOnValidThread());
  hardware_layouts_.clear();
  hardware_login_layouts_.clear();
  if (cached_hardware_layouts_.empty())
    Tokenize(delegate_->GetHardwareKeyboardLayouts(), ",",
             &cached_hardware_layouts_);
  hardware_layouts_ = cached_hardware_layouts_;
  MigrateInputMethods(&hardware_layouts_);

  for (size_t i = 0; i < hardware_layouts_.size(); ++i) {
    if (IsLoginKeyboard(hardware_layouts_[i]))
      hardware_login_layouts_.push_back(hardware_layouts_[i]);
  }
  if (hardware_layouts_.empty()) {
    // This is totally fine if it's empty. The hardware keyboard layout is
    // not stored if startup_manifest.json (OEM customization data) is not
    // present (ex. Cr48 doen't have that file).
    hardware_layouts_.push_back(GetFallbackInputMethodDescriptor().id());
  }

  if (hardware_login_layouts_.empty())
    hardware_login_layouts_.push_back(GetFallbackInputMethodDescriptor().id());
}

void InputMethodUtil::SetHardwareKeyboardLayoutForTesting(
    const std::string& layout) {
  delegate_->SetHardwareKeyboardLayoutForTesting(layout);
  cached_hardware_layouts_.clear();
  UpdateHardwareLayoutCache();
}

const std::vector<std::string>&
    InputMethodUtil::GetHardwareInputMethodIds() {
  DCHECK(thread_checker_.CalledOnValidThread());
  UpdateHardwareLayoutCache();
  return hardware_layouts_;
}

const std::vector<std::string>&
    InputMethodUtil::GetHardwareLoginInputMethodIds() {
  DCHECK(thread_checker_.CalledOnValidThread());
  UpdateHardwareLayoutCache();
  return hardware_login_layouts_;
}

bool InputMethodUtil::IsLoginKeyboard(const std::string& input_method_id)
    const {
  const InputMethodDescriptor* ime =
      GetInputMethodDescriptorFromId(input_method_id);
  return ime ? ime->is_login_keyboard() : false;
}

void InputMethodUtil::AppendInputMethods(const InputMethodDescriptors& imes) {
  for (size_t i = 0; i < imes.size(); ++i) {
    const InputMethodDescriptor& input_method = imes[i];
    DCHECK(!input_method.language_codes().empty());
    const std::vector<std::string>& language_codes =
        input_method.language_codes();
    id_to_descriptor_[input_method.id()] = input_method;

    typedef LanguageCodeToIdsMap::const_iterator It;
    for (size_t j = 0; j < language_codes.size(); ++j) {
      std::pair<It, It> range =
          language_code_to_ids_.equal_range(language_codes[j]);
      It it = range.first;
      for (; it != range.second; ++it) {
        if (it->second == input_method.id())
          break;
      }
      if (it == range.second)
        language_code_to_ids_.insert(
            std::make_pair(language_codes[j], input_method.id()));
    }
  }
}

void InputMethodUtil::ResetInputMethods(const InputMethodDescriptors& imes) {
  // Clear the existing maps.
  language_code_to_ids_.clear();
  id_to_descriptor_.clear();

  AppendInputMethods(imes);
}

void InputMethodUtil::InitXkbInputMethodsForTesting() {
  cached_hardware_layouts_.clear();
  ResetInputMethods(*(InputMethodWhitelist().GetSupportedInputMethods()));
}

const InputMethodUtil::InputMethodIdToDescriptorMap&
InputMethodUtil::GetIdToDesciptorMapForTesting() {
  return id_to_descriptor_;
}

InputMethodDescriptor InputMethodUtil::GetFallbackInputMethodDescriptor() {
  std::vector<std::string> layouts;
  layouts.push_back("us");
  std::vector<std::string> languages;
  languages.push_back("en-US");
  return InputMethodDescriptor(
      extension_ime_util::GetInputMethodIDByEngineID("xkb:us::eng"),
      "",
      "US",
      layouts,
      languages,
      true,  // login keyboard.
      GURL(),  // options page, not available.
      GURL()); // input view page, not available.
}

}  // namespace input_method
}  // namespace chromeos
