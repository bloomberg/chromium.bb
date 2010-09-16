// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/l10n_util.h"

#include <cstdlib>

#include "app/app_paths.h"
#include "app/l10n_util_collator.h"
#include "app/resource_bundle.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/i18n/file_util_icu.h"
#include "base/i18n/rtl.h"
#include "base/path_service.h"
#include "base/scoped_ptr.h"
#include "base/string16.h"
#include "base/string_number_conversions.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "build/build_config.h"
#include "gfx/canvas.h"
#include "unicode/rbbi.h"

#if defined(OS_MACOSX)
#include "app/l10n_util_mac.h"
#endif

// TODO(playmobil): remove this undef once SkPostConfig.h is fixed.
// skia/include/corecg/SkPostConfig.h #defines strcasecmp() so we can't use
// base::strcasecmp() without #undefing it here.
#undef strcasecmp

namespace {

#if defined(OS_WIN)
static const FilePath::CharType kLocaleFileExtension[] = L".dll";
#elif defined(OS_POSIX)
static const FilePath::CharType kLocaleFileExtension[] = ".pak";
#endif

// Added to the end of strings that are too big in TrucateString.
static const wchar_t* const kElideString = L"\x2026";

static const char* const kAcceptLanguageList[] = {
  "af",     // Afrikaans
  "am",     // Amharic
  "ar",     // Arabic
  "az",     // Azerbaijani
  "be",     // Belarusian
  "bg",     // Bulgarian
  "bh",     // Bihari
  "bn",     // Bengali
  "br",     // Breton
  "bs",     // Bosnian
  "ca",     // Catalan
  "co",     // Corsican
  "cs",     // Czech
  "cy",     // Welsh
  "da",     // Danish
  "de",     // German
  "de-AT",  // German (Austria)
  "de-CH",  // German (Switzerland)
  "de-DE",  // German (Germany)
  "el",     // Greek
  "en",     // English
  "en-AU",  // English (Austrailia)
  "en-CA",  // English (Canada)
  "en-GB",  // English (UK)
  "en-NZ",  // English (New Zealand)
  "en-US",  // English (US)
  "en-ZA",  // English (South Africa)
  "eo",     // Esperanto
  // TODO(jungshik) : Do we want to list all es-Foo for Latin-American
  // Spanish speaking countries?
  "es",     // Spanish
  "et",     // Estonian
  "eu",     // Basque
  "fa",     // Persian
  "fi",     // Finnish
  "fil",    // Filipino
  "fo",     // Faroese
  "fr",     // French
  "fr-CA",  // French (Canada)
  "fr-CH",  // French (Switzerland)
  "fr-FR",  // French (France)
  "fy",     // Frisian
  "ga",     // Irish
  "gd",     // Scots Gaelic
  "gl",     // Galician
  "gn",     // Guarani
  "gu",     // Gujarati
  "ha",     // Hausa
  "haw",    // Hawaiian
  "he",     // Hebrew
  "hi",     // Hindi
  "hr",     // Croatian
  "hu",     // Hungarian
  "hy",     // Armenian
  "ia",     // Interlingua
  "id",     // Indonesian
  "is",     // Icelandic
  "it",     // Italian
  "it-CH",  // Italian (Switzerland)
  "it-IT",  // Italian (Italy)
  "ja",     // Japanese
  "jw",     // Javanese
  "ka",     // Georgian
  "kk",     // Kazakh
  "km",     // Cambodian
  "kn",     // Kannada
  "ko",     // Korean
  "ku",     // Kurdish
  "ky",     // Kyrgyz
  "la",     // Latin
  "ln",     // Lingala
  "lo",     // Laothian
  "lt",     // Lithuanian
  "lv",     // Latvian
  "mk",     // Macedonian
  "ml",     // Malayalam
  "mn",     // Mongolian
  "mo",     // Moldavian
  "mr",     // Marathi
  "ms",     // Malay
  "mt",     // Maltese
  "nb",     // Norwegian (Bokmal)
  "ne",     // Nepali
  "nl",     // Dutch
  "nn",     // Norwegian (Nynorsk)
  "no",     // Norwegian
  "oc",     // Occitan
  "om",     // Oromo
  "or",     // Oriya
  "pa",     // Punjabi
  "pl",     // Polish
  "ps",     // Pashto
  "pt",     // Portuguese
  "pt-BR",  // Portuguese (Brazil)
  "pt-PT",  // Portuguese (Portugal)
  "qu",     // Quechua
  "rm",     // Romansh
  "ro",     // Romanian
  "ru",     // Russian
  "sd",     // Sindhi
  "sh",     // Serbo-Croatian
  "si",     // Sinhalese
  "sk",     // Slovak
  "sl",     // Slovenian
  "sn",     // Shona
  "so",     // Somali
  "sq",     // Albanian
  "sr",     // Serbian
  "st",     // Sesotho
  "su",     // Sundanese
  "sv",     // Swedish
  "sw",     // Swahili
  "ta",     // Tamil
  "te",     // Telugu
  "tg",     // Tajik
  "th",     // Thai
  "ti",     // Tigrinya
  "tk",     // Turkmen
  "to",     // Tonga
  "tr",     // Turkish
  "tt",     // Tatar
  "tw",     // Twi
  "ug",     // Uighur
  "uk",     // Ukrainian
  "ur",     // Urdu
  "uz",     // Uzbek
  "vi",     // Vietnamese
  "xh",     // Xhosa
  "yi",     // Yiddish
  "yo",     // Yoruba
  "zh",     // Chinese
  "zh-CN",  // Chinese (Simplified)
  "zh-TW",  // Chinese (Traditional)
  "zu",     // Zulu
};

// Returns true if |locale_name| has an alias in the ICU data file.
bool IsDuplicateName(const std::string& locale_name) {
  static const char* const kDuplicateNames[] = {
    "en",
    "pt",
    "zh",
    "zh_hans_cn",
    "zh_hant_tw"
  };

  // Skip all 'es_RR'. Currently, we use 'es' for es-ES (Spanish in Spain).
  // 'es-419' (Spanish in Latin America) is not available in ICU so that it
  // has to be added manually in GetAvailableLocales().
  if (LowerCaseEqualsASCII(locale_name.substr(0, 3),  "es_"))
    return true;
  for (size_t i = 0; i < arraysize(kDuplicateNames); ++i) {
    if (base::strcasecmp(kDuplicateNames[i], locale_name.c_str()) == 0)
      return true;
  }
  return false;
}

bool IsLocaleNameTranslated(const char* locale,
                            const std::string& display_locale) {
  string16 display_name =
      l10n_util::GetDisplayNameForLocale(locale, display_locale, false);
  // Because ICU sets the error code to U_USING_DEFAULT_WARNING whether or not
  // uloc_getDisplayName returns the actual translation or the default
  // value (locale code), we have to rely on this hack to tell whether
  // the translation is available or not.  If ICU doesn't have a translated
  // name for this locale, GetDisplayNameForLocale will just return the
  // locale code.
  return !IsStringASCII(display_name) || UTF16ToASCII(display_name) != locale;
}

// We added 30+ minimally populated locales with only a few entries
// (exemplar character set, script, writing direction and its own
// lanaguage name). These locales have to be distinguished from the
// fully populated locales to which Chrome is localized.
bool IsLocalePartiallyPopulated(const std::string& locale_name) {
  // For partially populated locales, even the translation for "English"
  // is not available. A more robust/elegant way to check is to add a special
  // field (say, 'isPartial' to our version of ICU locale files) and
  // check its value, but this hack seems to work well.
  return !IsLocaleNameTranslated("en", locale_name);
}

bool IsLocaleAvailable(const std::string& locale,
                       const FilePath& locale_path) {
  // If locale has any illegal characters in it, we don't want to try to
  // load it because it may be pointing outside the locale data file directory.
  if (!file_util::IsFilenameLegal(ASCIIToUTF16(locale)))
    return false;

  // IsLocalePartiallyPopulated() can be called here for an early return w/o
  // checking the resource availability below. It'd help when Chrome is run
  // under a system locale Chrome is not localized to (e.g.Farsi on Linux),
  // but it'd slow down the start up time a little bit for locales Chrome is
  // localized to. So, we don't call it here.
  if (!l10n_util::IsLocaleSupportedByOS(locale))
    return false;

  FilePath test_path = locale_path;
  test_path =
    test_path.AppendASCII(locale).ReplaceExtension(kLocaleFileExtension);
  return file_util::PathExists(test_path);
}

bool CheckAndResolveLocale(const std::string& locale,
                           const FilePath& locale_path,
                           std::string* resolved_locale) {
  if (IsLocaleAvailable(locale, locale_path)) {
    *resolved_locale = locale;
    return true;
  }
  // If the locale matches language but not country, use that instead.
  // TODO(jungshik) : Nothing is done about languages that Chrome
  // does not support but available on Windows. We fall
  // back to en-US in GetApplicationLocale so that it's a not critical,
  // but we can do better.
  std::string::size_type hyphen_pos = locale.find('-');
  if (hyphen_pos != std::string::npos && hyphen_pos > 0) {
    std::string lang(locale, 0, hyphen_pos);
    std::string region(locale, hyphen_pos + 1);
    std::string tmp_locale(lang);
    // Map es-RR other than es-ES to es-419 (Chrome's Latin American
    // Spanish locale).
    if (LowerCaseEqualsASCII(lang, "es") && !LowerCaseEqualsASCII(region, "es"))
      tmp_locale.append("-419");
    else if (LowerCaseEqualsASCII(lang, "zh")) {
      // Map zh-HK and zh-MK to zh-TW. Otherwise, zh-FOO is mapped to zh-CN.
     if (LowerCaseEqualsASCII(region, "hk") ||
         LowerCaseEqualsASCII(region, "mk")) {
       tmp_locale.append("-TW");
     } else {
       tmp_locale.append("-CN");
     }
    }
    if (IsLocaleAvailable(tmp_locale, locale_path)) {
      resolved_locale->swap(tmp_locale);
      return true;
    }
  }

  // Google updater uses no, iw and en for our nb, he, and en-US.
  // We need to map them to our codes.
  struct {
    const char* source;
    const char* dest;
  } alias_map[] = {
      {"no", "nb"},
      {"tl", "fil"},
      {"iw", "he"},
      {"en", "en-US"},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(alias_map); ++i) {
    if (LowerCaseEqualsASCII(locale, alias_map[i].source)) {
      std::string tmp_locale(alias_map[i].dest);
      if (IsLocaleAvailable(tmp_locale, locale_path)) {
        resolved_locale->swap(tmp_locale);
        return true;
      }
    }
  }

  return false;
}

// Get the locale of the operating system.  The return value is of the form
// language[-country] (e.g., en-US) where the language is the 2 letter code from
// ISO-639.
std::string GetSystemLocale() {
  std::string language, region;
  base::i18n::GetLanguageAndRegionFromOS(&language, &region);
  std::string ret;
  if (!language.empty())
    ret.append(language);
  if (!region.empty()) {
    ret.append("-");
    ret.append(region);
  }
  return ret;
}

#if defined(OS_POSIX) && !defined(OS_MACOSX)
// Split and normalize the language list specified by LANGUAGE environment.
// LANGUAGE environment specifies a priority list of user prefered locales for
// application UI messages. Locales are separated by ':' character. The format
// of a locale is: language[_territory[.codeset]][@modifier]
//
// This function splits the language list and normalizes each locale into
// language[-territory] format, eg. fr, zh-CN, etc.
void SplitAndNormalizeLanguageList(const std::string& env_language,
                                   std::vector<std::string>* result) {
  std::vector<std::string> langs;
  SplitString(env_language, ':', &langs);
  std::vector<std::string>::iterator i = langs.begin();
  for (; i != langs.end(); ++i) {
    size_t end_pos = i->find_first_of(".@");
    // Erase encoding and modifier part.
    if (end_pos != std::string::npos)
      i->erase(end_pos);

    if (!i->empty()) {
      std::string locale;
      size_t sep = i->find_first_of("_-");
      if (sep != std::string::npos) {
        // language part is always in lower case.
        locale = StringToLowerASCII(i->substr(0, sep));
        locale.append("-");
        // territory part is always in upper case.
        locale.append(StringToUpperASCII(i->substr(sep + 1)));
      } else {
        locale = StringToLowerASCII(*i);
      }
      result->push_back(locale);
    }
  }
}
#endif

// On Linux, the text layout engine Pango determines paragraph directionality
// by looking at the first strongly-directional character in the text. This
// means text such as "Google Chrome foo bar..." will be layed out LTR even
// if "foo bar" is RTL. So this function prepends the necessary RLM in such
// cases.
void AdjustParagraphDirectionality(string16* paragraph) {
#if defined(OS_LINUX)
  if (base::i18n::IsRTL() &&
      base::i18n::StringContainsStrongRTLChars(*paragraph)) {
    paragraph->insert(0, 1, static_cast<char16>(base::i18n::kRightToLeftMark));
  }
#endif
}

}  // namespace

