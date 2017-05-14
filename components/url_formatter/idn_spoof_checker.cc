// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_formatter/idn_spoof_checker.h"

#include "base/numerics/safe_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_local_storage.h"
#include "third_party/icu/source/common/unicode/schriter.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/i18n/unicode/regex.h"
#include "third_party/icu/source/i18n/unicode/uspoof.h"

namespace url_formatter {

namespace {
base::ThreadLocalStorage::StaticSlot tls_index = TLS_INITIALIZER;

void OnThreadTermination(void* regex_matcher) {
  delete reinterpret_cast<icu::RegexMatcher*>(regex_matcher);
}

}  // namespace

IDNSpoofChecker::IDNSpoofChecker() {
  UErrorCode status = U_ZERO_ERROR;
  checker_ = uspoof_open(&status);
  if (U_FAILURE(status)) {
    checker_ = nullptr;
    return;
  }

  // At this point, USpoofChecker has all the checks enabled except
  // for USPOOF_CHAR_LIMIT (USPOOF_{RESTRICTION_LEVEL, INVISIBLE,
  // MIXED_SCRIPT_CONFUSABLE, WHOLE_SCRIPT_CONFUSABLE, MIXED_NUMBERS, ANY_CASE})
  // This default configuration is adjusted below as necessary.

  // Set the restriction level to moderate. It allows mixing Latin with another
  // script (+ COMMON and INHERITED). Except for Chinese(Han + Bopomofo),
  // Japanese(Hiragana + Katakana + Han), and Korean(Hangul + Han), only one
  // script other than Common and Inherited can be mixed with Latin. Cyrillic
  // and Greek are not allowed to mix with Latin.
  // See http://www.unicode.org/reports/tr39/#Restriction_Level_Detection
  uspoof_setRestrictionLevel(checker_, USPOOF_MODERATELY_RESTRICTIVE);

  // Sets allowed characters in IDN labels and turns on USPOOF_CHAR_LIMIT.
  SetAllowedUnicodeSet(&status);

  // Enable the return of auxillary (non-error) information.
  // We used to disable WHOLE_SCRIPT_CONFUSABLE check explicitly, but as of
  // ICU 58.1, WSC is a no-op in a single string check API.
  int32_t checks = uspoof_getChecks(checker_, &status) | USPOOF_AUX_INFO;
  uspoof_setChecks(checker_, checks, &status);

  // Four characters handled differently by IDNA 2003 and IDNA 2008. UTS46
  // transitional processing treats them as IDNA 2003 does; maps U+00DF and
  // U+03C2 and drops U+200[CD].
  deviation_characters_ = icu::UnicodeSet(
      UNICODE_STRING_SIMPLE("[\\u00df\\u03c2\\u200c\\u200d]"), status);
  deviation_characters_.freeze();

  // Latin letters outside ASCII. 'Script_Extensions=Latin' is not necessary
  // because additional characters pulled in with scx=Latn are not included in
  // the allowed set.
  non_ascii_latin_letters_ =
      icu::UnicodeSet(UNICODE_STRING_SIMPLE("[[:Latin:] - [a-zA-Z]]"), status);
  non_ascii_latin_letters_.freeze();

  // These letters are parts of |dangerous_patterns_|.
  kana_letters_exceptions_ = icu::UnicodeSet(
      UNICODE_STRING_SIMPLE("[\\u3078-\\u307a\\u30d8-\\u30da\\u30fb-\\u30fe]"),
      status);
  kana_letters_exceptions_.freeze();

  // These Cyrillic letters look like Latin. A domain label entirely made of
  // these letters is blocked as a simplified whole-script-spoofable.
  cyrillic_letters_latin_alike_ =
      icu::UnicodeSet(icu::UnicodeString("[асԁеһіјӏорԛѕԝхуъЬҽпгѵѡ]"), status);
  cyrillic_letters_latin_alike_.freeze();

  cyrillic_letters_ =
      icu::UnicodeSet(UNICODE_STRING_SIMPLE("[[:Cyrl:]]"), status);
  cyrillic_letters_.freeze();

  DCHECK(U_SUCCESS(status));
}

IDNSpoofChecker::~IDNSpoofChecker() {
  uspoof_close(checker_);
}

bool IDNSpoofChecker::SafeToDisplayAsUnicode(base::StringPiece16 label,
                                             bool is_tld_ascii) {
  UErrorCode status = U_ZERO_ERROR;
  int32_t result =
      uspoof_check(checker_, label.data(),
                   base::checked_cast<int32_t>(label.size()), NULL, &status);
  // If uspoof_check fails (due to library failure), or if any of the checks
  // fail, treat the IDN as unsafe.
  if (U_FAILURE(status) || (result & USPOOF_ALL_CHECKS))
    return false;

  icu::UnicodeString label_string(FALSE, label.data(),
                                  base::checked_cast<int32_t>(label.size()));

  // A punycode label with 'xn--' prefix is not subject to the URL
  // canonicalization and is stored as it is in GURL. If it encodes a deviation
  // character (UTS 46; e.g. U+00DF/sharp-s), it should be still shown in
  // punycode instead of Unicode. Without this check, xn--fu-hia for
  // 'fu<sharp-s>' would be converted to 'fu<sharp-s>' for display because
  // "UTS 46 section 4 Processing step 4" applies validity criteria for
  // non-transitional processing (i.e. do not map deviation characters) to any
  // punycode labels regardless of whether transitional or non-transitional is
  // chosen. On the other hand, 'fu<sharp-s>' typed or copy and pasted
  // as Unicode would be canonicalized to 'fuss' by GURL and is displayed as
  // such. See http://crbug.com/595263 .
  if (deviation_characters_.containsSome(label_string))
    return false;

  // If there's no script mixing, the input is regarded as safe without any
  // extra check unless it contains Kana letter exceptions or it's made entirely
  // of Cyrillic letters that look like Latin letters. Note that the following
  // combinations of scripts are treated as a 'logical' single script.
  //  - Chinese: Han, Bopomofo, Common
  //  - Japanese: Han, Hiragana, Katakana, Common
  //  - Korean: Hangul, Han, Common
  result &= USPOOF_RESTRICTION_LEVEL_MASK;
  if (result == USPOOF_ASCII)
    return true;
  if (result == USPOOF_SINGLE_SCRIPT_RESTRICTIVE &&
      kana_letters_exceptions_.containsNone(label_string)) {
    // Check Cyrillic confusable only for ASCII TLDs.
    return !is_tld_ascii || !IsMadeOfLatinAlikeCyrillic(label_string);
  }

  // Additional checks for |label| with multiple scripts, one of which is Latin.
  // Disallow non-ASCII Latin letters to mix with a non-Latin script.
  if (non_ascii_latin_letters_.containsSome(label_string))
    return false;

  if (!tls_index.initialized())
    tls_index.Initialize(&OnThreadTermination);
  icu::RegexMatcher* dangerous_pattern =
      reinterpret_cast<icu::RegexMatcher*>(tls_index.Get());
  if (!dangerous_pattern) {
    // Disallow the katakana no, so, zo, or n, as they may be mistaken for
    // slashes when they're surrounded by non-Japanese scripts (i.e. scripts
    // other than Katakana, Hiragana or Han). If {no, so, zo, n} next to a
    // non-Japanese script on either side is disallowed, legitimate cases like
    // '{vitamin in Katakana}b6' are blocked. Note that trying to block those
    // characters when used alone as a label is futile because those cases
    // would not reach here.
    // Also disallow what used to be blocked by mixed-script-confusable (MSC)
    // detection. ICU 58 does not detect MSC any more for a single input string.
    // See http://bugs.icu-project.org/trac/ticket/12823 .
    // TODO(jshin): adjust the pattern once the above ICU bug is fixed.
    // - Disallow U+30FB (Katakana Middle Dot) and U+30FC (Hiragana-Katakana
    //   Prolonged Sound) used out-of-context.
    // - Dislallow U+30FD/E (Katakana iteration mark/voiced iteration mark)
    //   unless they're preceded by a Katakana.
    // - Disallow three Hiragana letters (U+307[8-A]) or Katakana letters
    //   (U+30D[8-A]) that look exactly like each other when they're used in a
    //   label otherwise entirely in Katakna or Hiragana.
    // - Disallow U+0585 (Armenian Small Letter Oh) and U+0581 (Armenian Small
    //   Letter Co) to be next to Latin.
    // - Disallow Latin 'o' and 'g' next to Armenian.
    // - Disalow mixing of Latin and Canadian Syllabary.
    dangerous_pattern = new icu::RegexMatcher(
        icu::UnicodeString(
            R"([^\p{scx=kana}\p{scx=hira}\p{scx=hani}])"
            R"([\u30ce\u30f3\u30bd\u30be])"
            R"([^\p{scx=kana}\p{scx=hira}\p{scx=hani}]|)"
            R"([^\p{scx=kana}\p{scx=hira}]\u30fc|^\u30fc|)"
            R"([^\p{scx=kana}][\u30fd\u30fe]|^[\u30fd\u30fe]|)"
            R"(^[\p{scx=kana}]+[\u3078-\u307a][\p{scx=kana}]+$|)"
            R"(^[\p{scx=hira}]+[\u30d8-\u30da][\p{scx=hira}]+$|)"
            R"([a-z]\u30fb|\u30fb[a-z]|)"
            R"(^[\u0585\u0581]+[a-z]|[a-z][\u0585\u0581]+$|)"
            R"([a-z][\u0585\u0581]+[a-z]|)"
            R"(^[og]+[\p{scx=armn}]|[\p{scx=armn}][og]+$|)"
            R"([\p{scx=armn}][og]+[\p{scx=armn}]|)"
            R"([\p{sc=cans}].*[a-z]|[a-z].*[\p{sc=cans}])",
            -1, US_INV),
        0, status);
    tls_index.Set(dangerous_pattern);
  }
  dangerous_pattern->reset(label_string);
  return !dangerous_pattern->find();
}

bool IDNSpoofChecker::IsMadeOfLatinAlikeCyrillic(
    const icu::UnicodeString& label) {
  // A shortcut of defining cyrillic_letters_latin_alike_ to include [0-9] and
  // [_-] and checking if the set contains all letters of |label_string|
  // would work in most cases, but not if a label has non-letters outside
  // ASCII.
  icu::UnicodeSet cyrillic_in_label;
  icu::StringCharacterIterator it(label);
  for (it.setToStart(); it.hasNext();) {
    const UChar32 c = it.next32PostInc();
    if (cyrillic_letters_.contains(c))
      cyrillic_in_label.add(c);
  }
  return !cyrillic_in_label.isEmpty() &&
         cyrillic_letters_latin_alike_.containsAll(cyrillic_in_label);
}

void IDNSpoofChecker::SetAllowedUnicodeSet(UErrorCode* status) {
  if (U_FAILURE(*status))
    return;

  // The recommended set is a set of characters for identifiers in a
  // security-sensitive environment taken from UTR 39
  // (http://unicode.org/reports/tr39/) and
  // http://www.unicode.org/Public/security/latest/xidmodifications.txt .
  // The inclusion set comes from "Candidate Characters for Inclusion
  // in idenfiers" of UTR 31 (http://www.unicode.org/reports/tr31). The list
  // may change over the time and will be updated whenever the version of ICU
  // used in Chromium is updated.
  const icu::UnicodeSet* recommended_set =
      uspoof_getRecommendedUnicodeSet(status);
  icu::UnicodeSet allowed_set;
  allowed_set.addAll(*recommended_set);
  const icu::UnicodeSet* inclusion_set = uspoof_getInclusionUnicodeSet(status);
  allowed_set.addAll(*inclusion_set);

// Five aspirational scripts are taken from UTR 31 Table 6 at
// http://www.unicode.org/reports/tr31/#Aspirational_Use_Scripts .
// Not all the characters of aspirational scripts are suitable for
// identifiers. Therefore, only characters belonging to
// [:Identifier_Type=Aspirational:] (listed in 'Status/Type=Aspirational'
// section at
// http://www.unicode.org/Public/security/latest/xidmodifications.txt) are
// are added to the allowed set. The list has to be updated when a new
// version of Unicode is released. The current version is 9.0.0 and ICU 60
// will have Unicode 10.0 data.
#if U_ICU_VERSION_MAJOR_NUM < 60
  const icu::UnicodeSet aspirational_scripts(
      icu::UnicodeString(
          // Unified Canadian Syllabics
          "[\\u1401-\\u166C\\u166F-\\u167F"
          // Mongolian
          "\\u1810-\\u1819\\u1820-\\u1877\\u1880-\\u18AA"
          // Unified Canadian Syllabics
          "\\u18B0-\\u18F5"
          // Tifinagh
          "\\u2D30-\\u2D67\\u2D7F"
          // Yi
          "\\uA000-\\uA48C"
          // Miao
          "\\U00016F00-\\U00016F44\\U00016F50-\\U00016F7E"
          "\\U00016F8F-\\U00016F9F]",
          -1, US_INV),
      *status);
  allowed_set.addAll(aspirational_scripts);
#else
#error "Update aspirational_scripts per Unicode 10.0"
#endif

  // U+0338 is included in the recommended set, while U+05F4 and U+2027 are in
  // the inclusion set. However, they are blacklisted as a part of Mozilla's
  // IDN blacklist (http://kb.mozillazine.org/Network.IDN.blacklist_chars).
  // U+2010 is in the inclusion set, but we drop it because it can be confused
  // with an ASCII U+002D (Hyphen-Minus).
  // U+0338 and U+2027 are dropped; the former can look like a slash when
  // rendered with a broken font, and the latter can be confused with U+30FB
  // (Katakana Middle Dot). U+05F4 (Hebrew Punctuation Gershayim) is kept,
  // even though it can look like a double quotation mark. Using it in Hebrew
  // should be safe. When used with a non-Hebrew script, it'd be filtered by
  // other checks in place.
  allowed_set.remove(0x338u);   // Combining Long Solidus Overlay
  allowed_set.remove(0x2010u);  // Hyphen
  allowed_set.remove(0x2027u);  // Hyphenation Point

#if defined(OS_MACOSX)
  // The following characters are reported as present in the default macOS
  // system UI font, but they render as blank. Remove them from the allowed
  // set to prevent spoofing.
  // Tibetan characters used for transliteration of ancient texts:
  allowed_set.remove(0x0F8Cu);
  allowed_set.remove(0x0F8Du);
  allowed_set.remove(0x0F8Eu);
  allowed_set.remove(0x0F8Fu);
#endif

  uspoof_setAllowedUnicodeSet(checker_, &allowed_set, status);
}

}  // namespace url_formatter
