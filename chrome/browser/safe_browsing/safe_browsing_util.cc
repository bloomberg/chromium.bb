// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/safe_browsing_util.h"

#include "base/base64.h"
#include "base/hmac.h"
#include "base/sha2.h"
#include "base/string_util.h"
#include "chrome/browser/google/google_util.h"
#include "googleurl/src/gurl.h"
#include "googleurl/src/url_util.h"
#include "net/base/escape.h"
#include "unicode/locid.h"

#if defined(OS_WIN)
#include "chrome/installer/util/browser_distribution.h"
#endif

static const int kSafeBrowsingMacDigestSize = 20;

// Continue to this URL after submitting the phishing report form.
// TODO(paulg): Change to a Chrome specific URL.
static const char kContinueUrlFormat[] =
  "http://www.google.com/tools/firefox/toolbar/FT2/intl/%s/submit_success.html";

static const char kReportParams[] = "?tpl=%s&continue=%s&url=%s";

// SBChunk ---------------------------------------------------------------------

SBChunk::SBChunk()
    : chunk_number(0),
      list_id(0),
      is_add(false) {
}

SBChunk::~SBChunk() {}

// SBChunkList -----------------------------------------------------------------

SBChunkList::SBChunkList() {}

SBChunkList::~SBChunkList() {
  clear();
}

void SBChunkList::clear() {
  for (std::vector<SBChunk>::iterator citer = chunks_.begin();
       citer != chunks_.end(); ++citer) {
    for (std::deque<SBChunkHost>::iterator hiter = citer->hosts.begin();
         hiter != citer->hosts.end(); ++hiter) {
      if (hiter->entry) {
        hiter->entry->Destroy();
        hiter->entry = NULL;
      }
    }
  }
  chunks_.clear();
}

// SBListChunkRanges -----------------------------------------------------------

SBListChunkRanges::SBListChunkRanges(const std::string& n) : name(n) {}

// SBChunkDelete ---------------------------------------------------------------

SBChunkDelete::SBChunkDelete() : is_sub_del(false) {}

SBChunkDelete::~SBChunkDelete() {}

// SBEntry ---------------------------------------------------------------------

// static
SBEntry* SBEntry::Create(Type type, int prefix_count) {
  int size = Size(type, prefix_count);
  SBEntry *rv = static_cast<SBEntry*>(malloc(size));
  memset(rv, 0, size);
  rv->set_type(type);
  rv->set_prefix_count(prefix_count);
  return rv;
}

void SBEntry::Destroy() {
  free(this);
}

// static
int SBEntry::PrefixSize(Type type) {
  switch (type) {
    case ADD_PREFIX:
      return sizeof(SBPrefix);
    case ADD_FULL_HASH:
      return sizeof(SBFullHash);
    case SUB_PREFIX:
      return sizeof(SBSubPrefix);
    case SUB_FULL_HASH:
      return sizeof(SBSubFullHash);
    default:
      NOTREACHED();
      return 0;
  }
}

int SBEntry::Size() const {
  return Size(type(), prefix_count());
}

// static
int SBEntry::Size(Type type, int prefix_count) {
  return sizeof(Data) + prefix_count * PrefixSize(type);
}

int SBEntry::ChunkIdAtPrefix(int index) const {
  if (type() == SUB_PREFIX)
    return sub_prefixes_[index].add_chunk;
  return (type() == SUB_FULL_HASH) ?
      sub_full_hashes_[index].add_chunk : chunk_id();
}

void SBEntry::SetChunkIdAtPrefix(int index, int chunk_id) {
  DCHECK(IsSub());

  if (type() == SUB_PREFIX)
    sub_prefixes_[index].add_chunk = chunk_id;
  else
    sub_full_hashes_[index].add_chunk = chunk_id;
}

const SBPrefix& SBEntry::PrefixAt(int index) const {
  DCHECK(IsPrefix());

  return IsAdd() ? add_prefixes_[index] : sub_prefixes_[index].prefix;
}

const SBFullHash& SBEntry::FullHashAt(int index) const {
  DCHECK(!IsPrefix());

  return IsAdd() ? add_full_hashes_[index] : sub_full_hashes_[index].prefix;
}

void SBEntry::SetPrefixAt(int index, const SBPrefix& prefix) {
  DCHECK(IsPrefix());

  if (IsAdd())
    add_prefixes_[index] = prefix;
  else
    sub_prefixes_[index].prefix = prefix;
}

void SBEntry::SetFullHashAt(int index, const SBFullHash& full_hash) {
  DCHECK(!IsPrefix());

  if (IsAdd())
    add_full_hashes_[index] = full_hash;
  else
    sub_full_hashes_[index].prefix = full_hash;
}


