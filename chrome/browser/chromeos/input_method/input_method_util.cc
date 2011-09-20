// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/input_method_util.h"

#include <algorithm>
#include <functional>
#include <map>
#include <utility>

#include "unicode/uloc.h"

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/input_method/ibus_controller.h"
#include "chrome/browser/chromeos/input_method/ibus_input_methods.h"
#include "chrome/browser/chromeos/input_method/input_method_manager.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_collator.h"

namespace chromeos {
namespace input_method {

namespace {

const struct EnglishToResouceId {
  const char* english_string_from_ibus;
  int resource_id;
} kEnglishToResourceIdArray[] = {
  // For ibus-mozc.
  { "Direct input", IDS_STATUSBAR_IME_JAPANESE_IME_STATUS_DIRECT_INPUT },
  { "Hiragana", IDS_STATUSBAR_IME_JAPANESE_IME_STATUS_HIRAGANA },
  { "Katakana", IDS_STATUSBAR_IME_JAPANESE_IME_STATUS_KATAKANA },
  { "Half width katakana",  // small k is not a typo.
    IDS_STATUSBAR_IME_JAPANESE_IME_STATUS_HALF_WIDTH_KATAKANA },
  { "Latin", IDS_STATUSBAR_IME_JAPANESE_IME_STATUS_LATIN },
  { "Wide Latin", IDS_STATUSBAR_IME_JAPANESE_IME_STATUS_WIDE_LATIN },

  // For ibus-mozc-hangul
  { "Hanja mode", IDS_STATUSBAR_IME_KOREAN_HANJA_INPUT_MODE },
  { "Hangul mode", IDS_STATUSBAR_IME_KOREAN_HANGUL_INPUT_MODE },

  // For ibus-pinyin.
  { "Full/Half width",
    IDS_STATUSBAR_IME_CHINESE_PINYIN_TOGGLE_FULL_HALF },
  { "Full/Half width punctuation",
    IDS_STATUSBAR_IME_CHINESE_PINYIN_TOGGLE_FULL_HALF_PUNCTUATION },
  { "Simplfied/Traditional Chinese",
    IDS_STATUSBAR_IME_CHINESE_PINYIN_TOGGLE_S_T_CHINESE },
  { "Chinese",
    IDS_STATUSBAR_IME_CHINESE_PINYIN_TOGGLE_CHINESE_ENGLISH },

  // For ibus-mozc-chewing.
  { "English",
    IDS_STATUSBAR_IME_CHINESE_MOZC_CHEWING_ENGLISH_MODE },
  { "_Chinese",
    IDS_STATUSBAR_IME_CHINESE_MOZC_CHEWING_CHINESE_MODE },
  { "Full-width English",
    IDS_STATUSBAR_IME_CHINESE_MOZC_CHEWING_FULL_WIDTH_ENGLISH_MODE },

  // For the "Languages and Input" dialog.
  { "m17n:ar:kbd", IDS_OPTIONS_SETTINGS_LANGUAGES_M17N_STANDARD_INPUT_METHOD },
  { "m17n:hi:itrans",  // also uses the "STANDARD_INPUT_METHOD" id.
    IDS_OPTIONS_SETTINGS_LANGUAGES_M17N_STANDARD_INPUT_METHOD },
  { "m17n:zh:cangjie",
    IDS_OPTIONS_SETTINGS_LANGUAGES_M17N_CHINESE_CANGJIE_INPUT_METHOD },
  { "m17n:zh:quick",
    IDS_OPTIONS_SETTINGS_LANGUAGES_M17N_CHINESE_QUICK_INPUT_METHOD },
  { "m17n:fa:isiri",
    IDS_OPTIONS_SETTINGS_LANGUAGES_M17N_PERSIAN_ISIRI_2901_INPUT_METHOD },
  { "m17n:th:kesmanee",
    IDS_OPTIONS_SETTINGS_LANGUAGES_M17N_THAI_KESMANEE_INPUT_METHOD },
  { "m17n:th:tis820",
    IDS_OPTIONS_SETTINGS_LANGUAGES_M17N_THAI_TIS820_INPUT_METHOD },
  { "m17n:th:pattachote",
    IDS_OPTIONS_SETTINGS_LANGUAGES_M17N_THAI_PATTACHOTE_INPUT_METHOD },
  { "m17n:vi:tcvn",
    IDS_OPTIONS_SETTINGS_LANGUAGES_M17N_VIETNAMESE_TCVN_INPUT_METHOD },
  { "m17n:vi:telex",
    IDS_OPTIONS_SETTINGS_LANGUAGES_M17N_VIETNAMESE_TELEX_INPUT_METHOD },
  { "m17n:vi:viqr",
    IDS_OPTIONS_SETTINGS_LANGUAGES_M17N_VIETNAMESE_VIQR_INPUT_METHOD },
  { "m17n:vi:vni",
    IDS_OPTIONS_SETTINGS_LANGUAGES_M17N_VIETNAMESE_VNI_INPUT_METHOD },
  { "m17n:bn:itrans",
    IDS_OPTIONS_SETTINGS_LANGUAGES_M17N_STANDARD_INPUT_METHOD },
  { "m17n:gu:itrans",
    IDS_OPTIONS_SETTINGS_LANGUAGES_M17N_STANDARD_INPUT_METHOD },
  { "m17n:ml:itrans",
    IDS_OPTIONS_SETTINGS_LANGUAGES_M17N_STANDARD_INPUT_METHOD },
  { "m17n:mr:itrans",
    IDS_OPTIONS_SETTINGS_LANGUAGES_M17N_STANDARD_INPUT_METHOD },
  { "m17n:ta:itrans",
    IDS_OPTIONS_SETTINGS_LANGUAGES_M17N_STANDARD_INPUT_METHOD },
  { "m17n:am:sera",
    IDS_OPTIONS_SETTINGS_LANGUAGES_M17N_STANDARD_INPUT_METHOD },
  { "m17n:te:itrans",
    IDS_OPTIONS_SETTINGS_LANGUAGES_M17N_STANDARD_INPUT_METHOD },
  { "m17n:kn:itrans",
    IDS_OPTIONS_SETTINGS_LANGUAGES_M17N_STANDARD_INPUT_METHOD },

  { "mozc-chewing",
    IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_INPUT_METHOD },
  { "pinyin", IDS_OPTIONS_SETTINGS_LANGUAGES_PINYIN_INPUT_METHOD },
  { "pinyin-dv",
    IDS_OPTIONS_SETTINGS_LANGUAGES_PINYIN_DV_INPUT_METHOD },
#if defined(GOOGLE_CHROME_BUILD)
  { "mozc",
    IDS_OPTIONS_SETTINGS_LANGUAGES_JAPANESE_GOOGLE_US_INPUT_METHOD },
  { "mozc-dv",
    IDS_OPTIONS_SETTINGS_LANGUAGES_JAPANESE_GOOGLE_US_DV_INPUT_METHOD },
  { "mozc-jp",
    IDS_OPTIONS_SETTINGS_LANGUAGES_JAPANESE_GOOGLE_JP_INPUT_METHOD },
#else
  { "mozc",
    IDS_OPTIONS_SETTINGS_LANGUAGES_JAPANESE_MOZC_US_INPUT_METHOD },
  { "mozc-dv",
    IDS_OPTIONS_SETTINGS_LANGUAGES_JAPANESE_MOZC_US_DV_INPUT_METHOD },
  { "mozc-jp",
    IDS_OPTIONS_SETTINGS_LANGUAGES_JAPANESE_MOZC_JP_INPUT_METHOD },
#endif  // if defined(GOOGLE_CHROME_BUILD)
  { "zinnia-japanese",
    IDS_OPTIONS_SETTINGS_LANGUAGES_JAPANESE_HANDWRITING_INPUT_METHOD },
  { "mozc-hangul", IDS_OPTIONS_SETTINGS_LANGUAGES_KOREAN_INPUT_METHOD },

  // For ibus-xkb-layouts engine: third_party/ibus-xkb-layouts/files
  { "xkb:jp::jpn", IDS_STATUSBAR_LAYOUT_JAPAN },
  { "xkb:si::slv", IDS_STATUSBAR_LAYOUT_SLOVENIA },
  { "xkb:de::ger", IDS_STATUSBAR_LAYOUT_GERMANY },
  { "xkb:de:neo:ger", IDS_STATUSBAR_LAYOUT_GERMANY_NEO2 },
  { "xkb:it::ita", IDS_STATUSBAR_LAYOUT_ITALY },
  { "xkb:ee::est", IDS_STATUSBAR_LAYOUT_ESTONIA },
  { "xkb:hu::hun", IDS_STATUSBAR_LAYOUT_HUNGARY },
  { "xkb:pl::pol", IDS_STATUSBAR_LAYOUT_POLAND },
  { "xkb:dk::dan", IDS_STATUSBAR_LAYOUT_DENMARK },
  { "xkb:hr::scr", IDS_STATUSBAR_LAYOUT_CROATIA },
  { "xkb:br::por", IDS_STATUSBAR_LAYOUT_BRAZIL },
  { "xkb:rs::srp", IDS_STATUSBAR_LAYOUT_SERBIA },
  { "xkb:cz::cze", IDS_STATUSBAR_LAYOUT_CZECHIA },
  { "xkb:us:dvorak:eng", IDS_STATUSBAR_LAYOUT_USA_DVORAK },
  { "xkb:us:colemak:eng", IDS_STATUSBAR_LAYOUT_USA_COLEMAK },
  { "xkb:ro::rum", IDS_STATUSBAR_LAYOUT_ROMANIA },
  { "xkb:us::eng", IDS_STATUSBAR_LAYOUT_USA },
  { "xkb:us:altgr-intl:eng", IDS_STATUSBAR_LAYOUT_USA_EXTENDED },
  { "xkb:us:intl:eng", IDS_STATUSBAR_LAYOUT_USA_INTERNATIONAL },
  { "xkb:lt::lit", IDS_STATUSBAR_LAYOUT_LITHUANIA },
  { "xkb:gb:extd:eng", IDS_STATUSBAR_LAYOUT_UNITED_KINGDOM },
  { "xkb:gb:dvorak:eng", IDS_STATUSBAR_LAYOUT_UNITED_KINGDOM_DVORAK },
  { "xkb:sk::slo", IDS_STATUSBAR_LAYOUT_SLOVAKIA },
  { "xkb:ru::rus", IDS_STATUSBAR_LAYOUT_RUSSIA },
  { "xkb:ru:phonetic:rus", IDS_STATUSBAR_LAYOUT_RUSSIA_PHONETIC },
  { "xkb:gr::gre", IDS_STATUSBAR_LAYOUT_GREECE },
  { "xkb:be::fra", IDS_STATUSBAR_LAYOUT_BELGIUM },
  { "xkb:be::ger", IDS_STATUSBAR_LAYOUT_BELGIUM },
  { "xkb:be::nld", IDS_STATUSBAR_LAYOUT_BELGIUM },
  { "xkb:bg::bul", IDS_STATUSBAR_LAYOUT_BULGARIA },
  { "xkb:bg:phonetic:bul", IDS_STATUSBAR_LAYOUT_BULGARIA_PHONETIC },
  { "xkb:ch::ger", IDS_STATUSBAR_LAYOUT_SWITZERLAND },
  { "xkb:ch:fr:fra", IDS_STATUSBAR_LAYOUT_SWITZERLAND_FRENCH },
  { "xkb:tr::tur", IDS_STATUSBAR_LAYOUT_TURKEY },
  { "xkb:pt::por", IDS_STATUSBAR_LAYOUT_PORTUGAL },
  { "xkb:es::spa", IDS_STATUSBAR_LAYOUT_SPAIN },
  { "xkb:fi::fin", IDS_STATUSBAR_LAYOUT_FINLAND },
  { "xkb:ua::ukr", IDS_STATUSBAR_LAYOUT_UKRAINE },
  { "xkb:es:cat:cat", IDS_STATUSBAR_LAYOUT_SPAIN_CATALAN },
  { "xkb:fr::fra", IDS_STATUSBAR_LAYOUT_FRANCE },
  { "xkb:no::nob", IDS_STATUSBAR_LAYOUT_NORWAY },
  { "xkb:se::swe", IDS_STATUSBAR_LAYOUT_SWEDEN },
  { "xkb:nl::nld", IDS_STATUSBAR_LAYOUT_NETHERLANDS },
  { "xkb:latam::spa", IDS_STATUSBAR_LAYOUT_LATIN_AMERICAN },
  { "xkb:lv:apostrophe:lav", IDS_STATUSBAR_LAYOUT_LATVIA },
  { "xkb:ca::fra", IDS_STATUSBAR_LAYOUT_CANADA },
  { "xkb:ca:eng:eng", IDS_STATUSBAR_LAYOUT_CANADA_ENGLISH },
  { "xkb:il::heb", IDS_STATUSBAR_LAYOUT_ISRAEL },
  { "xkb:kr:kr104:kor", IDS_STATUSBAR_LAYOUT_KOREA_104 },

  { "english-m", IDS_STATUSBAR_LAYOUT_USA_MYSTERY },
};
const size_t kEnglishToResourceIdArraySize =
    arraysize(kEnglishToResourceIdArray);

// There are some differences between ISO 639-2 (T) and ISO 639-2 B, and
// some language codes are not recognized by ICU (i.e. ICU cannot convert
// these codes to two-letter language codes and display names). Hence we
// convert these codes to ones that ICU recognize.
//
// See http://en.wikipedia.org/wiki/List_of_ISO_639-1_codes for details.
const char* kIso639VariantMapping[][2] = {
  { "cze", "ces" },
  { "ger", "deu" },
  { "gre", "ell" },
  // "scr" is not a ISO 639 code. For some reason, evdev.xml uses "scr" as
  // the language code for Croatian.
  { "scr", "hrv" },
  { "rum", "ron" },
  { "slo", "slk" },
};

// The comparator is used for sorting language codes by their
// corresponding language names, using the ICU collator.
struct CompareLanguageCodesByLanguageName
    : std::binary_function<const std::string&, const std::string&, bool> {
  explicit CompareLanguageCodesByLanguageName(icu::Collator* collator)
      : collator_(collator) {
  }

  // Calling GetLanguageDisplayNameFromCode() in the comparator is not
  // efficient, but acceptable as the function is cheap, and the language
  // list is short (about 60 at most).
  bool operator()(const std::string& s1, const std::string& s2) const {
    const string16 key1 = InputMethodUtil::GetLanguageDisplayNameFromCode(s1);
    const string16 key2 = InputMethodUtil::GetLanguageDisplayNameFromCode(s2);
    return l10n_util::StringComparator<string16>(collator_)(key1, key2);
  }

 private:
  icu::Collator* collator_;
};

}  // namespace

const ExtraLanguage kExtraLanguages[] = {
  // Language Code  Input Method ID
  { "en-AU",        "xkb:us::eng" },  // For Austrailia, use US keyboard layout.
  { "id",           "xkb:us::eng" },  // For Indonesian, use US keyboard layout.
  // The code "fil" comes from l10_util.cc.
  { "fil",          "xkb:us::eng" },  // For Filipino, use US keyboard layout.
  // For Netherlands, use US international keyboard layout.
  { "nl",           "xkb:us:intl:eng" },
  // The code "es-419" comes from l10_util.cc.
  // For Spanish in Latin America, use Latin American keyboard layout.
  { "es-419",       "xkb:latam::spa" },

  // TODO(yusukes): Add {"sw", "xkb:us::eng"} once Swahili is removed from the
  // blacklist in src/ui/base/l10n/l10n_util_posix.cc.
};
const size_t kExtraLanguagesLength = arraysize(kExtraLanguages);

InputMethodUtil::InputMethodUtil() {
  ReloadInternalMaps();

  // Initialize a map from English string to Chrome string resource ID as well.
  for (size_t i = 0; i < kEnglishToResourceIdArraySize; ++i) {
    const EnglishToResouceId& map_entry = kEnglishToResourceIdArray[i];
    const bool result = english_to_resource_id_.insert(std::make_pair(
        map_entry.english_string_from_ibus, map_entry.resource_id)).second;
    DCHECK(result) << "Duplicated string is found: "
                   << map_entry.english_string_from_ibus;
  }
}

InputMethodUtil::~InputMethodUtil() {
}

bool InputMethodUtil::TranslateStringInternal(
    const std::string& english_string, string16 *out_string) const {
  DCHECK(out_string);
  HashType::const_iterator iter = english_to_resource_id_.find(english_string);
  if (iter == english_to_resource_id_.end()) {
    // TODO(yusukes): Write Autotest which checks if all display names and all
    // property names for supported input methods are listed in the resource
    // ID array (crosbug.com/4572).
    LOG(ERROR) << "Resource ID is not found for: " << english_string;
    return false;
  }

  *out_string = l10n_util::GetStringUTF16(iter->second);
  return true;
}

string16 InputMethodUtil::TranslateString(
    const std::string& english_string) const {
  string16 localized_string;
  if (TranslateStringInternal(english_string, &localized_string)) {
    return localized_string;
  }
  return UTF8ToUTF16(english_string);
}

bool InputMethodUtil::StringIsSupported(
    const std::string& english_string) const {
  string16 localized_string;
  return TranslateStringInternal(english_string, &localized_string);
}

std::string InputMethodUtil::NormalizeLanguageCode(
    const std::string& language_code) const {
  // Some ibus engines return locale codes like "zh_CN" as language codes.
  // Normalize these to like "zh-CN".
  if (language_code.size() >= 5 && language_code[2] == '_') {
    std::string copied_language_code = language_code;
    copied_language_code[2] = '-';
    // Downcase the language code part.
    for (size_t i = 0; i < 2; ++i) {
      copied_language_code[i] = base::ToLowerASCII(copied_language_code[i]);
    }
    // Upcase the country code part.
    for (size_t i = 3; i < copied_language_code.size(); ++i) {
      copied_language_code[i] = base::ToUpperASCII(copied_language_code[i]);
    }
    return copied_language_code;
  }
  // We only handle three-letter codes from here.
  if (language_code.size() != 3) {
    return language_code;
  }

  // Convert special language codes. See comments at kIso639VariantMapping.
  std::string copied_language_code = language_code;
  for (size_t i = 0; i < arraysize(kIso639VariantMapping); ++i) {
    if (language_code == kIso639VariantMapping[i][0]) {
      copied_language_code = kIso639VariantMapping[i][1];
    }
  }
  // Convert the three-letter code to two letter-code.
  UErrorCode error = U_ZERO_ERROR;
  char two_letter_code[ULOC_LANG_CAPACITY];
  uloc_getLanguage(copied_language_code.c_str(),
                   two_letter_code, sizeof(two_letter_code), &error);
  if (U_FAILURE(error)) {
    return language_code;
  }
  return two_letter_code;
}

// static
bool InputMethodUtil::IsKeyboardLayout(const std::string& input_method_id) {
  const bool kCaseInsensitive = false;
  return StartsWithASCII(input_method_id, "xkb:", kCaseInsensitive);
}

std::string InputMethodUtil::GetLanguageCodeFromDescriptor(
    const InputMethodDescriptor& descriptor) const {
  // Handle some Chinese input methods as zh-CN/zh-TW, rather than zh.
  // TODO: we should fix this issue in engines rather than here.
  if (descriptor.language_code() == "zh") {
    if (descriptor.id() == "pinyin" || descriptor.id() == "pinyin-dv") {
      return "zh-CN";
    } else if (descriptor.id() == "mozc-chewing" ||
               descriptor.id() == "m17n:zh:cangjie" ||
               descriptor.id() == "m17n:zh:quick") {
      return "zh-TW";
    }
    LOG(ERROR) << "Unhandled Chinese engine: " << descriptor.id();
  }

  std::string language_code = NormalizeLanguageCode(descriptor.language_code());

  // Add country codes to language codes of some XKB input methods to make
  // these compatible with Chrome's application locale codes like "en-US".
  // TODO(satorux): Maybe we need to handle "es" for "es-419".
  // TODO: We should not rely on the format of the engine name. Should we add
  //       |country_code| in InputMethodDescriptor?
  if (IsKeyboardLayout(descriptor.id()) &&
      (language_code == "en" ||
       language_code == "zh" ||
       language_code == "pt")) {
    std::vector<std::string> portions;
    base::SplitString(descriptor.id(), ':', &portions);
    if (portions.size() >= 2 && !portions[1].empty()) {
      language_code.append("-");
      language_code.append(StringToUpperASCII(portions[1]));
    }
  }
  return language_code;
}

std::string InputMethodUtil::GetLanguageCodeFromInputMethodId(
    const std::string& input_method_id) const {
  // The code should be compatible with one of codes used for UI languages,
  // defined in app/l10_util.cc.
  const char kDefaultLanguageCode[] = "en-US";
  std::map<std::string, std::string>::const_iterator iter
      = id_to_language_code_.find(input_method_id);
  return (iter == id_to_language_code_.end()) ?
      // Returning |kDefaultLanguageCode| here is not for Chrome OS but for
      // Ubuntu where the ibus-xkb-layouts engine could be missing.
      kDefaultLanguageCode : iter->second;
}

std::string InputMethodUtil::GetKeyboardLayoutName(
    const std::string& input_method_id) const {
  InputMethodIdToDescriptorMap::const_iterator iter
      = id_to_descriptor_.find(input_method_id);
  return (iter == id_to_descriptor_.end()) ?
      "" : iter->second.keyboard_layout();
}

std::string InputMethodUtil::GetInputMethodDisplayNameFromId(
    const std::string& input_method_id) const {
  string16 display_name;
  if (TranslateStringInternal(input_method_id, &display_name)) {
    return UTF16ToUTF8(display_name);
  }
  // Return an empty string if the display name is not found.
  return "";
}

const InputMethodDescriptor* InputMethodUtil::GetInputMethodDescriptorFromId(
    const std::string& input_method_id) const {
  InputMethodIdToDescriptorMap::const_iterator iter
      = id_to_descriptor_.find(input_method_id);
  return (iter == id_to_descriptor_.end()) ? NULL : &(iter->second);
}

const InputMethodDescriptor* InputMethodUtil::GetInputMethodDescriptorFromXkbId(
    const std::string& xkb_id) const {
  InputMethodIdToDescriptorMap::const_iterator iter
      = xkb_id_to_descriptor_.find(xkb_id);
  return (iter == xkb_id_to_descriptor_.end()) ? NULL : &(iter->second);
}

// static
string16 InputMethodUtil::GetLanguageDisplayNameFromCode(
    const std::string& language_code) {
  if (!g_browser_process) {
    return string16();
  }
  return l10n_util::GetDisplayNameForLocale(
      language_code, g_browser_process->GetApplicationLocale(), true);
}

// static
string16 InputMethodUtil::GetLanguageNativeDisplayNameFromCode(
    const std::string& language_code) {
  return l10n_util::GetDisplayNameForLocale(language_code, language_code, true);
}

// static
void InputMethodUtil::SortLanguageCodesByNames(
    std::vector<std::string>* language_codes) {
  if (!g_browser_process) {
    return;
  }
  // We should build collator outside of the comparator. We cannot have
  // scoped_ptr<> in the comparator for a subtle STL reason.
  UErrorCode error = U_ZERO_ERROR;
  icu::Locale locale(g_browser_process->GetApplicationLocale().c_str());
  scoped_ptr<icu::Collator> collator(
      icu::Collator::createInstance(locale, error));
  if (U_FAILURE(error)) {
    collator.reset();
  }
  std::sort(language_codes->begin(), language_codes->end(),
            CompareLanguageCodesByLanguageName(collator.get()));
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
    LOG(ERROR) << "Unknown language code: " << normalized_language_code;
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
        descriptor->keyboard_layout() ==
        current_input_method.keyboard_layout()) {
      most_popular_id = input_method_id;
      break;
    }
  }
  // Add the most popular input method ID, if it's different from the
  // current input method.
  if (most_popular_id != current_input_method.id() &&
      // TODO(yusukes): Remove this hack when we remove the "english-m" IME.
      most_popular_id != "english-m") {
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
      LOG(ERROR) << "Unknown input method ID: " << input_method_ids[i];
      continue;
    }
    const std::string language_code =
        GetLanguageCodeFromDescriptor(*input_method);
    // Add it if it's not already present.
    if (std::count(out_language_codes->begin(), out_language_codes->end(),
                   language_code) == 0) {
      out_language_codes->push_back(language_code);
    }
  }
}