namespace l10n_util {

std::string GetApplicationLocale(const std::string& pref_locale) {
#if !defined(OS_MACOSX)
  FilePath locale_path;
  PathService::Get(app::DIR_LOCALES, &locale_path);
  std::string resolved_locale;
  std::vector<std::string> candidates;
  const std::string system_locale = GetSystemLocale();

  // We only use --lang and the app pref on Windows.  On Linux, we only
  // look at the LC_*/LANG environment variables.  We do, however, pass --lang
  // to renderer and plugin processes so they know what language the parent
  // process decided to use.
#if defined(OS_WIN)
  // First, try the preference value.
  if (!pref_locale.empty())
    candidates.push_back(pref_locale);

  // Next, try the system locale.
  candidates.push_back(system_locale);

#elif defined(OS_CHROMEOS)
  // On ChromeOS, use the application locale preference.
  if (!pref_locale.empty())
    candidates.push_back(pref_locale);

#elif defined(OS_POSIX)
  // On POSIX, we also check LANGUAGE environment variable, which is supported
  // by gettext to specify a priority list of prefered languages.
  const char* env_language = ::getenv("LANGUAGE");
  if (env_language)
    SplitAndNormalizeLanguageList(env_language, &candidates);

  // Only fallback to the system locale if LANGUAGE is not specified.
  // We emulate gettext's behavior here, which ignores LANG/LC_MESSAGES/LC_ALL
  // when LANGUAGE is specified. If no language specified in LANGUAGE is valid,
  // then just fallback to the locale based on LC_ALL/LANG.
  if (candidates.empty())
    candidates.push_back(system_locale);
#endif

  std::vector<std::string>::const_iterator i = candidates.begin();
  for (; i != candidates.end(); ++i) {
    if (CheckAndResolveLocale(*i, locale_path, &resolved_locale)) {
      base::i18n::SetICUDefaultLocale(resolved_locale);
      return resolved_locale;
    }
  }

  // Fallback on en-US.
  const std::string fallback_locale("en-US");
  if (IsLocaleAvailable(fallback_locale, locale_path)) {
    base::i18n::SetICUDefaultLocale(fallback_locale);
    return fallback_locale;
  }

  // No locale data file was found; we shouldn't get here.
  NOTREACHED();

  return std::string();

#else  // !defined(OS_MACOSX)

  // Use any override (Cocoa for the browser), otherwise use the preference
  // passed to the function.
  std::string app_locale = l10n_util::GetLocaleOverride();
  if (app_locale.empty())
    app_locale = pref_locale;

  // The above should handle all of the cases Chrome normally hits, but for some
  // unit tests, we need something to fall back too.
  if (app_locale.empty())
    app_locale = "en-US";

  // Windows/Linux call SetICUDefaultLocale after determining the actual locale
  // with CheckAndResolveLocal to make ICU APIs work in that locale.
  // Mac doesn't use a locale directory tree of resources (it uses Mac style
  // resources), so mirror the Windows/Linux behavior of calling
  // SetICUDefaultLocale.
  base::i18n::SetICUDefaultLocale(app_locale);
  return app_locale;
#endif  // !defined(OS_MACOSX)
}

string16 GetDisplayNameForLocale(const std::string& locale,
                                 const std::string& display_locale,
                                 bool is_for_ui) {
  std::string locale_code = locale;
  // Internally, we use the language code of zh-CN and zh-TW, but we want the
  // display names to be Chinese (Simplified) and Chinese (Traditional) instead
  // of Chinese (China) and Chinese (Taiwan).  To do that, we pass zh-Hans
  // and zh-Hant to ICU. Even with this mapping, we'd get
  // 'Chinese (Simplified Han)' and 'Chinese (Traditional Han)' in English and
  // even longer results in other languages. Arguably, they're better than
  // the current results : Chinese (China) / Chinese (Taiwan).
  // TODO(jungshik): Do one of the following:
  // 1. Special-case Chinese by getting the custom-translation for them
  // 2. Recycle IDS_ENCODING_{SIMP,TRAD}_CHINESE.
  // 3. Get translations for two directly from the ICU resouce bundle
  // because they're not accessible with other any API.
  // 4. Patch ICU to special-case zh-Hans/zh-Hant for us.
  // #1 and #2 wouldn't work if display_locale != current UI locale although
  // we can think of additional hack to work around the problem.
  // #3 can be potentially expensive.
  if (locale_code == "zh-CN")
    locale_code = "zh-Hans";
  else if (locale_code == "zh-TW")
    locale_code = "zh-Hant";

  UErrorCode error = U_ZERO_ERROR;
  const int buffer_size = 1024;

  string16 display_name;
  int actual_size = uloc_getDisplayName(locale_code.c_str(),
      display_locale.c_str(),
      WriteInto(&display_name, buffer_size + 1), buffer_size, &error);
  DCHECK(U_SUCCESS(error));
  display_name.resize(actual_size);
  // Add an RTL mark so parentheses are properly placed.
  if (is_for_ui && base::i18n::IsRTL())
    display_name.push_back(static_cast<char16>(base::i18n::kRightToLeftMark));
  return display_name;
}

std::wstring GetString(int message_id) {
  return UTF16ToWide(GetStringUTF16(message_id));
}

std::string GetStringUTF8(int message_id) {
  return UTF16ToUTF8(GetStringUTF16(message_id));
}

string16 GetStringUTF16(int message_id) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  string16 str = rb.GetLocalizedString(message_id);
  AdjustParagraphDirectionality(&str);

