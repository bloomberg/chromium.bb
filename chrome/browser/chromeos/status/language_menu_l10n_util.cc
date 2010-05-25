// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/language_menu_l10n_util.h"

#include "base/hash_tables.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chrome_thread.h"
#include "grit/generated_resources.h"

namespace {

const struct EnglishToResouceId {
  const char* english_string_from_ibus;
  int resource_id;
} kEnglishToResourceIdArray[] = {
  // For ibus-mozc: third_party/ibus-mozc/files/src/unix/ibus/.
  { "Direct input", IDS_STATUSBAR_IME_JAPANESE_IME_STATUS_DIRECT_INPUT },
  { "Hiragana", IDS_STATUSBAR_IME_JAPANESE_IME_STATUS_HIRAGANA },
  { "Katakana", IDS_STATUSBAR_IME_JAPANESE_IME_STATUS_KATAKANA },
  { "Half width katakana",  // small k is not a typo.
    IDS_STATUSBAR_IME_JAPANESE_IME_STATUS_HALF_WIDTH_KATAKANA },
  { "Latin", IDS_STATUSBAR_IME_JAPANESE_IME_STATUS_LATIN },
  { "Wide Latin", IDS_STATUSBAR_IME_JAPANESE_IME_STATUS_WIDE_LATIN },

  // For ibus-hangul: third_party/ibus-hangul/files/po/.
  { "Enable/Disable Hanja mode", IDS_STATUSBAR_IME_KOREAN_HANJA_MODE },

  // For ibus-pinyin: third_party/ibus-pinyin/files/po/.
  { "Chinese", IDS_STATUSBAR_IME_CHINESE_PINYIN_TOGGLE_CHINESE_ENGLISH },
  { "Full/Half width",
    IDS_STATUSBAR_IME_CHINESE_PINYIN_TOGGLE_FULL_HALF },
  { "Full/Half width punctuation",
    IDS_STATUSBAR_IME_CHINESE_PINYIN_TOGGLE_FULL_HALF_PUNCTUATION },
  { "Simplfied/Traditional Chinese",
    IDS_STATUSBAR_IME_CHINESE_PINYIN_TOGGLE_S_T_CHINESE },

  // TODO(yusukes): Support ibus-chewing and ibus-table-* if needed.

  // For the "Languages and Input" dialog.
  { "kbd (m17n)", IDS_OPTIONS_SETTINGS_LANGUAGES_M17N_STANDARD_INPUT_METHOD },
  { "itrans (m17n)",  // also uses the "STANDARD_INPUT_METHOD" id.
    IDS_OPTIONS_SETTINGS_LANGUAGES_M17N_STANDARD_INPUT_METHOD },
  { "isiri (m17n)",
    IDS_OPTIONS_SETTINGS_LANGUAGES_M17N_PERSIAN_ISIRI_2901_INPUT_METHOD },
  { "kesmanee (m17n)",
    IDS_OPTIONS_SETTINGS_LANGUAGES_M17N_THAI_KESMANEE_INPUT_METHOD },
  { "tis820 (m17n)",
    IDS_OPTIONS_SETTINGS_LANGUAGES_M17N_THAI_TIS820_INPUT_METHOD },
  { "pattachote (m17n)",
    IDS_OPTIONS_SETTINGS_LANGUAGES_M17N_THAI_PATTACHOTE_INPUT_METHOD },
  { "tcvn (m17n)",
    IDS_OPTIONS_SETTINGS_LANGUAGES_M17N_VIETNAMESE_TCVN_INPUT_METHOD },
  { "telex (m17n)",
    IDS_OPTIONS_SETTINGS_LANGUAGES_M17N_VIETNAMESE_TELEX_INPUT_METHOD },
  { "viqr (m17n)",
    IDS_OPTIONS_SETTINGS_LANGUAGES_M17N_VIETNAMESE_VIQR_INPUT_METHOD },
  { "vni (m17n)",
    IDS_OPTIONS_SETTINGS_LANGUAGES_M17N_VIETNAMESE_VNI_INPUT_METHOD },
  { "latn-post (m17n)",
    IDS_OPTIONS_SETTINGS_LANGUAGES_M17N_LATIN_POST_INPUT_METHOD },
  { "latn-pre (m17n)",
    IDS_OPTIONS_SETTINGS_LANGUAGES_M17N_LATIN_PRE_INPUT_METHOD },
  { "Bopomofo", IDS_OPTIONS_SETTINGS_LANGUAGES_BOPOMOFO_INPUT_METHOD },
  { "Chewing", IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_INPUT_METHOD },
  { "Pinyin", IDS_OPTIONS_SETTINGS_LANGUAGES_PINYIN_INPUT_METHOD },
  { "Mozc (US keyboard layout)",
    IDS_OPTIONS_SETTINGS_LANGUAGES_JAPANESE_MOZC_US_INPUT_METHOD },
  { "Mozc (Japanese keyboard layout)",
    IDS_OPTIONS_SETTINGS_LANGUAGES_JAPANESE_MOZC_JP_INPUT_METHOD },
  { "Google Japanese Input (US keyboard layout)",
    IDS_OPTIONS_SETTINGS_LANGUAGES_JAPANESE_GOOGLE_US_INPUT_METHOD },
  { "Google Japanese Input (Japanese keyboard layout)",
    IDS_OPTIONS_SETTINGS_LANGUAGES_JAPANESE_GOOGLE_JP_INPUT_METHOD },
  { "Korean", IDS_OPTIONS_SETTINGS_LANGUAGES_KOREAN_INPUT_METHOD },
  // TODO(yusukes): Support input methods in the ibus-xkb-layouts engine.
};
const size_t kNumEntries = arraysize(kEnglishToResourceIdArray);

bool GetLocalizedString(
    const std::string& english_string, string16 *out_string) {
  DCHECK(out_string);
  typedef base::hash_map<std::string, int> HashType;
  static HashType* english_to_resource_id = NULL;

  // Initialize the map if needed.
  if (!english_to_resource_id) {
    // We don't free this map.
    english_to_resource_id = new HashType(kNumEntries);
    for (size_t i = 0; i < kNumEntries; ++i) {
      const bool result = english_to_resource_id->insert(
          std::make_pair(kEnglishToResourceIdArray[i].english_string_from_ibus,
                         kEnglishToResourceIdArray[i].resource_id)).second;
      DCHECK(result) << "Duplicated string is found: "
                     << kEnglishToResourceIdArray[i].english_string_from_ibus;
    }
  }

  HashType::const_iterator iter = english_to_resource_id->find(english_string);
  if (iter == english_to_resource_id->end()) {
    LOG(ERROR) << "Resouce ID is not found for: " << english_string;
    return false;
  }

  *out_string = l10n_util::GetStringUTF16(iter->second);
  return true;
};


}  // namespace

namespace chromeos {

std::wstring LanguageMenuL10nUtil::GetString(
    const std::string& english_string) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI))
      << "You should not call this function from non-UI threads";
  string16 localized_string;
  if (GetLocalizedString(english_string, &localized_string)) {
    return UTF16ToWide(localized_string);
  }
  return UTF8ToWide(english_string);
}

std::string LanguageMenuL10nUtil::GetStringUTF8(
    const std::string& english_string) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI))
      << "You should not call this function from non-UI threads";
  string16 localized_string;
  if (GetLocalizedString(english_string, &localized_string)) {
    return UTF16ToUTF8(localized_string);
  }
  return english_string;
}

string16 LanguageMenuL10nUtil::GetStringUTF16(
    const std::string& english_string) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI))
      << "You should not call this function from non-UI threads";
  string16 localized_string;
  if (GetLocalizedString(english_string, &localized_string)) {
    return localized_string;
  }
  return UTF8ToUTF16(english_string);
}

bool LanguageMenuL10nUtil::StringIsSupported(
    const std::string& english_string) {
  // Do not check the current thread since the function is supposed to be called
  // from unit tests.
  string16 localized_string;
  return GetLocalizedString(english_string, &localized_string);
}

}  // namespace chromeos