void InputMethodUtil::EnableInputMethods(
    const std::string& language_code,
    InputMethodType type,
    const std::string& initial_input_method_id) {
  std::vector<std::string> candidates;
  // Add input methods associated with the language.
  GetInputMethodIdsFromLanguageCode(language_code, type, &candidates);
  // Add the hardware keyboard as well. We should always add this so users
  // can use the hardware keyboard on the login screen and the screen locker.
  candidates.push_back(GetHardwareInputMethodId());

  std::vector<std::string> input_method_ids;
  // First, add the initial input method ID, if it's requested, to
  // input_method_ids, so it appears first on the list of active input
  // methods at the input language status menu.
  if (!initial_input_method_id.empty()) {
    input_method_ids.push_back(initial_input_method_id);
  }

  // Add candidates to input_method_ids, while skipping duplicates.
  for (size_t i = 0; i < candidates.size(); ++i) {
    const std::string& candidate = candidates[i];
    // Not efficient, but should be fine, as the two vectors are very
    // short (2-5 items).
    if (std::count(input_method_ids.begin(), input_method_ids.end(),
                   candidate) == 0) {
      input_method_ids.push_back(candidate);
    }
  }

  // Update ibus-daemon setting. Here, we don't save the input method list
  // in the user's preferences.
  ImeConfigValue value;
  value.type = ImeConfigValue::kValueTypeStringList;
  value.string_list_value = input_method_ids;
  InputMethodManager* manager = InputMethodManager::GetInstance();
  manager->SetImeConfig(language_prefs::kGeneralSectionName,
                        language_prefs::kPreloadEnginesConfigName, value);

  // Finaly, change to the initial input method, as needed.
  if (!initial_input_method_id.empty()) {
    manager->ChangeInputMethod(initial_input_method_id);
  }
}