  return str;
}

static string16 GetStringF(int message_id,
                           const string16& a,
                           const string16& b,
                           const string16& c,
                           const string16& d,
                           std::vector<size_t>* offsets) {
  // TODO(tc): We could save a string copy if we got the raw string as
  // a StringPiece and were able to call ReplaceStringPlaceholders with
  // a StringPiece format string and string16 substitution strings.  In
  // practice, the strings should be relatively short.
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  const string16& format_string = rb.GetLocalizedString(message_id);
  std::vector<string16> subst;
  subst.push_back(a);
  subst.push_back(b);
  subst.push_back(c);
  subst.push_back(d);
  string16 formatted = ReplaceStringPlaceholders(format_string, subst,
                                                 offsets);
  AdjustParagraphDirectionality(&formatted);

  return formatted;
}

#if !defined(WCHAR_T_IS_UTF16)
std::wstring GetStringF(int message_id, const std::wstring& a) {
  return UTF16ToWide(GetStringF(message_id, WideToUTF16(a), string16(),
                                string16(), string16(), NULL));
}

std::wstring GetStringF(int message_id,
                        const std::wstring& a,
                        const std::wstring& b) {
  return UTF16ToWide(GetStringF(message_id, WideToUTF16(a), WideToUTF16(b),
                                string16(), string16(), NULL));
}

