// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_util.h"

#include <errno.h>

#include <algorithm>
#include <iterator>
#include <map>
#include <set>

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#include <iphlpapi.h>
#include <winsock2.h>
#pragma comment(lib, "iphlpapi.lib")
#elif defined(OS_POSIX)
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#if !defined(OS_NACL)
#include <net/if.h>
#if !defined(OS_ANDROID)
#include <ifaddrs.h>
#endif  // !defined(OS_NACL)
#endif  // !defined(OS_ANDROID)
#endif  // defined(OS_POSIX)

#include "base/basictypes.h"
#include "base/i18n/time_formatting.h"
#include "base/json/string_escape.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_offset_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/sys_byteorder.h"
#include "base/time/time.h"
#include "base/values.h"
#include "grit/net_resources.h"
#include "url/gurl.h"
#include "url/url_canon.h"
#include "url/url_canon_ip.h"
#include "url/url_parse.h"
#include "net/base/dns_util.h"
#include "net/base/escape.h"
#include "net/base/net_module.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/http/http_content_disposition.h"
#include "third_party/icu/source/common/unicode/uidna.h"
#include "third_party/icu/source/common/unicode/uniset.h"
#include "third_party/icu/source/common/unicode/uscript.h"
#include "third_party/icu/source/common/unicode/uset.h"
#include "third_party/icu/source/i18n/unicode/datefmt.h"
#include "third_party/icu/source/i18n/unicode/regex.h"
#include "third_party/icu/source/i18n/unicode/ulocdata.h"

#if defined(OS_ANDROID)
#include "net/android/network_library.h"
#endif
#if defined(OS_WIN)
#include "net/base/winsock_init.h"
#endif

using base::Time;

namespace net {

namespace {

typedef std::vector<size_t> Offsets;

// The general list of blocked ports. Will be blocked unless a specific
// protocol overrides it. (Ex: ftp can use ports 20 and 21)
static const int kRestrictedPorts[] = {
  1,    // tcpmux
  7,    // echo
  9,    // discard
  11,   // systat
  13,   // daytime
  15,   // netstat
  17,   // qotd
  19,   // chargen
  20,   // ftp data
  21,   // ftp access
  22,   // ssh
  23,   // telnet
  25,   // smtp
  37,   // time
  42,   // name
  43,   // nicname
  53,   // domain
  77,   // priv-rjs
  79,   // finger
  87,   // ttylink
  95,   // supdup
  101,  // hostriame
  102,  // iso-tsap
  103,  // gppitnp
  104,  // acr-nema
  109,  // pop2
  110,  // pop3
  111,  // sunrpc
  113,  // auth
  115,  // sftp
  117,  // uucp-path
  119,  // nntp
  123,  // NTP
  135,  // loc-srv /epmap
  139,  // netbios
  143,  // imap2
  179,  // BGP
  389,  // ldap
  465,  // smtp+ssl
  512,  // print / exec
  513,  // login
  514,  // shell
  515,  // printer
  526,  // tempo
  530,  // courier
  531,  // chat
  532,  // netnews
  540,  // uucp
  556,  // remotefs
  563,  // nntp+ssl
  587,  // stmp?
  601,  // ??
  636,  // ldap+ssl
  993,  // ldap+ssl
  995,  // pop3+ssl
  2049, // nfs
  3659, // apple-sasl / PasswordServer
  4045, // lockd
  6000, // X11
  6665, // Alternate IRC [Apple addition]
  6666, // Alternate IRC [Apple addition]
  6667, // Standard IRC [Apple addition]
  6668, // Alternate IRC [Apple addition]
  6669, // Alternate IRC [Apple addition]
  0xFFFF, // Used to block all invalid port numbers (see
          // third_party/WebKit/Source/platform/weborigin/KURL.cpp,
          // KURL::port())
};

// FTP overrides the following restricted ports.
static const int kAllowedFtpPorts[] = {
  21,   // ftp data
  22,   // ssh
};

// Does some simple normalization of scripts so we can allow certain scripts
// to exist together.
// TODO(brettw) bug 880223: we should allow some other languages to be
// oombined such as Chinese and Latin. We will probably need a more
// complicated system of language pairs to have more fine-grained control.
UScriptCode NormalizeScript(UScriptCode code) {
  switch (code) {
    case USCRIPT_KATAKANA:
    case USCRIPT_HIRAGANA:
    case USCRIPT_KATAKANA_OR_HIRAGANA:
    case USCRIPT_HANGUL:  // This one is arguable.
      return USCRIPT_HAN;
    default:
      return code;
  }
}

bool IsIDNComponentInSingleScript(const base::char16* str, int str_len) {
  UScriptCode first_script = USCRIPT_INVALID_CODE;
  bool is_first = true;

  int i = 0;
  while (i < str_len) {
    unsigned code_point;
    U16_NEXT(str, i, str_len, code_point);

    UErrorCode err = U_ZERO_ERROR;
    UScriptCode cur_script = uscript_getScript(code_point, &err);
    if (err != U_ZERO_ERROR)
      return false;  // Report mixed on error.
    cur_script = NormalizeScript(cur_script);

    // TODO(brettw) We may have to check for USCRIPT_INHERENT as well.
    if (is_first && cur_script != USCRIPT_COMMON) {
      first_script = cur_script;
      is_first = false;
    } else {
      if (cur_script != USCRIPT_COMMON && cur_script != first_script)
        return false;
    }
  }
  return true;
}

// Check if the script of a language can be 'safely' mixed with
// Latin letters in the ASCII range.
bool IsCompatibleWithASCIILetters(const std::string& lang) {
  // For now, just list Chinese, Japanese and Korean (positive list).
  // An alternative is negative-listing (languages using Greek and
  // Cyrillic letters), but it can be more dangerous.
  return !lang.substr(0, 2).compare("zh") ||
         !lang.substr(0, 2).compare("ja") ||
         !lang.substr(0, 2).compare("ko");
}

typedef std::map<std::string, icu::UnicodeSet*> LangToExemplarSetMap;

class LangToExemplarSet {
 public:
  static LangToExemplarSet* GetInstance() {
    return Singleton<LangToExemplarSet>::get();
  }

 private:
  LangToExemplarSetMap map;
  LangToExemplarSet() { }
  ~LangToExemplarSet() {
    STLDeleteContainerPairSecondPointers(map.begin(), map.end());
  }

  friend class Singleton<LangToExemplarSet>;
  friend struct DefaultSingletonTraits<LangToExemplarSet>;
  friend bool GetExemplarSetForLang(const std::string&, icu::UnicodeSet**);
  friend void SetExemplarSetForLang(const std::string&, icu::UnicodeSet*);