// Utility functions -----------------------------------------------------------

namespace safe_browsing_util {

// Listnames that browser can process.
const char kMalwareList[] = "goog-malware-shavar";
const char kPhishingList[] = "goog-phish-shavar";
const char kBinUrlList[] = "goog-badbinurl-shavar";
const char kBinHashList[] = "goog-badbinhash-shavar";

// Keywords to identify a list type from listname.
// TODO(lzheng): check if this can be replaced by full listnames.
const char kMalwareListKey[] = "-malware-";
const char kPhishingListKey[] = "-phish-";
const char kBinUrlListKey[] = "-badbinurl-";
const char kBinHashListKey[] = "-badbinhash-";

int GetListId(const std::string& name) {
  int id;
  if (name == safe_browsing_util::kMalwareList) {
    id = MALWARE;
  } else if (name == safe_browsing_util::kPhishingList) {
    id = PHISH;
  } else if (name == safe_browsing_util::kBinUrlList) {
    id = BINURL;
  }  else if (name == safe_browsing_util::kBinHashList) {
    id = BINHASH;
  } else {
    id = INVALID;
  }
  return id;
}

bool GetListName(int list_id, std::string* list) {
  switch (list_id) {
    case MALWARE:
      *list = safe_browsing_util::kMalwareList;
      break;
    case PHISH:
      *list = safe_browsing_util::kPhishingList;
      break;
    case BINURL:
      *list = safe_browsing_util::kBinUrlList;
      break;
    case BINHASH:
      *list = safe_browsing_util::kBinHashList;
      break;
    default:
      return false;
  }
  return true;
}

std::string Unescape(const std::string& url) {
  std::string unescaped_str(url);
  std::string old_unescaped_str;
  const int kMaxLoopIterations = 1024;
  int loop_var = 0;
  do {
    old_unescaped_str = unescaped_str;
    unescaped_str = UnescapeURLComponent(old_unescaped_str,
        UnescapeRule::CONTROL_CHARS | UnescapeRule::SPACES |
        UnescapeRule::URL_SPECIAL_CHARS);
  } while (unescaped_str != old_unescaped_str && ++loop_var <=
           kMaxLoopIterations);

  return unescaped_str;
}

std::string Escape(const std::string& url) {
  std::string escaped_str;
  const char* kHexString = "0123456789ABCDEF";
  for (size_t i = 0; i < url.length(); i++) {
    unsigned char c = static_cast<unsigned char>(url[i]);
    if (c <= ' ' || c > '~' || c == '#' || c == '%') {
      escaped_str.push_back('%');
      escaped_str.push_back(kHexString[c >> 4]);
      escaped_str.push_back(kHexString[c & 0xf]);
    } else {
      escaped_str.push_back(c);
    }
  }

  return escaped_str;
}

std::string RemoveConsecutiveChars(const std::string& str, const char c) {
  std::string output(str);
  std::string string_to_find;
  std::string::size_type loc = 0;
  string_to_find.append(2, c);
  while ((loc = output.find(string_to_find, loc)) != std::string::npos) {
    output.erase(loc, 1);
  }

  return output;
}

// Canonicalizes url as per Google Safe Browsing Specification.
// See section 6.1 in
// http://code.google.com/p/google-safe-browsing/wiki/Protocolv2Spec.
void CanonicalizeUrl(const GURL& url,
                     std::string* canonicalized_hostname,
                     std::string* canonicalized_path,
                     std::string* canonicalized_query) {
  DCHECK(url.is_valid());

  // We only canonicalize "normal" URLs.
  if (!url.IsStandard())
    return;

  // Following canonicalization steps are excluded since url parsing takes care
  // of those :-
  // 1. Remove any tab (0x09), CR (0x0d), and LF (0x0a) chars from url.
  //    (Exclude escaped version of these chars).
  // 2. Normalize hostname to 4 dot-seperated decimal values.
  // 3. Lowercase hostname.
  // 4. Resolve path sequences "/../" and "/./".

  // That leaves us with the following :-
  // 1. Remove fragment in URL.
  GURL url_without_fragment;
  GURL::Replacements f_replacements;
  f_replacements.ClearRef();
  f_replacements.ClearUsername();
  f_replacements.ClearPassword();
  url_without_fragment = url.ReplaceComponents(f_replacements);

  // 2. Do URL unescaping until no more hex encoded characters exist.
  std::string url_unescaped_str(Unescape(url_without_fragment.spec()));
  url_parse::Parsed parsed;
  url_parse::ParseStandardURL(url_unescaped_str.data(),
      url_unescaped_str.length(), &parsed);

  // 3. In hostname, remove all leading and trailing dots.
  const std::string host = (parsed.host.len > 0) ? url_unescaped_str.substr(
      parsed.host.begin, parsed.host.len) : "";
  const char kCharsToTrim[] = ".";
  std::string host_without_end_dots;
  TrimString(host, kCharsToTrim, &host_without_end_dots);

  // 4. In hostname, replace consecutive dots with a single dot.
  std::string host_without_consecutive_dots(RemoveConsecutiveChars(
      host_without_end_dots, '.'));

  // 5. In path, replace runs of consecutive slashes with a single slash.
  std::string path = (parsed.path.len > 0) ? url_unescaped_str.substr(
       parsed.path.begin, parsed.path.len): "";
  std::string path_without_consecutive_slash(RemoveConsecutiveChars(
      path, '/'));

  url_canon::Replacements<char> hp_replacements;
  hp_replacements.SetHost(host_without_consecutive_dots.data(),
  url_parse::Component(0, host_without_consecutive_dots.length()));
  hp_replacements.SetPath(path_without_consecutive_slash.data(),
  url_parse::Component(0, path_without_consecutive_slash.length()));

  std::string url_unescaped_with_can_hostpath;
  url_canon::StdStringCanonOutput output(&url_unescaped_with_can_hostpath);
  url_parse::Parsed temp_parsed;
  url_util::ReplaceComponents(url_unescaped_str.data(),
                              url_unescaped_str.length(), parsed,
                              hp_replacements, NULL, &output, &temp_parsed);
  output.Complete();

  // 6. Step needed to revert escaping done in url_util::ReplaceComponents.
  url_unescaped_with_can_hostpath = Unescape(url_unescaped_with_can_hostpath);

  // 7. After performing all above steps, percent-escape all chars in url which
  // are <= ASCII 32, >= 127, #, %. Escapes must be uppercase hex characters.
  std::string escaped_canon_url_str(Escape(url_unescaped_with_can_hostpath));
  url_parse::Parsed final_parsed;
  url_parse::ParseStandardURL(escaped_canon_url_str.data(),
                              escaped_canon_url_str.length(), &final_parsed);

  if (canonicalized_hostname && final_parsed.host.len > 0) {
    *canonicalized_hostname =
        escaped_canon_url_str.substr(final_parsed.host.begin,
                                     final_parsed.host.len);
  }
  if (canonicalized_path && final_parsed.path.len > 0) {
    *canonicalized_path = escaped_canon_url_str.substr(final_parsed.path.begin,
                                                       final_parsed.path.len);
  }
  if (canonicalized_query && final_parsed.query.len > 0) {
    *canonicalized_query = escaped_canon_url_str.substr(
        final_parsed.query.begin, final_parsed.query.len);
  }
}

void GenerateHostsToCheck(const GURL& url, std::vector<std::string>* hosts) {
  hosts->clear();

  std::string canon_host;
  CanonicalizeUrl(url, &canon_host, NULL, NULL);

  const std::string host = canon_host;  // const sidesteps GCC bugs below!
  if (host.empty())
    return;

  // Per the Safe Browsing Protocol v2 spec, we try the host, and also up to 4
  // hostnames formed by starting with the last 5 components and successively
  // removing the leading component.  The last component isn't examined alone,
  // since it's the TLD or a subcomponent thereof.
  //
  // Note that we don't need to be clever about stopping at the "real" eTLD --
  // the data on the server side has been filtered to ensure it will not
  // blacklist a whole TLD, and it's not significantly slower on our side to
  // just check too much.
  //
  // Also note that because we have a simple blacklist, not some sort of complex
  // whitelist-in-blacklist or vice versa, it doesn't matter what order we check
  // these in.
  const size_t kMaxHostsToCheck = 4;
  bool skipped_last_component = false;
  for (std::string::const_reverse_iterator i(host.rbegin());
       i != host.rend() && hosts->size() < kMaxHostsToCheck; ++i) {
    if (*i == '.') {
      if (skipped_last_component)
        hosts->push_back(std::string(i.base(), host.end()));
      else
        skipped_last_component = true;
    }
  }
  hosts->push_back(host);
}

void GeneratePathsToCheck(const GURL& url, std::vector<std::string>* paths) {
  paths->clear();

  std::string canon_path;
  std::string canon_query;
  CanonicalizeUrl(url, NULL, &canon_path, &canon_query);

  const std::string path = canon_path;   // const sidesteps GCC bugs below!
  const std::string query = canon_query;
  if (path.empty())
    return;

  // Per the Safe Browsing Protocol v2 spec, we try the exact path with/without
  // the query parameters, and also up to 4 paths formed by starting at the root
  // and adding more path components.
  //
  // As with the hosts above, it doesn't matter what order we check these in.
  const size_t kMaxPathsToCheck = 4;
  for (std::string::const_iterator i(path.begin());
       i != path.end() && paths->size() < kMaxPathsToCheck; ++i) {
    if (*i == '/')
      paths->push_back(std::string(path.begin(), i + 1));
  }

  if (!paths->empty() && paths->back() != path)
    paths->push_back(path);

  if (!query.empty())
    paths->push_back(path + "?" + query);
}

int GetHashIndex(const SBFullHash& hash,
                 const std::vector<SBFullHashResult>& full_hashes) {
  for (size_t i = 0; i < full_hashes.size(); ++i) {
    if (hash == full_hashes[i].hash)
      return static_cast<int>(i);
  }
  return -1;
}

int GetUrlHashIndex(const GURL& url,
                    const std::vector<SBFullHashResult>& full_hashes) {
  if (full_hashes.empty())
    return -1;

  std::vector<std::string> hosts, paths;
  GenerateHostsToCheck(url, &hosts);
  GeneratePathsToCheck(url, &paths);

  for (size_t h = 0; h < hosts.size(); ++h) {
    for (size_t p = 0; p < paths.size(); ++p) {
      SBFullHash key;
      base::SHA256HashString(hosts[h] + paths[p],
                             key.full_hash,
                             sizeof(SBFullHash));
      int index = GetHashIndex(key, full_hashes);
      if (index != -1) return index;
    }
  }

  return -1;
}

bool IsPhishingList(const std::string& list_name) {
  return list_name.find(kPhishingListKey) != std::string::npos;
}

bool IsMalwareList(const std::string& list_name) {
  return list_name.find(kMalwareListKey) != std::string::npos;
}

bool IsBadbinurlList(const std::string& list_name) {
  return list_name.find(kBinUrlListKey) != std::string::npos;
}

bool IsBadbinhashList(const std::string& list_name) {
  return list_name.find(kBinHashListKey) != std::string::npos;
}

static void DecodeWebSafe(std::string* decoded) {
  DCHECK(decoded);
  for (std::string::iterator i(decoded->begin()); i != decoded->end(); ++i) {
    if (*i == '_')
      *i = '/';
    else if (*i == '-')
      *i = '+';
  }
}

bool VerifyMAC(const std::string& key, const std::string& mac,
               const char* data, int data_length) {
  std::string key_copy = key;
  DecodeWebSafe(&key_copy);
  std::string decoded_key;
  base::Base64Decode(key_copy, &decoded_key);

  std::string mac_copy = mac;
  DecodeWebSafe(&mac_copy);
  std::string decoded_mac;
  base::Base64Decode(mac_copy, &decoded_mac);

  base::HMAC hmac(base::HMAC::SHA1);
  if (!hmac.Init(decoded_key))
    return false;
  const std::string data_str(data, data_length);
  unsigned char digest[kSafeBrowsingMacDigestSize];
  if (!hmac.Sign(data_str, digest, kSafeBrowsingMacDigestSize))
    return false;

  return !memcmp(digest, decoded_mac.data(), kSafeBrowsingMacDigestSize);
}

GURL GeneratePhishingReportUrl(const std::string& report_page,
                               const std::string& url_to_report) {
  icu::Locale locale = icu::Locale::getDefault();
  const char* lang = locale.getLanguage();
  if (!lang)
    lang = "en";  // fallback
  const std::string continue_esc =
      EscapeQueryParamValue(StringPrintf(kContinueUrlFormat, lang), true);
  const std::string current_esc = EscapeQueryParamValue(url_to_report, true);

#if defined(OS_WIN)
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  std::string client_name(dist->GetSafeBrowsingName());
#else
  std::string client_name("googlechrome");
#endif

  GURL report_url(report_page +
      StringPrintf(kReportParams, client_name.c_str(), continue_esc.c_str(),
                   current_esc.c_str()));
  return google_util::AppendGoogleLocaleParam(report_url);
}

void StringToSBFullHash(const std::string& hash_in, SBFullHash* hash_out) {
  DCHECK_EQ(static_cast<size_t>(base::SHA256_LENGTH), hash_in.size());
  memcpy(hash_out->full_hash, hash_in.data(), base::SHA256_LENGTH);
}

}  // namespace safe_browsing_util