std::wstring GetStringF(int message_id,
                        const std::wstring& a,
                        const std::wstring& b,
                        const std::wstring& c) {
  return UTF16ToWide(GetStringF(message_id, WideToUTF16(a), WideToUTF16(b),
                                WideToUTF16(c), string16(), NULL));
}

std::wstring GetStringF(int message_id,
                        const std::wstring& a,
                        const std::wstring& b,
                        const std::wstring& c,
                        const std::wstring& d) {
  return UTF16ToWide(GetStringF(message_id, WideToUTF16(a), WideToUTF16(b),
                                WideToUTF16(c), WideToUTF16(d), NULL));
}
#endif

std::string GetStringFUTF8(int message_id,
                           const string16& a) {
  return UTF16ToUTF8(GetStringF(message_id, a, string16(), string16(),
                                string16(), NULL));
}

std::string GetStringFUTF8(int message_id,
                           const string16& a,
                           const string16& b) {
  return UTF16ToUTF8(GetStringF(message_id, a, b, string16(), string16(),
                                NULL));
}

std::string GetStringFUTF8(int message_id,
                           const string16& a,
                           const string16& b,
                           const string16& c) {
  return UTF16ToUTF8(GetStringF(message_id, a, b, c, string16(), NULL));
}

std::string GetStringFUTF8(int message_id,
                           const string16& a,
                           const string16& b,
                           const string16& c,
                           const string16& d) {
  return UTF16ToUTF8(GetStringF(message_id, a, b, c, d, NULL));
}