  DISALLOW_COPY_AND_ASSIGN(LangToExemplarSet);
};

bool GetExemplarSetForLang(const std::string& lang,
                           icu::UnicodeSet** lang_set) {
  const LangToExemplarSetMap& map = LangToExemplarSet::GetInstance()->map;
  LangToExemplarSetMap::const_iterator pos = map.find(lang);
  if (pos != map.end()) {
    *lang_set = pos->second;
    return true;
  }
  return false;
}

void SetExemplarSetForLang(const std::string& lang,
                           icu::UnicodeSet* lang_set) {
  LangToExemplarSetMap& map = LangToExemplarSet::GetInstance()->map;
  map.insert(std::make_pair(lang, lang_set));
}

static base::LazyInstance<base::Lock>::Leaky
    g_lang_set_lock = LAZY_INSTANCE_INITIALIZER;

// Returns true if all the characters in component_characters are used by
// the language |lang|.
bool IsComponentCoveredByLang(const icu::UnicodeSet& component_characters,
                              const std::string& lang) {
  CR_DEFINE_STATIC_LOCAL(
      const icu::UnicodeSet, kASCIILetters, ('a', 'z'));
  icu::UnicodeSet* lang_set = NULL;
  // We're called from both the UI thread and the history thread.
  {
    base::AutoLock lock(g_lang_set_lock.Get());
    if (!GetExemplarSetForLang(lang, &lang_set)) {
      UErrorCode status = U_ZERO_ERROR;
      ULocaleData* uld = ulocdata_open(lang.c_str(), &status);
      // TODO(jungshik) Turn this check on when the ICU data file is
      // rebuilt with the minimal subset of locale data for languages
      // to which Chrome is not localized but which we offer in the list
      // of languages selectable for Accept-Languages. With the rebuilt ICU
      // data, ulocdata_open never should fall back to the default locale.
      // (issue 2078)
      // DCHECK(U_SUCCESS(status) && status != U_USING_DEFAULT_WARNING);
      if (U_SUCCESS(status) && status != U_USING_DEFAULT_WARNING) {
        lang_set = reinterpret_cast<icu::UnicodeSet *>(
            ulocdata_getExemplarSet(uld, NULL, 0,
                                    ULOCDATA_ES_STANDARD, &status));
        // If |lang| is compatible with ASCII Latin letters, add them.
        if (IsCompatibleWithASCIILetters(lang))
          lang_set->addAll(kASCIILetters);
      } else {
        lang_set = new icu::UnicodeSet(1, 0);
      }
      lang_set->freeze();
      SetExemplarSetForLang(lang, lang_set);
      ulocdata_close(uld);
    }
  }
  return !lang_set->isEmpty() && lang_set->containsAll(component_characters);
}

// Returns true if the given Unicode host component is safe to display to the
// user.
bool IsIDNComponentSafe(const base::char16* str,
                        int str_len,
                        const std::string& languages) {
  // Most common cases (non-IDN) do not reach here so that we don't
  // need a fast return path.
  // TODO(jungshik) : Check if there's any character inappropriate
  // (although allowed) for domain names.
  // See http://www.unicode.org/reports/tr39/#IDN_Security_Profiles and
  // http://www.unicode.org/reports/tr39/data/xidmodifications.txt
  // For now, we borrow the list from Mozilla and tweaked it slightly.
  // (e.g. Characters like U+00A0, U+3000, U+3002 are omitted because
  //  they're gonna be canonicalized to U+0020 and full stop before
  //  reaching here.)
  // The original list is available at
  // http://kb.mozillazine.org/Network.IDN.blacklist_chars and
  // at http://mxr.mozilla.org/seamonkey/source/modules/libpref/src/init/all.js#703

  UErrorCode status = U_ZERO_ERROR;
#ifdef U_WCHAR_IS_UTF16
  icu::UnicodeSet dangerous_characters(icu::UnicodeString(
      L"[[\\ \u00ad\u00bc\u00bd\u01c3\u0337\u0338"
      L"\u05c3\u05f4\u06d4\u0702\u115f\u1160][\u2000-\u200b]"
      L"[\u2024\u2027\u2028\u2029\u2039\u203a\u2044\u205f]"
      L"[\u2154-\u2156][\u2159-\u215b][\u215f\u2215\u23ae"
      L"\u29f6\u29f8\u2afb\u2afd][\u2ff0-\u2ffb][\u3014"
      L"\u3015\u3033\u3164\u321d\u321e\u33ae\u33af\u33c6\u33df\ufe14"
      L"\ufe15\ufe3f\ufe5d\ufe5e\ufeff\uff0e\uff06\uff61\uffa0\ufff9]"
      L"[\ufffa-\ufffd]]"), status);
  DCHECK(U_SUCCESS(status));
  icu::RegexMatcher dangerous_patterns(icu::UnicodeString(
      // Lone katakana no, so, or n
      L"[^\\p{Katakana}][\u30ce\u30f3\u30bd][^\\p{Katakana}]"
      // Repeating Japanese accent characters
      L"|[\u3099\u309a\u309b\u309c][\u3099\u309a\u309b\u309c]"),
      0, status);
#else
  icu::UnicodeSet dangerous_characters(icu::UnicodeString(
      "[[\\u0020\\u00ad\\u00bc\\u00bd\\u01c3\\u0337\\u0338"
      "\\u05c3\\u05f4\\u06d4\\u0702\\u115f\\u1160][\\u2000-\\u200b]"
      "[\\u2024\\u2027\\u2028\\u2029\\u2039\\u203a\\u2044\\u205f]"
      "[\\u2154-\\u2156][\\u2159-\\u215b][\\u215f\\u2215\\u23ae"
      "\\u29f6\\u29f8\\u2afb\\u2afd][\\u2ff0-\\u2ffb][\\u3014"
      "\\u3015\\u3033\\u3164\\u321d\\u321e\\u33ae\\u33af\\u33c6\\u33df\\ufe14"
      "\\ufe15\\ufe3f\\ufe5d\\ufe5e\\ufeff\\uff0e\\uff06\\uff61\\uffa0\\ufff9]"
      "[\\ufffa-\\ufffd]]", -1, US_INV), status);
  DCHECK(U_SUCCESS(status));
  icu::RegexMatcher dangerous_patterns(icu::UnicodeString(
      // Lone katakana no, so, or n
      "[^\\p{Katakana}][\\u30ce\\u30f3\u30bd][^\\p{Katakana}]"
      // Repeating Japanese accent characters
      "|[\\u3099\\u309a\\u309b\\u309c][\\u3099\\u309a\\u309b\\u309c]"),
      0, status);
#endif
  DCHECK(U_SUCCESS(status));
  icu::UnicodeSet component_characters;
  icu::UnicodeString component_string(str, str_len);
  component_characters.addAll(component_string);
  if (dangerous_characters.containsSome(component_characters))
    return false;

  DCHECK(U_SUCCESS(status));
  dangerous_patterns.reset(component_string);
  if (dangerous_patterns.find())
    return false;

  // If the language list is empty, the result is completely determined
  // by whether a component is a single script or not. This will block
  // even "safe" script mixing cases like <Chinese, Latin-ASCII> that are
  // allowed with |languages| (while it blocks Chinese + Latin letters with
  // an accent as should be the case), but we want to err on the safe side
  // when |languages| is empty.
  if (languages.empty())
    return IsIDNComponentInSingleScript(str, str_len);

  // |common_characters| is made up of  ASCII numbers, hyphen, plus and
  // underscore that are used across scripts and allowed in domain names.
  // (sync'd with characters allowed in url_canon_host with square
  // brackets excluded.) See kHostCharLookup[] array in url_canon_host.cc.
  icu::UnicodeSet common_characters(UNICODE_STRING_SIMPLE("[[0-9]\\-_+\\ ]"),
                                    status);
  DCHECK(U_SUCCESS(status));
  // Subtract common characters because they're always allowed so that
  // we just have to check if a language-specific set contains
  // the remainder.
  component_characters.removeAll(common_characters);

  base::StringTokenizer t(languages, ",");
  while (t.GetNext()) {
    if (IsComponentCoveredByLang(component_characters, t.token()))
      return true;
  }
  return false;
}

// A wrapper to use LazyInstance<>::Leaky with ICU's UIDNA, a C pointer to
// a UTS46/IDNA 2008 handling object opened with uidna_openUTS46().
//
// We use UTS46 with BiDiCheck to migrate from IDNA 2003 to IDNA 2008 with
// the backward compatibility in mind. What it does:
//
// 1. Use the up-to-date Unicode data.
// 2. Define a case folding/mapping with the up-to-date Unicode data as
//    in IDNA 2003.
// 3. Use transitional mechanism for 4 deviation characters (sharp-s,
//    final sigma, ZWJ and ZWNJ) for now.
// 4. Continue to allow symbols and punctuations.
// 5. Apply new BiDi check rules more permissive than the IDNA 2003 BiDI rules.
// 6. Do not apply STD3 rules
// 7. Do not allow unassigned code points.
//
// It also closely matches what IE 10 does except for the BiDi check (
// http://goo.gl/3XBhqw ).
// See http://http://unicode.org/reports/tr46/ and references therein
// for more details.
struct UIDNAWrapper {
  UIDNAWrapper() {
    UErrorCode err = U_ZERO_ERROR;
    // TODO(jungshik): Change options as different parties (browsers,
    // registrars, search engines) converge toward a consensus.
    value = uidna_openUTS46(UIDNA_CHECK_BIDI, &err);
    if (U_FAILURE(err))
      value = NULL;
  }