std::string InputMethodUtil::GetHardwareInputMethodId() const {
  if (!(g_browser_process && g_browser_process->local_state())) {
    // This shouldn't happen but just in case.
    VLOG(1) << "Local state is not yet ready";
    return InputMethodDescriptor::GetFallbackInputMethodDescriptor().id();
  }

  PrefService* local_state = g_browser_process->local_state();
  if (!local_state->FindPreference(prefs::kHardwareKeyboardLayout)) {
    // This could happen in unittests. We register the preference in
    // BrowserMain::InitializeLocalState and that method is not called during
    // unittests.
    LOG(ERROR) << prefs::kHardwareKeyboardLayout << " is not registered";
    return InputMethodDescriptor::GetFallbackInputMethodDescriptor().id();
  }

  const std::string input_method_id =
      local_state->GetString(prefs::kHardwareKeyboardLayout);
  if (input_method_id.empty()) {
    // This is totally fine if it's empty. The hardware keyboard layout is
    // not stored if startup_manifest.json (OEM customization data) is not
    // present (ex. Cr48 doen't have that file).
    return InputMethodDescriptor::GetFallbackInputMethodDescriptor().id();
  }
  return input_method_id;
}

void InputMethodUtil::ReloadInternalMaps() {
  scoped_ptr<InputMethodDescriptors> supported_input_methods(
      InputMethodDescriptor::GetSupportedInputMethods());
  if (supported_input_methods->size() <= 1) {
    LOG(ERROR) << "GetSupportedInputMethods returned a fallback ID";
    // TODO(yusukes): Handle this error in nicer way.
  }

  // Clear the existing maps.
  language_code_to_ids_.clear();
  id_to_language_code_.clear();
  id_to_descriptor_.clear();
  xkb_id_to_descriptor_.clear();

  for (size_t i = 0; i < supported_input_methods->size(); ++i) {
    const InputMethodDescriptor& input_method =
        supported_input_methods->at(i);
    const std::string language_code =
        GetLanguageCodeFromDescriptor(input_method);
    language_code_to_ids_.insert(
        std::make_pair(language_code, input_method.id()));
    // Remember the pairs.
    id_to_language_code_.insert(
        std::make_pair(input_method.id(), language_code));
    id_to_descriptor_.insert(
        std::make_pair(input_method.id(), input_method));
    if (IsKeyboardLayout(input_method.id())) {
      xkb_id_to_descriptor_.insert(
          std::make_pair(input_method.keyboard_layout(), input_method));
    }
  }

  // Go through the languages listed in kExtraLanguages.
  for (size_t i = 0; i < kExtraLanguagesLength; ++i) {
    const char* language_code = kExtraLanguages[i].language_code;
    const char* input_method_id = kExtraLanguages[i].input_method_id;
    InputMethodIdToDescriptorMap::const_iterator iter =
        id_to_descriptor_.find(input_method_id);
    // If the associated input method descriptor is found, add the language
    // code and the input method.
    if (iter != id_to_descriptor_.end()) {
      const InputMethodDescriptor& input_method = iter->second;
      language_code_to_ids_.insert(
          std::make_pair(language_code, input_method.id()));
    }
  }
}

void InputMethodUtil::OnLocaleChanged() {
  ReloadInternalMaps();
}

}  // namespace input_method
}  // namespace chromeos