string16 GetStringFUTF16(int message_id,
                         const string16& a) {
  return GetStringF(message_id, a, string16(), string16(), string16(), NULL);
}

string16 GetStringFUTF16(int message_id,
                         const string16& a,
                         const string16& b) {
  return GetStringF(message_id, a, b, string16(), string16(), NULL);
}

string16 GetStringFUTF16(int message_id,
                         const string16& a,
                         const string16& b,
                         const string16& c) {
  return GetStringF(message_id, a, b, c, string16(), NULL);
}

string16 GetStringFUTF16(int message_id,
                         const string16& a,
                         const string16& b,
                         const string16& c,
                         const string16& d) {
  return GetStringF(message_id, a, b, c, d, NULL);
}

std::wstring GetStringF(int message_id, const std::wstring& a, size_t* offset) {
  DCHECK(offset);
  std::vector<size_t> offsets;
  string16 result = GetStringF(message_id, WideToUTF16(a), string16(),
                               string16(), string16(), &offsets);
  DCHECK(offsets.size() == 1);
  *offset = offsets[0];
  return UTF16ToWide(result);
}

std::wstring GetStringF(int message_id,
                        const std::wstring& a,
                        const std::wstring& b,
                        std::vector<size_t>* offsets) {
  return UTF16ToWide(GetStringF(message_id, WideToUTF16(a), WideToUTF16(b),
                                string16(), string16(), offsets));
}