  UIDNA* value;
};

static base::LazyInstance<UIDNAWrapper>::Leaky
    g_uidna = LAZY_INSTANCE_INITIALIZER;

// Converts one component of a host (between dots) to IDN if safe. The result
// will be APPENDED to the given output string and will be the same as the input
// if it is not IDN or the IDN is unsafe to display.  Returns whether any
// conversion was performed.
bool IDNToUnicodeOneComponent(const base::char16* comp,
                              size_t comp_len,
                              const std::string& languages,
                              base::string16* out) {
  DCHECK(out);
  if (comp_len == 0)
    return false;

  // Only transform if the input can be an IDN component.
  static const base::char16 kIdnPrefix[] = {'x', 'n', '-', '-'};
  if ((comp_len > arraysize(kIdnPrefix)) &&
      !memcmp(comp, kIdnPrefix, arraysize(kIdnPrefix) * sizeof(base::char16))) {
    UIDNA* uidna = g_uidna.Get().value;
    DCHECK(uidna != NULL);
    size_t original_length = out->length();
    int output_length = 64;
    UIDNAInfo info = UIDNA_INFO_INITIALIZER;
    UErrorCode status;
    do {
      out->resize(original_length + output_length);
      status = U_ZERO_ERROR;
      // This returns the actual length required. If this is more than 64
      // code units, |status| will be U_BUFFER_OVERFLOW_ERROR and we'll try
      // the conversion again, but with a sufficiently large buffer.
      output_length = uidna_labelToUnicode(
          uidna, comp, static_cast<int32_t>(comp_len), &(*out)[original_length],
          output_length, &info, &status);
    } while ((status == U_BUFFER_OVERFLOW_ERROR && info.errors == 0));

    if (U_SUCCESS(status) && info.errors == 0) {
      // Converted successfully. Ensure that the converted component
      // can be safely displayed to the user.
      out->resize(original_length + output_length);
      if (IsIDNComponentSafe(out->data() + original_length, output_length,
                             languages))
        return true;
    }

    // Something went wrong. Revert to original string.
    out->resize(original_length);
  }

  // We get here with no IDN or on error, in which case we just append the
  // literal input.
  out->append(comp, comp_len);
  return false;
}

// Clamps the offsets in |offsets_for_adjustment| to the length of |str|.
void LimitOffsets(const base::string16& str, Offsets* offsets_for_adjustment) {
  if (offsets_for_adjustment) {
    std::for_each(offsets_for_adjustment->begin(),
                  offsets_for_adjustment->end(),
                  base::LimitOffset<base::string16>(str.length()));
  }
}

// TODO(brettw) bug 734373: check the scripts for each host component and
// don't un-IDN-ize if there is more than one. Alternatively, only IDN for
// scripts that the user has installed. For now, just put the entire
// path through IDN. Maybe this feature can be implemented in ICU itself?
//
// We may want to skip this step in the case of file URLs to allow unicode
// UNC hostnames regardless of encodings.
base::string16 IDNToUnicodeWithOffsets(const std::string& host,
                                       const std::string& languages,
                                       Offsets* offsets_for_adjustment) {
  // Convert the ASCII input to a base::string16 for ICU.
  base::string16 input16;
  input16.reserve(host.length());
  input16.insert(input16.end(), host.begin(), host.end());

  // Do each component of the host separately, since we enforce script matching
  // on a per-component basis.
  base::string16 out16;
  {
    base::OffsetAdjuster offset_adjuster(offsets_for_adjustment);
    for (size_t component_start = 0, component_end;
         component_start < input16.length();
         component_start = component_end + 1) {
      // Find the end of the component.
      component_end = input16.find('.', component_start);
      if (component_end == base::string16::npos)
        component_end = input16.length();  // For getting the last component.
      size_t component_length = component_end - component_start;
      size_t new_component_start = out16.length();
      bool converted_idn = false;
      if (component_end > component_start) {
        // Add the substring that we just found.
        converted_idn = IDNToUnicodeOneComponent(
            input16.data() + component_start, component_length, languages,
            &out16);
      }
      size_t new_component_length = out16.length() - new_component_start;

      if (converted_idn && offsets_for_adjustment) {
        offset_adjuster.Add(base::OffsetAdjuster::Adjustment(component_start,
            component_length, new_component_length));
      }

      // Need to add the dot we just found (if we found one).
      if (component_end < input16.length())
        out16.push_back('.');
    }
  }

  LimitOffsets(out16, offsets_for_adjustment);
  return out16;
}

// Called after transforming a component to set all affected elements in
// |offsets_for_adjustment| to the correct new values.  |original_offsets|
// represents the offsets before the transform; |original_component_begin| and
// |original_component_end| represent the pre-transform boundaries of the
// affected component.  |transformed_offsets| should be a vector created by
// adjusting |original_offsets| to be relative to the beginning of the component
// in question (via an OffsetAdjuster) and then transformed along with the
// component.  Note that any elements in this vector which didn't originally
// point into the component may contain arbitrary values and should be ignored.
// |transformed_component_begin| and |transformed_component_end| are the
// endpoints of the transformed component and are used in combination with the
// two offset vectors to calculate the resulting absolute offsets, which are
// stored in |offsets_for_adjustment|.
void AdjustForComponentTransform(const Offsets& original_offsets,
                                 size_t original_component_begin,
                                 size_t original_component_end,
                                 const Offsets& transformed_offsets,
                                 size_t transformed_component_begin,
                                 size_t transformed_component_end,
                                 Offsets* offsets_for_adjustment) {
  if (!offsets_for_adjustment)
    return;  // Nothing to do.

  for (size_t i = 0; i < original_offsets.size(); ++i) {
    size_t original_offset = original_offsets[i];
    if ((original_offset >= original_component_begin) &&
        (original_offset < original_component_end)) {
      // This offset originally pointed into the transformed component.
      // Adjust the transformed relative offset by the new beginning point of
      // the transformed component.
      size_t transformed_offset = transformed_offsets[i];
      (*offsets_for_adjustment)[i] =
          (transformed_offset == base::string16::npos) ?
              base::string16::npos :
              (transformed_offset + transformed_component_begin);
    } else if ((original_offset >= original_component_end) &&
               (original_offset != std::string::npos)) {
      // This offset pointed after the transformed component.  Adjust the
      // original absolute offset by the difference between the new and old
      // component lengths.
      (*offsets_for_adjustment)[i] =
          original_offset - original_component_end + transformed_component_end;
    }
  }
}

// If |component| is valid, its begin is incremented by |delta|.
void AdjustComponent(int delta, url_parse::Component* component) {
  if (!component->is_valid())
    return;

  DCHECK(delta >= 0 || component->begin >= -delta);
  component->begin += delta;
}

// Adjusts all the components of |parsed| by |delta|, except for the scheme.
void AdjustAllComponentsButScheme(int delta, url_parse::Parsed* parsed) {
  AdjustComponent(delta, &(parsed->username));
  AdjustComponent(delta, &(parsed->password));
  AdjustComponent(delta, &(parsed->host));
  AdjustComponent(delta, &(parsed->port));
  AdjustComponent(delta, &(parsed->path));
  AdjustComponent(delta, &(parsed->query));
  AdjustComponent(delta, &(parsed->ref));
}

// Helper for FormatUrlWithOffsets().
base::string16 FormatViewSourceUrl(const GURL& url,
                                   const Offsets& original_offsets,
                                   const std::string& languages,
                                   FormatUrlTypes format_types,
                                   UnescapeRule::Type unescape_rules,
                                   url_parse::Parsed* new_parsed,
                                   size_t* prefix_end,
                                   Offsets* offsets_for_adjustment) {
  DCHECK(new_parsed);
  const char kViewSource[] = "view-source:";
  const size_t kViewSourceLength = arraysize(kViewSource) - 1;

  // Format the underlying URL and adjust offsets.
  const std::string& url_str(url.possibly_invalid_spec());
  Offsets offsets_into_underlying_url(original_offsets);
  {
    base::OffsetAdjuster adjuster(&offsets_into_underlying_url);
    adjuster.Add(base::OffsetAdjuster::Adjustment(0, kViewSourceLength, 0));
  }
  base::string16 result(base::ASCIIToUTF16(kViewSource) +
      FormatUrlWithOffsets(GURL(url_str.substr(kViewSourceLength)), languages,
                           format_types, unescape_rules, new_parsed, prefix_end,
                           &offsets_into_underlying_url));
  AdjustForComponentTransform(original_offsets, kViewSourceLength,
                              url_str.length(), offsets_into_underlying_url,
                              kViewSourceLength, result.length(),
                              offsets_for_adjustment);
  LimitOffsets(result, offsets_for_adjustment);

  // Adjust positions of the parsed components.
  if (new_parsed->scheme.is_nonempty()) {
    // Assume "view-source:real-scheme" as a scheme.
    new_parsed->scheme.len += kViewSourceLength;
  } else {
    new_parsed->scheme.begin = 0;
    new_parsed->scheme.len = kViewSourceLength - 1;
  }
  AdjustAllComponentsButScheme(kViewSourceLength, new_parsed);

  if (prefix_end)
    *prefix_end += kViewSourceLength;

  return result;
}

class AppendComponentTransform {
 public:
  AppendComponentTransform() {}
  virtual ~AppendComponentTransform() {}

  virtual base::string16 Execute(const std::string& component_text,
                                 Offsets* offsets_into_component) const = 0;

  // NOTE: No DISALLOW_COPY_AND_ASSIGN here, since gcc < 4.3.0 requires an
  // accessible copy constructor in order to call AppendFormattedComponent()
  // with an inline temporary (see http://gcc.gnu.org/bugs/#cxx%5Frvalbind ).
};

class HostComponentTransform : public AppendComponentTransform {
 public:
  explicit HostComponentTransform(const std::string& languages)
      : languages_(languages) {
  }