string16 GetStringFUTF16(int message_id, const string16& a, size_t* offset) {
  DCHECK(offset);
  std::vector<size_t> offsets;
  string16 result = GetStringFUTF16(message_id, a, string16(), &offsets);
  DCHECK(offsets.size() == 1);
  *offset = offsets[0];
  return result;
}

string16 GetStringFUTF16(int message_id,
                        const string16& a,
                        const string16& b,
                        std::vector<size_t>* offsets) {
  return GetStringF(message_id, a, b, string16(), string16(), offsets);
}

std::wstring GetStringF(int message_id, int a) {
  return GetStringF(message_id, UTF8ToWide(base::IntToString(a)));
}

std::wstring GetStringF(int message_id, int64 a) {
  return GetStringF(message_id, UTF8ToWide(base::Int64ToString(a)));
}

std::wstring TruncateString(const std::wstring& string, size_t length) {
  if (string.size() <= length)
    // String fits, return it.
    return string;

  if (length == 0) {
    // No room for the ellide string, return an empty string.
    return std::wstring(L"");
  }
  size_t max = length - 1;

  if (max == 0) {
    // Just enough room for the elide string.
    return kElideString;
  }

#if defined(WCHAR_T_IS_UTF32)
  const string16 string_utf16 = WideToUTF16(string);
#else
  const std::wstring &string_utf16 = string;
#endif
  // Use a line iterator to find the first boundary.
  UErrorCode status = U_ZERO_ERROR;
  scoped_ptr<icu::RuleBasedBreakIterator> bi(
      static_cast<icu::RuleBasedBreakIterator*>(
          icu::RuleBasedBreakIterator::createLineInstance(
              icu::Locale::getDefault(), status)));
  if (U_FAILURE(status))
    return string.substr(0, max) + kElideString;
  bi->setText(string_utf16.c_str());
  int32_t index = bi->preceding(static_cast<int32_t>(max));
  if (index == icu::BreakIterator::DONE) {
    index = static_cast<int32_t>(max);
  } else {
    // Found a valid break (may be the beginning of the string). Now use
    // a character iterator to find the previous non-whitespace character.
    icu::StringCharacterIterator char_iterator(string_utf16.c_str());
    if (index == 0) {
      // No valid line breaks. Start at the end again. This ensures we break
      // on a valid character boundary.
      index = static_cast<int32_t>(max);
    }
    char_iterator.setIndex(index);
    while (char_iterator.hasPrevious()) {
      char_iterator.previous();
      if (!(u_isspace(char_iterator.current()) ||
            u_charType(char_iterator.current()) == U_CONTROL_CHAR ||
            u_charType(char_iterator.current()) == U_NON_SPACING_MARK)) {
        // Not a whitespace character. Advance the iterator so that we
        // include the current character in the truncated string.
        char_iterator.next();
        break;
      }
    }
    if (char_iterator.hasPrevious()) {
      // Found a valid break point.
      index = char_iterator.getIndex();
    } else {
      // String has leading whitespace, return the elide string.
      return kElideString;
    }
  }
  return string.substr(0, index) + kElideString;
}