 private:
  virtual base::string16 Execute(
      const std::string& component_text,
      Offsets* offsets_into_component) const OVERRIDE {
    return IDNToUnicodeWithOffsets(component_text, languages_,
                                   offsets_into_component);
  }

  const std::string& languages_;
};

class NonHostComponentTransform : public AppendComponentTransform {
 public:
  explicit NonHostComponentTransform(UnescapeRule::Type unescape_rules)
      : unescape_rules_(unescape_rules) {
  }

 private:
  virtual base::string16 Execute(
      const std::string& component_text,
      Offsets* offsets_into_component) const OVERRIDE {
    return (unescape_rules_ == UnescapeRule::NONE) ?
        base::UTF8ToUTF16AndAdjustOffsets(component_text,
                                          offsets_into_component) :
        UnescapeAndDecodeUTF8URLComponentWithOffsets(component_text,
            unescape_rules_, offsets_into_component);
  }

  const UnescapeRule::Type unescape_rules_;
};

// Transforms the portion of |spec| covered by |original_component| according to
// |transform|.  Appends the result to |output|.  If |output_component| is
// non-NULL, its start and length are set to the transformed component's new
// start and length.  For each element in |original_offsets| which is at least
// as large as original_component.begin, the corresponding element of
// |offsets_for_adjustment| is transformed appropriately.
void AppendFormattedComponent(const std::string& spec,
                              const url_parse::Component& original_component,
                              const Offsets& original_offsets,
                              const AppendComponentTransform& transform,
                              base::string16* output,
                              url_parse::Component* output_component,
                              Offsets* offsets_for_adjustment) {
  DCHECK(output);
  if (original_component.is_nonempty()) {
    size_t original_component_begin =
        static_cast<size_t>(original_component.begin);
    size_t output_component_begin = output->length();
    std::string component_str(spec, original_component_begin,
                              static_cast<size_t>(original_component.len));

    // Transform |component_str| and adjust the offsets accordingly.
    Offsets offsets_into_component(original_offsets);
    {
      base::OffsetAdjuster adjuster(&offsets_into_component);
      adjuster.Add(base::OffsetAdjuster::Adjustment(0, original_component_begin,
                                                    0));
    }
    output->append(transform.Execute(component_str, &offsets_into_component));
    AdjustForComponentTransform(original_offsets, original_component_begin,
                                static_cast<size_t>(original_component.end()),
                                offsets_into_component, output_component_begin,
                                output->length(), offsets_for_adjustment);

    // Set positions of the parsed component.
    if (output_component) {
      output_component->begin = static_cast<int>(output_component_begin);
      output_component->len =
          static_cast<int>(output->length() - output_component_begin);
    }
  } else if (output_component) {
    output_component->reset();
  }
}

bool IPNumberPrefixCheck(const IPAddressNumber& ip_number,
                         const unsigned char* ip_prefix,
                         size_t prefix_length_in_bits) {
  // Compare all the bytes that fall entirely within the prefix.
  int num_entire_bytes_in_prefix = prefix_length_in_bits / 8;
  for (int i = 0; i < num_entire_bytes_in_prefix; ++i) {
    if (ip_number[i] != ip_prefix[i])
      return false;
  }

  // In case the prefix was not a multiple of 8, there will be 1 byte
  // which is only partially masked.
  int remaining_bits = prefix_length_in_bits % 8;
  if (remaining_bits != 0) {
    unsigned char mask = 0xFF << (8 - remaining_bits);
    int i = num_entire_bytes_in_prefix;
    if ((ip_number[i] & mask) != (ip_prefix[i] & mask))
      return false;
  }
  return true;
}

}  // namespace

const FormatUrlType kFormatUrlOmitNothing                     = 0;
const FormatUrlType kFormatUrlOmitUsernamePassword            = 1 << 0;
const FormatUrlType kFormatUrlOmitHTTP                        = 1 << 1;
const FormatUrlType kFormatUrlOmitTrailingSlashOnBareHostname = 1 << 2;
const FormatUrlType kFormatUrlOmitAll = kFormatUrlOmitUsernamePassword |
    kFormatUrlOmitHTTP | kFormatUrlOmitTrailingSlashOnBareHostname;

static base::LazyInstance<std::multiset<int> >::Leaky
    g_explicitly_allowed_ports = LAZY_INSTANCE_INITIALIZER;

size_t GetCountOfExplicitlyAllowedPorts() {
  return g_explicitly_allowed_ports.Get().size();
}

std::string GetSpecificHeader(const std::string& headers,
                              const std::string& name) {
  // We want to grab the Value from the "Key: Value" pairs in the headers,
  // which should look like this (no leading spaces, \n-separated) (we format
  // them this way in url_request_inet.cc):
  //    HTTP/1.1 200 OK\n
  //    ETag: "6d0b8-947-24f35ec0"\n
  //    Content-Length: 2375\n
  //    Content-Type: text/html; charset=UTF-8\n
  //    Last-Modified: Sun, 03 Sep 2006 04:34:43 GMT\n
  if (headers.empty())
    return std::string();

  std::string match('\n' + name + ':');

  std::string::const_iterator begin =
      std::search(headers.begin(), headers.end(), match.begin(), match.end(),
             base::CaseInsensitiveCompareASCII<char>());

  if (begin == headers.end())
    return std::string();

  begin += match.length();

  std::string ret;
  base::TrimWhitespace(std::string(begin,
                                   std::find(begin, headers.end(), '\n')),
                       base::TRIM_ALL, &ret);
  return ret;
}

base::string16 IDNToUnicode(const std::string& host,
                            const std::string& languages) {
  return IDNToUnicodeWithOffsets(host, languages, NULL);
}

std::string CanonicalizeHost(const std::string& host,
                             url_canon::CanonHostInfo* host_info) {
  // Try to canonicalize the host.
  const url_parse::Component raw_host_component(
      0, static_cast<int>(host.length()));
  std::string canon_host;
  url_canon::StdStringCanonOutput canon_host_output(&canon_host);
  url_canon::CanonicalizeHostVerbose(host.c_str(), raw_host_component,
                                     &canon_host_output, host_info);

  if (host_info->out_host.is_nonempty() &&
      host_info->family != url_canon::CanonHostInfo::BROKEN) {
    // Success!  Assert that there's no extra garbage.
    canon_host_output.Complete();
    DCHECK_EQ(host_info->out_host.len, static_cast<int>(canon_host.length()));
  } else {
    // Empty host, or canonicalization failed.  We'll return empty.
    canon_host.clear();
  }

  return canon_host;
}

std::string GetDirectoryListingHeader(const base::string16& title) {
  static const base::StringPiece header(
      NetModule::GetResource(IDR_DIR_HEADER_HTML));
  // This can be null in unit tests.
  DLOG_IF(WARNING, header.empty()) <<
      "Missing resource: directory listing header";

  std::string result;
  if (!header.empty())
    result.assign(header.data(), header.size());

  result.append("<script>start(");
  base::EscapeJSONString(title, true, &result);
  result.append(");</script>\n");

  return result;
}

inline bool IsHostCharAlphanumeric(char c) {
  // We can just check lowercase because uppercase characters have already been
  // normalized.
  return ((c >= 'a') && (c <= 'z')) || ((c >= '0') && (c <= '9'));
}

bool IsCanonicalizedHostCompliant(const std::string& host,
                                  const std::string& desired_tld) {
  if (host.empty())
    return false;

  bool in_component = false;
  bool most_recent_component_started_alphanumeric = false;
  bool last_char_was_underscore = false;

  for (std::string::const_iterator i(host.begin()); i != host.end(); ++i) {
    const char c = *i;
    if (!in_component) {
      most_recent_component_started_alphanumeric = IsHostCharAlphanumeric(c);
      if (!most_recent_component_started_alphanumeric && (c != '-'))
        return false;
      in_component = true;
    } else {
      if (c == '.') {
        if (last_char_was_underscore)
          return false;
        in_component = false;
      } else if (IsHostCharAlphanumeric(c) || (c == '-')) {
        last_char_was_underscore = false;
      } else if (c == '_') {
        last_char_was_underscore = true;
      } else {
        return false;
      }
    }
  }

  return most_recent_component_started_alphanumeric ||
      (!desired_tld.empty() && IsHostCharAlphanumeric(desired_tld[0]));
}

std::string GetDirectoryListingEntry(const base::string16& name,
                                     const std::string& raw_bytes,
                                     bool is_dir,
                                     int64 size,
                                     Time modified) {
  std::string result;
  result.append("<script>addRow(");
  base::EscapeJSONString(name, true, &result);
  result.append(",");
  if (raw_bytes.empty()) {
    base::EscapeJSONString(EscapePath(base::UTF16ToUTF8(name)), true, &result);
  } else {
    base::EscapeJSONString(EscapePath(raw_bytes), true, &result);
  }
  if (is_dir) {
    result.append(",1,");
  } else {
    result.append(",0,");
  }

  // Negative size means unknown or not applicable (e.g. directory).
  base::string16 size_string;
  if (size >= 0)
    size_string = FormatBytesUnlocalized(size);
  base::EscapeJSONString(size_string, true, &result);

  result.append(",");

  base::string16 modified_str;
  // |modified| can be NULL in FTP listings.
  if (!modified.is_null()) {
    modified_str = base::TimeFormatShortDateAndTime(modified);
  }
  base::EscapeJSONString(modified_str, true, &result);

  result.append(");</script>\n");

  return result;
}

base::string16 StripWWW(const base::string16& text) {
  const base::string16 www(base::ASCIIToUTF16("www."));
  return StartsWith(text, www, true) ? text.substr(www.length()) : text;
}

base::string16 StripWWWFromHost(const GURL& url) {
  DCHECK(url.is_valid());
  return StripWWW(base::ASCIIToUTF16(url.host()));
}


bool IsPortAllowedByDefault(int port) {
  int array_size = arraysize(kRestrictedPorts);
  for (int i = 0; i < array_size; i++) {
    if (kRestrictedPorts[i] == port) {
      return false;
    }
  }
  return true;
}

bool IsPortAllowedByFtp(int port) {
  int array_size = arraysize(kAllowedFtpPorts);
  for (int i = 0; i < array_size; i++) {
    if (kAllowedFtpPorts[i] == port) {
        return true;
    }
  }
  // Port not explicitly allowed by FTP, so return the default restrictions.
  return IsPortAllowedByDefault(port);
}

bool IsPortAllowedByOverride(int port) {
  if (g_explicitly_allowed_ports.Get().empty())
    return false;

  return g_explicitly_allowed_ports.Get().count(port) > 0;
}

int SetNonBlocking(int fd) {
#if defined(OS_WIN)
  unsigned long no_block = 1;
  return ioctlsocket(fd, FIONBIO, &no_block);
#elif defined(OS_POSIX)
  int flags = fcntl(fd, F_GETFL, 0);
  if (-1 == flags)
    return flags;
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#endif
}

bool ParseHostAndPort(std::string::const_iterator host_and_port_begin,
                      std::string::const_iterator host_and_port_end,
                      std::string* host,
                      int* port) {
  if (host_and_port_begin >= host_and_port_end)
    return false;

  // When using url_parse, we use char*.
  const char* auth_begin = &(*host_and_port_begin);
  int auth_len = host_and_port_end - host_and_port_begin;

  url_parse::Component auth_component(0, auth_len);
  url_parse::Component username_component;
  url_parse::Component password_component;
  url_parse::Component hostname_component;
  url_parse::Component port_component;

  url_parse::ParseAuthority(auth_begin, auth_component, &username_component,
      &password_component, &hostname_component, &port_component);

  // There shouldn't be a username/password.
  if (username_component.is_valid() || password_component.is_valid())
    return false;

  if (!hostname_component.is_nonempty())
    return false;  // Failed parsing.

  int parsed_port_number = -1;
  if (port_component.is_nonempty()) {
    parsed_port_number = url_parse::ParsePort(auth_begin, port_component);

    // If parsing failed, port_number will be either PORT_INVALID or
    // PORT_UNSPECIFIED, both of which are negative.
    if (parsed_port_number < 0)
      return false;  // Failed parsing the port number.
  }

  if (port_component.len == 0)
    return false;  // Reject inputs like "foo:"

  // Pass results back to caller.
  host->assign(auth_begin + hostname_component.begin, hostname_component.len);
  *port = parsed_port_number;

  return true;  // Success.
}

bool ParseHostAndPort(const std::string& host_and_port,
                      std::string* host,
                      int* port) {
  return ParseHostAndPort(
      host_and_port.begin(), host_and_port.end(), host, port);
}

std::string GetHostAndPort(const GURL& url) {
  // For IPv6 literals, GURL::host() already includes the brackets so it is
  // safe to just append a colon.
  return base::StringPrintf("%s:%d", url.host().c_str(),
                            url.EffectiveIntPort());
}

std::string GetHostAndOptionalPort(const GURL& url) {
  // For IPv6 literals, GURL::host() already includes the brackets
  // so it is safe to just append a colon.
  if (url.has_port())
    return base::StringPrintf("%s:%s", url.host().c_str(), url.port().c_str());
  return url.host();
}

bool IsHostnameNonUnique(const std::string& hostname) {
  // CanonicalizeHost requires surrounding brackets to parse an IPv6 address.
  const std::string host_or_ip = hostname.find(':') != std::string::npos ?
      "[" + hostname + "]" : hostname;
  url_canon::CanonHostInfo host_info;
  std::string canonical_name = CanonicalizeHost(host_or_ip, &host_info);

  // If canonicalization fails, then the input is truly malformed. However,
  // to avoid mis-reporting bad inputs as "non-unique", treat them as unique.
  if (canonical_name.empty())
    return false;

  // If |hostname| is an IP address, check to see if it's in an IANA-reserved
  // range.
  if (host_info.IsIPAddress()) {
    IPAddressNumber host_addr;
    if (!ParseIPLiteralToNumber(hostname.substr(host_info.out_host.begin,
                                                host_info.out_host.len),
                                &host_addr)) {
      return false;
    }
    switch (host_info.family) {
      case url_canon::CanonHostInfo::IPV4:
      case url_canon::CanonHostInfo::IPV6:
        return IsIPAddressReserved(host_addr);
      case url_canon::CanonHostInfo::NEUTRAL:
      case url_canon::CanonHostInfo::BROKEN:
        return false;
    }
  }

  // Check for a registry controlled portion of |hostname|, ignoring private
  // registries, as they already chain to ICANN-administered registries,
  // and explicitly ignoring unknown registries.
  //
  // Note: This means that as new gTLDs are introduced on the Internet, they
  // will be treated as non-unique until the registry controlled domain list
  // is updated. However, because gTLDs are expected to provide significant
  // advance notice to deprecate older versions of this code, this an
  // acceptable tradeoff.
  return 0 == registry_controlled_domains::GetRegistryLength(
                  canonical_name,
                  registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES,
                  registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
}

// Don't compare IPv4 and IPv6 addresses (they have different range
// reservations). Keep separate reservation arrays for each IP type, and
// consolidate adjacent reserved ranges within a reservation array when
// possible.
// Sources for info:
// www.iana.org/assignments/ipv4-address-space/ipv4-address-space.xhtml
// www.iana.org/assignments/ipv6-address-space/ipv6-address-space.xhtml
// They're formatted here with the prefix as the last element. For example:
// 10.0.0.0/8 becomes 10,0,0,0,8 and fec0::/10 becomes 0xfe,0xc0,0,0,0...,10.
bool IsIPAddressReserved(const IPAddressNumber& host_addr) {
  static const unsigned char kReservedIPv4[][5] = {
      { 0,0,0,0,8 }, { 10,0,0,0,8 }, { 100,64,0,0,10 }, { 127,0,0,0,8 },
      { 169,254,0,0,16 }, { 172,16,0,0,12 }, { 192,0,2,0,24 },
      { 192,88,99,0,24 }, { 192,168,0,0,16 }, { 198,18,0,0,15 },
      { 198,51,100,0,24 }, { 203,0,113,0,24 }, { 224,0,0,0,3 }
  };
  static const unsigned char kReservedIPv6[][17] = {
      { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,8 },
      { 0x40,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2 },
      { 0x80,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2 },
      { 0xc0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3 },
      { 0xe0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4 },
      { 0xf0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5 },
      { 0xf8,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,6 },
      { 0xfc,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7 },
      { 0xfe,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,9 },
      { 0xfe,0x80,0,0,0,0,0,0,0,0,0,0,0,0,0,0,10 },
      { 0xfe,0xc0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,10 },
  };
  size_t array_size = 0;
  const unsigned char* array = NULL;
  switch (host_addr.size()) {
    case kIPv4AddressSize:
      array_size = arraysize(kReservedIPv4);
      array = kReservedIPv4[0];
      break;
    case kIPv6AddressSize:
      array_size = arraysize(kReservedIPv6);
      array = kReservedIPv6[0];
      break;
  }
  if (!array)
    return false;
  size_t width = host_addr.size() + 1;
  for (size_t i = 0; i < array_size; ++i, array += width) {
    if (IPNumberPrefixCheck(host_addr, array, array[width-1]))
      return true;
  }
  return false;
}

// Extracts the address and port portions of a sockaddr.
bool GetIPAddressFromSockAddr(const struct sockaddr* sock_addr,
                              socklen_t sock_addr_len,
                              const uint8** address,
                              size_t* address_len,
                              uint16* port) {
  if (sock_addr->sa_family == AF_INET) {
    if (sock_addr_len < static_cast<socklen_t>(sizeof(struct sockaddr_in)))
      return false;
    const struct sockaddr_in* addr =
        reinterpret_cast<const struct sockaddr_in*>(sock_addr);
    *address = reinterpret_cast<const uint8*>(&addr->sin_addr);
    *address_len = kIPv4AddressSize;
    if (port)
      *port = base::NetToHost16(addr->sin_port);
    return true;
  }

  if (sock_addr->sa_family == AF_INET6) {
    if (sock_addr_len < static_cast<socklen_t>(sizeof(struct sockaddr_in6)))
      return false;
    const struct sockaddr_in6* addr =
        reinterpret_cast<const struct sockaddr_in6*>(sock_addr);
    *address = reinterpret_cast<const unsigned char*>(&addr->sin6_addr);
    *address_len = kIPv6AddressSize;
    if (port)
      *port = base::NetToHost16(addr->sin6_port);
    return true;
  }

  return false;  // Unrecognized |sa_family|.
}

std::string IPAddressToString(const uint8* address,
                              size_t address_len) {
  std::string str;
  url_canon::StdStringCanonOutput output(&str);

  if (address_len == kIPv4AddressSize) {
    url_canon::AppendIPv4Address(address, &output);
  } else if (address_len == kIPv6AddressSize) {
    url_canon::AppendIPv6Address(address, &output);
  } else {
    CHECK(false) << "Invalid IP address with length: " << address_len;
  }

  output.Complete();
  return str;
}

std::string IPAddressToStringWithPort(const uint8* address,
                                      size_t address_len,
                                      uint16 port) {
  std::string address_str = IPAddressToString(address, address_len);

  if (address_len == kIPv6AddressSize) {
    // Need to bracket IPv6 addresses since they contain colons.
    return base::StringPrintf("[%s]:%d", address_str.c_str(), port);
  }
  return base::StringPrintf("%s:%d", address_str.c_str(), port);
}

std::string NetAddressToString(const struct sockaddr* sa,
                               socklen_t sock_addr_len) {
  const uint8* address;
  size_t address_len;
  if (!GetIPAddressFromSockAddr(sa, sock_addr_len, &address,
                                &address_len, NULL)) {
    NOTREACHED();
    return std::string();
  }
  return IPAddressToString(address, address_len);
}

std::string NetAddressToStringWithPort(const struct sockaddr* sa,
                                       socklen_t sock_addr_len) {
  const uint8* address;
  size_t address_len;
  uint16 port;
  if (!GetIPAddressFromSockAddr(sa, sock_addr_len, &address,
                                &address_len, &port)) {
    NOTREACHED();
    return std::string();
  }
  return IPAddressToStringWithPort(address, address_len, port);
}

std::string IPAddressToString(const IPAddressNumber& addr) {
  return IPAddressToString(&addr.front(), addr.size());
}

std::string IPAddressToStringWithPort(const IPAddressNumber& addr,
                                      uint16 port) {
  return IPAddressToStringWithPort(&addr.front(), addr.size(), port);
}

std::string IPAddressToPackedString(const IPAddressNumber& addr) {
  return std::string(reinterpret_cast<const char *>(&addr.front()),
                     addr.size());
}

std::string GetHostName() {
#if defined(OS_NACL)
  NOTIMPLEMENTED();
  return std::string();
#else  // defined(OS_NACL)
#if defined(OS_WIN)
  EnsureWinsockInit();
#endif

  // Host names are limited to 255 bytes.
  char buffer[256];
  int result = gethostname(buffer, sizeof(buffer));
  if (result != 0) {
    DVLOG(1) << "gethostname() failed with " << result;
    buffer[0] = '\0';
  }
  return std::string(buffer);
#endif  // !defined(OS_NACL)
}

void GetIdentityFromURL(const GURL& url,
                        base::string16* username,
                        base::string16* password) {
  UnescapeRule::Type flags =
      UnescapeRule::SPACES | UnescapeRule::URL_SPECIAL_CHARS;
  *username = UnescapeAndDecodeUTF8URLComponent(url.username(), flags, NULL);
  *password = UnescapeAndDecodeUTF8URLComponent(url.password(), flags, NULL);
}

std::string GetHostOrSpecFromURL(const GURL& url) {
  return url.has_host() ? TrimEndingDot(url.host()) : url.spec();
}

void AppendFormattedHost(const GURL& url,
                         const std::string& languages,
                         base::string16* output) {
  Offsets offsets;
  AppendFormattedComponent(url.possibly_invalid_spec(),
      url.parsed_for_possibly_invalid_spec().host, offsets,
      HostComponentTransform(languages), output, NULL, NULL);
}

base::string16 FormatUrlWithOffsets(
    const GURL& url,
    const std::string& languages,
    FormatUrlTypes format_types,
    UnescapeRule::Type unescape_rules,
    url_parse::Parsed* new_parsed,
    size_t* prefix_end,
    Offsets* offsets_for_adjustment) {
  url_parse::Parsed parsed_temp;
  if (!new_parsed)
    new_parsed = &parsed_temp;
  else
    *new_parsed = url_parse::Parsed();
  Offsets original_offsets;
  if (offsets_for_adjustment)
    original_offsets = *offsets_for_adjustment;

  // Special handling for view-source:.  Don't use content::kViewSourceScheme
  // because this library shouldn't depend on chrome.
  const char* const kViewSource = "view-source";
  // Reject "view-source:view-source:..." to avoid deep recursion.
  const char* const kViewSourceTwice = "view-source:view-source:";
  if (url.SchemeIs(kViewSource) &&
      !StartsWithASCII(url.possibly_invalid_spec(), kViewSourceTwice, false)) {
    return FormatViewSourceUrl(url, original_offsets, languages, format_types,
                               unescape_rules, new_parsed, prefix_end,
                               offsets_for_adjustment);
  }

  // We handle both valid and invalid URLs (this will give us the spec
  // regardless of validity).
  const std::string& spec = url.possibly_invalid_spec();
  const url_parse::Parsed& parsed = url.parsed_for_possibly_invalid_spec();

  // Scheme & separators.  These are ASCII.
  base::string16 url_string;
  url_string.insert(url_string.end(), spec.begin(),
      spec.begin() + parsed.CountCharactersBefore(url_parse::Parsed::USERNAME,
                                                  true));
  const char kHTTP[] = "http://";
  const char kFTP[] = "ftp.";
  // URLFixerUpper::FixupURL() treats "ftp.foo.com" as ftp://ftp.foo.com.  This
  // means that if we trim "http://" off a URL whose host starts with "ftp." and
  // the user inputs this into any field subject to fixup (which is basically
  // all input fields), the meaning would be changed.  (In fact, often the
  // formatted URL is directly pre-filled into an input field.)  For this reason
  // we avoid stripping "http://" in this case.
  bool omit_http = (format_types & kFormatUrlOmitHTTP) &&
      EqualsASCII(url_string, kHTTP) &&
      !StartsWithASCII(url.host(), kFTP, true);
  new_parsed->scheme = parsed.scheme;

  // Username & password.
  if ((format_types & kFormatUrlOmitUsernamePassword) != 0) {
    // Remove the username and password fields. We don't want to display those
    // to the user since they can be used for attacks,
    // e.g. "http://google.com:search@evil.ru/"
    new_parsed->username.reset();
    new_parsed->password.reset();
    // Update the offsets based on removed username and/or password.
    if (offsets_for_adjustment && !offsets_for_adjustment->empty() &&
        (parsed.username.is_nonempty() || parsed.password.is_nonempty())) {
      base::OffsetAdjuster offset_adjuster(offsets_for_adjustment);
      if (parsed.username.is_nonempty() && parsed.password.is_nonempty()) {
        // The seeming off-by-one and off-by-two in these first two lines are to
        // account for the ':' after the username and '@' after the password.
        offset_adjuster.Add(base::OffsetAdjuster::Adjustment(
            static_cast<size_t>(parsed.username.begin),
            static_cast<size_t>(parsed.username.len + parsed.password.len + 2),
            0));
      } else {
        const url_parse::Component* nonempty_component =
            parsed.username.is_nonempty() ? &parsed.username : &parsed.password;
        // The seeming off-by-one in below is to account for the '@' after the
        // username/password.
        offset_adjuster.Add(base::OffsetAdjuster::Adjustment(
            static_cast<size_t>(nonempty_component->begin),
            static_cast<size_t>(nonempty_component->len + 1), 0));
      }
    }
  } else {
    AppendFormattedComponent(spec, parsed.username, original_offsets,
        NonHostComponentTransform(unescape_rules), &url_string,
        &new_parsed->username, offsets_for_adjustment);
    if (parsed.password.is_valid())
      url_string.push_back(':');
    AppendFormattedComponent(spec, parsed.password, original_offsets,
        NonHostComponentTransform(unescape_rules), &url_string,
        &new_parsed->password, offsets_for_adjustment);
    if (parsed.username.is_valid() || parsed.password.is_valid())
      url_string.push_back('@');
  }
  if (prefix_end)
    *prefix_end = static_cast<size_t>(url_string.length());

  // Host.
  AppendFormattedComponent(spec, parsed.host, original_offsets,
      HostComponentTransform(languages), &url_string, &new_parsed->host,
      offsets_for_adjustment);

  // Port.
  if (parsed.port.is_nonempty()) {
    url_string.push_back(':');
    new_parsed->port.begin = url_string.length();
    url_string.insert(url_string.end(),
                      spec.begin() + parsed.port.begin,
                      spec.begin() + parsed.port.end());
    new_parsed->port.len = url_string.length() - new_parsed->port.begin;
  } else {
    new_parsed->port.reset();
  }

  // Path & query.  Both get the same general unescape & convert treatment.
  if (!(format_types & kFormatUrlOmitTrailingSlashOnBareHostname) ||
      !CanStripTrailingSlash(url)) {
    AppendFormattedComponent(spec, parsed.path, original_offsets,
        NonHostComponentTransform(unescape_rules), &url_string,
        &new_parsed->path, offsets_for_adjustment);
  } else {
    base::OffsetAdjuster offset_adjuster(offsets_for_adjustment);
    offset_adjuster.Add(base::OffsetAdjuster::Adjustment(
        url_string.length(), parsed.path.len, 0));
  }
  if (parsed.query.is_valid())
    url_string.push_back('?');
  AppendFormattedComponent(spec, parsed.query, original_offsets,
      NonHostComponentTransform(unescape_rules), &url_string,
      &new_parsed->query, offsets_for_adjustment);

  // Ref.  This is valid, unescaped UTF-8, so we can just convert.
  if (parsed.ref.is_valid())
    url_string.push_back('#');
  AppendFormattedComponent(spec, parsed.ref, original_offsets,
      NonHostComponentTransform(UnescapeRule::NONE), &url_string,
      &new_parsed->ref, offsets_for_adjustment);

  // If we need to strip out http do it after the fact. This way we don't need
  // to worry about how offset_for_adjustment is interpreted.
  if (omit_http && StartsWith(url_string, base::ASCIIToUTF16(kHTTP), true)) {
    const size_t kHTTPSize = arraysize(kHTTP) - 1;
    url_string = url_string.substr(kHTTPSize);
    if (offsets_for_adjustment && !offsets_for_adjustment->empty()) {
      base::OffsetAdjuster offset_adjuster(offsets_for_adjustment);
      offset_adjuster.Add(base::OffsetAdjuster::Adjustment(0, kHTTPSize, 0));
    }
    if (prefix_end)
      *prefix_end -= kHTTPSize;

    // Adjust new_parsed.
    DCHECK(new_parsed->scheme.is_valid());
    int delta = -(new_parsed->scheme.len + 3);  // +3 for ://.
    new_parsed->scheme.reset();
    AdjustAllComponentsButScheme(delta, new_parsed);
  }

  LimitOffsets(url_string, offsets_for_adjustment);
  return url_string;
}

base::string16 FormatUrl(const GURL& url,
                         const std::string& languages,
                         FormatUrlTypes format_types,
                         UnescapeRule::Type unescape_rules,
                         url_parse::Parsed* new_parsed,
                         size_t* prefix_end,
                         size_t* offset_for_adjustment) {
  Offsets offsets;
  if (offset_for_adjustment)
    offsets.push_back(*offset_for_adjustment);
  base::string16 result = FormatUrlWithOffsets(url, languages, format_types,
      unescape_rules, new_parsed, prefix_end, &offsets);
  if (offset_for_adjustment)
    *offset_for_adjustment = offsets[0];
  return result;
}

bool CanStripTrailingSlash(const GURL& url) {
  // Omit the path only for standard, non-file URLs with nothing but "/" after
  // the hostname.
  return url.IsStandard() && !url.SchemeIsFile() &&
      !url.SchemeIsFileSystem() && !url.has_query() && !url.has_ref()
      && url.path() == "/";
}

GURL SimplifyUrlForRequest(const GURL& url) {
  DCHECK(url.is_valid());
  GURL::Replacements replacements;
  replacements.ClearUsername();
  replacements.ClearPassword();
  replacements.ClearRef();
  return url.ReplaceComponents(replacements);
}

// Specifies a comma separated list of port numbers that should be accepted
// despite bans. If the string is invalid no allowed ports are stored.
void SetExplicitlyAllowedPorts(const std::string& allowed_ports) {
  if (allowed_ports.empty())
    return;

  std::multiset<int> ports;
  size_t last = 0;
  size_t size = allowed_ports.size();
  // The comma delimiter.
  const std::string::value_type kComma = ',';

  // Overflow is still possible for evil user inputs.
  for (size_t i = 0; i <= size; ++i) {
    // The string should be composed of only digits and commas.
    if (i != size && !IsAsciiDigit(allowed_ports[i]) &&
        (allowed_ports[i] != kComma))
      return;
    if (i == size || allowed_ports[i] == kComma) {
      if (i > last) {
        int port;
        base::StringToInt(base::StringPiece(allowed_ports.begin() + last,
                                            allowed_ports.begin() + i),
                          &port);
        ports.insert(port);
      }
      last = i + 1;
    }
  }
  g_explicitly_allowed_ports.Get() = ports;
}

ScopedPortException::ScopedPortException(int port) : port_(port) {
  g_explicitly_allowed_ports.Get().insert(port);
}

ScopedPortException::~ScopedPortException() {
  std::multiset<int>::iterator it =
      g_explicitly_allowed_ports.Get().find(port_);
  if (it != g_explicitly_allowed_ports.Get().end())
    g_explicitly_allowed_ports.Get().erase(it);
  else
    NOTREACHED();
}

bool HaveOnlyLoopbackAddresses() {
#if defined(OS_ANDROID)
  return android::HaveOnlyLoopbackAddresses();
#elif defined(OS_NACL)
  NOTIMPLEMENTED();
  return false;
#elif defined(OS_POSIX)
  struct ifaddrs* interface_addr = NULL;
  int rv = getifaddrs(&interface_addr);
  if (rv != 0) {
    DVLOG(1) << "getifaddrs() failed with errno = " << errno;
    return false;
  }

  bool result = true;
  for (struct ifaddrs* interface = interface_addr;
       interface != NULL;
       interface = interface->ifa_next) {
    if (!(IFF_UP & interface->ifa_flags))
      continue;
    if (IFF_LOOPBACK & interface->ifa_flags)
      continue;
    const struct sockaddr* addr = interface->ifa_addr;
    if (!addr)
      continue;
    if (addr->sa_family == AF_INET6) {
      // Safe cast since this is AF_INET6.
      const struct sockaddr_in6* addr_in6 =
          reinterpret_cast<const struct sockaddr_in6*>(addr);
      const struct in6_addr* sin6_addr = &addr_in6->sin6_addr;
      if (IN6_IS_ADDR_LOOPBACK(sin6_addr) || IN6_IS_ADDR_LINKLOCAL(sin6_addr))
        continue;
    }
    if (addr->sa_family != AF_INET6 && addr->sa_family != AF_INET)
      continue;

    result = false;
    break;
  }
  freeifaddrs(interface_addr);
  return result;
#elif defined(OS_WIN)
  // TODO(wtc): implement with the GetAdaptersAddresses function.
  NOTIMPLEMENTED();
  return false;
#else
  NOTIMPLEMENTED();
  return false;
#endif  // defined(various platforms)
}

AddressFamily GetAddressFamily(const IPAddressNumber& address) {
  switch (address.size()) {
    case kIPv4AddressSize:
      return ADDRESS_FAMILY_IPV4;
    case kIPv6AddressSize:
      return ADDRESS_FAMILY_IPV6;
    default:
      return ADDRESS_FAMILY_UNSPECIFIED;
  }
}

int ConvertAddressFamily(AddressFamily address_family) {
  switch (address_family) {
    case ADDRESS_FAMILY_UNSPECIFIED:
      return AF_UNSPEC;
    case ADDRESS_FAMILY_IPV4:
      return AF_INET;
    case ADDRESS_FAMILY_IPV6:
      return AF_INET6;
  }
  NOTREACHED();
  return AF_UNSPEC;
}

bool ParseIPLiteralToNumber(const std::string& ip_literal,
                            IPAddressNumber* ip_number) {
  // |ip_literal| could be either a IPv4 or an IPv6 literal. If it contains
  // a colon however, it must be an IPv6 address.
  if (ip_literal.find(':') != std::string::npos) {
    // GURL expects IPv6 hostnames to be surrounded with brackets.
    std::string host_brackets = "[" + ip_literal + "]";
    url_parse::Component host_comp(0, host_brackets.size());

    // Try parsing the hostname as an IPv6 literal.
    ip_number->resize(16);  // 128 bits.
    return url_canon::IPv6AddressToNumber(host_brackets.data(),
                                          host_comp,
                                          &(*ip_number)[0]);
  }

  // Otherwise the string is an IPv4 address.
  ip_number->resize(4);  // 32 bits.
  url_parse::Component host_comp(0, ip_literal.size());
  int num_components;
  url_canon::CanonHostInfo::Family family = url_canon::IPv4AddressToNumber(
      ip_literal.data(), host_comp, &(*ip_number)[0], &num_components);
  return family == url_canon::CanonHostInfo::IPV4;
}

namespace {

const unsigned char kIPv4MappedPrefix[] =
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF };
}

IPAddressNumber ConvertIPv4NumberToIPv6Number(
    const IPAddressNumber& ipv4_number) {
  DCHECK(ipv4_number.size() == 4);

  // IPv4-mapped addresses are formed by:
  // <80 bits of zeros>  + <16 bits of ones> + <32-bit IPv4 address>.
  IPAddressNumber ipv6_number;
  ipv6_number.reserve(16);
  ipv6_number.insert(ipv6_number.end(),
                     kIPv4MappedPrefix,
                     kIPv4MappedPrefix + arraysize(kIPv4MappedPrefix));
  ipv6_number.insert(ipv6_number.end(), ipv4_number.begin(), ipv4_number.end());
  return ipv6_number;
}

bool IsIPv4Mapped(const IPAddressNumber& address) {
  if (address.size() != kIPv6AddressSize)
    return false;
  return std::equal(address.begin(),
                    address.begin() + arraysize(kIPv4MappedPrefix),
                    kIPv4MappedPrefix);
}

IPAddressNumber ConvertIPv4MappedToIPv4(const IPAddressNumber& address) {
  DCHECK(IsIPv4Mapped(address));
  return IPAddressNumber(address.begin() + arraysize(kIPv4MappedPrefix),
                         address.end());
}

bool ParseCIDRBlock(const std::string& cidr_literal,
                    IPAddressNumber* ip_number,
                    size_t* prefix_length_in_bits) {
  // We expect CIDR notation to match one of these two templates:
  //   <IPv4-literal> "/" <number of bits>
  //   <IPv6-literal> "/" <number of bits>

  std::vector<std::string> parts;
  base::SplitString(cidr_literal, '/', &parts);
  if (parts.size() != 2)
    return false;

  // Parse the IP address.
  if (!ParseIPLiteralToNumber(parts[0], ip_number))
    return false;

  // Parse the prefix length.
  int number_of_bits = -1;
  if (!base::StringToInt(parts[1], &number_of_bits))
    return false;

  // Make sure the prefix length is in a valid range.
  if (number_of_bits < 0 ||
      number_of_bits > static_cast<int>(ip_number->size() * 8))
    return false;

  *prefix_length_in_bits = static_cast<size_t>(number_of_bits);
  return true;
}

bool IPNumberMatchesPrefix(const IPAddressNumber& ip_number,
                           const IPAddressNumber& ip_prefix,
                           size_t prefix_length_in_bits) {
  // Both the input IP address and the prefix IP address should be
  // either IPv4 or IPv6.
  DCHECK(ip_number.size() == 4 || ip_number.size() == 16);
  DCHECK(ip_prefix.size() == 4 || ip_prefix.size() == 16);

  DCHECK_LE(prefix_length_in_bits, ip_prefix.size() * 8);

  // In case we have an IPv6 / IPv4 mismatch, convert the IPv4 addresses to
  // IPv6 addresses in order to do the comparison.
  if (ip_number.size() != ip_prefix.size()) {
    if (ip_number.size() == 4) {
      return IPNumberMatchesPrefix(ConvertIPv4NumberToIPv6Number(ip_number),
                                   ip_prefix, prefix_length_in_bits);
    }
    return IPNumberMatchesPrefix(ip_number,
                                 ConvertIPv4NumberToIPv6Number(ip_prefix),
                                 96 + prefix_length_in_bits);
  }

  return IPNumberPrefixCheck(ip_number, &ip_prefix[0], prefix_length_in_bits);
}

const uint16* GetPortFieldFromSockaddr(const struct sockaddr* address,
                                       socklen_t address_len) {
  if (address->sa_family == AF_INET) {
    DCHECK_LE(sizeof(sockaddr_in), static_cast<size_t>(address_len));
    const struct sockaddr_in* sockaddr =
        reinterpret_cast<const struct sockaddr_in*>(address);
    return &sockaddr->sin_port;
  } else if (address->sa_family == AF_INET6) {
    DCHECK_LE(sizeof(sockaddr_in6), static_cast<size_t>(address_len));
    const struct sockaddr_in6* sockaddr =
        reinterpret_cast<const struct sockaddr_in6*>(address);
    return &sockaddr->sin6_port;
  } else {
    NOTREACHED();
    return NULL;
  }
}

int GetPortFromSockaddr(const struct sockaddr* address, socklen_t address_len) {
  const uint16* port_field = GetPortFieldFromSockaddr(address, address_len);
  if (!port_field)
    return -1;
  return base::NetToHost16(*port_field);
}

bool IsLocalhost(const std::string& host) {
  if (host == "localhost" ||
      host == "localhost.localdomain" ||
      host == "localhost6" ||
      host == "localhost6.localdomain6")
    return true;

  IPAddressNumber ip_number;
  if (ParseIPLiteralToNumber(host, &ip_number)) {
    size_t size = ip_number.size();
    switch (size) {
      case kIPv4AddressSize: {
        IPAddressNumber localhost_prefix;
        localhost_prefix.push_back(127);
        for (int i = 0; i < 3; ++i) {
          localhost_prefix.push_back(0);
        }
        return IPNumberMatchesPrefix(ip_number, localhost_prefix, 8);
      }

      case kIPv6AddressSize: {
        struct in6_addr sin6_addr;
        memcpy(&sin6_addr, &ip_number[0], kIPv6AddressSize);
        return !!IN6_IS_ADDR_LOOPBACK(&sin6_addr);
      }

      default:
        NOTREACHED();
    }
  }

  return false;
}

NetworkInterface::NetworkInterface()
    : type(NETWORK_INTERFACE_UNKNOWN),
      network_prefix(0) {
}

NetworkInterface::NetworkInterface(const std::string& name,
                                   const std::string& friendly_name,
                                   uint32 interface_index,
                                   NetworkInterfaceType type,
                                   const IPAddressNumber& address,
                                   size_t network_prefix)
    : name(name),
      friendly_name(friendly_name),
      interface_index(interface_index),
      type(type),
      address(address),
      network_prefix(network_prefix) {
}

NetworkInterface::~NetworkInterface() {
}

unsigned CommonPrefixLength(const IPAddressNumber& a1,
                            const IPAddressNumber& a2) {
  DCHECK_EQ(a1.size(), a2.size());
  for (size_t i = 0; i < a1.size(); ++i) {
    unsigned diff = a1[i] ^ a2[i];
    if (!diff)
      continue;
    for (unsigned j = 0; j < CHAR_BIT; ++j) {
      if (diff & (1 << (CHAR_BIT - 1)))
        return i * CHAR_BIT + j;
      diff <<= 1;
    }
    NOTREACHED();
  }
  return a1.size() * CHAR_BIT;
}

unsigned MaskPrefixLength(const IPAddressNumber& mask) {
  IPAddressNumber all_ones(mask.size(), 0xFF);
  return CommonPrefixLength(mask, all_ones);
}

}  // namespace net