string16 ToLower(const string16& string) {
  icu::UnicodeString lower_u_str(
      icu::UnicodeString(string.c_str()).toLower(icu::Locale::getDefault()));
  string16 result;
  lower_u_str.extract(0, lower_u_str.length(),
                      WriteInto(&result, lower_u_str.length() + 1));
  return result;
}

string16 ToUpper(const string16& string) {
  icu::UnicodeString upper_u_str(
      icu::UnicodeString(string.c_str()).toUpper(icu::Locale::getDefault()));
  string16 result;
  upper_u_str.extract(0, upper_u_str.length(),
                      WriteInto(&result, upper_u_str.length() + 1));
  return result;
}

// Compares the character data stored in two different string16 strings by
// specified Collator instance.
UCollationResult CompareString16WithCollator(const icu::Collator* collator,
                                             const string16& lhs,
                                             const string16& rhs) {
  DCHECK(collator);
  UErrorCode error = U_ZERO_ERROR;
  UCollationResult result = collator->compare(
      static_cast<const UChar*>(lhs.c_str()), static_cast<int>(lhs.length()),
      static_cast<const UChar*>(rhs.c_str()), static_cast<int>(rhs.length()),
      error);
  DCHECK(U_SUCCESS(error));
  return result;
}

// Compares the character data stored in two different std:wstring strings by
// specified Collator instance.
UCollationResult CompareStringWithCollator(const icu::Collator* collator,
                                           const std::wstring& lhs,
                                           const std::wstring& rhs) {
  DCHECK(collator);
  UCollationResult result;
#if defined(WCHAR_T_IS_UTF32)
  // Need to convert to UTF-16 to be compatible with UnicodeString's
  // constructor.
  string16 lhs_utf16 = WideToUTF16(lhs);
  string16 rhs_utf16 = WideToUTF16(rhs);

  result = CompareString16WithCollator(collator, lhs_utf16, rhs_utf16);
#else
  result = CompareString16WithCollator(collator, lhs, rhs);
#endif
  return result;
}

// Specialization of operator() method for std::wstring version.
template <>
bool StringComparator<std::wstring>::operator()(const std::wstring& lhs,
                                                const std::wstring& rhs) {
  // If we can not get collator instance for specified locale, just do simple
  // string compare.
  if (!collator_)
    return lhs < rhs;
  return CompareStringWithCollator(collator_, lhs, rhs) == UCOL_LESS;
};

#if !defined(WCHAR_T_IS_UTF16)
// Specialization of operator() method for string16 version.
template <>
bool StringComparator<string16>::operator()(const string16& lhs,
                                            const string16& rhs) {
  // If we can not get collator instance for specified locale, just do simple
  // string compare.
  if (!collator_)
    return lhs < rhs;
  return CompareString16WithCollator(collator_, lhs, rhs) == UCOL_LESS;
};
#endif  // !defined(WCHAR_T_IS_UTF16)

void SortStrings(const std::string& locale,
                 std::vector<std::wstring>* strings) {
  SortVectorWithStringKey(locale, strings, false);
}

void SortStrings16(const std::string& locale,
                   std::vector<string16>* strings) {
  SortVectorWithStringKey(locale, strings, false);
}

const std::vector<std::string>& GetAvailableLocales() {
  static std::vector<std::string> locales;
  if (locales.empty()) {
    int num_locales = uloc_countAvailable();
    for (int i = 0; i < num_locales; ++i) {
      std::string locale_name = uloc_getAvailable(i);
      // Filter out the names that have aliases.
      if (IsDuplicateName(locale_name))
        continue;
      // Filter out locales for which we have only partially populated data
      // and to which Chrome is not localized.
      if (IsLocalePartiallyPopulated(locale_name))
        continue;
      if (!IsLocaleSupportedByOS(locale_name))
        continue;
      // Normalize underscores to hyphens because that's what our locale files
      // use.
      std::replace(locale_name.begin(), locale_name.end(), '_', '-');

      // Map the Chinese locale names over to zh-CN and zh-TW.
      if (LowerCaseEqualsASCII(locale_name, "zh-hans")) {
        locale_name = "zh-CN";
      } else if (LowerCaseEqualsASCII(locale_name, "zh-hant")) {
        locale_name = "zh-TW";
      }
      locales.push_back(locale_name);
    }

    // Manually add 'es-419' to the list. See the comment in IsDuplicateName().
    locales.push_back("es-419");
  }
  return locales;
}

void GetAcceptLanguagesForLocale(const std::string& display_locale,
                                 std::vector<std::string>* locale_codes) {
  for (size_t i = 0; i < arraysize(kAcceptLanguageList); ++i) {
    if (!IsLocaleNameTranslated(kAcceptLanguageList[i], display_locale))
      // TODO(jungshik) : Put them at the of the list with language codes
      // enclosed by brackets instead of skipping.
        continue;
    locale_codes->push_back(kAcceptLanguageList[i]);
  }
}

}  // namespace l10n_util
