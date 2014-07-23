// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/safe_browsing_util.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/safe_browsing/chunk.pb.h"
#include "components/google/core/browser/google_util.h"
#include "crypto/sha2.h"
#include "net/base/escape.h"
#include "url/gurl.h"
#include "url/url_util.h"

#if defined(OS_WIN)
#include "chrome/installer/util/browser_distribution.h"
#endif

static const char kReportParams[] = "?tpl=%s&url=%s";

SBFullHash SBFullHashForString(const base::StringPiece& str) {
  SBFullHash h;
  crypto::SHA256HashString(str, &h.full_hash, sizeof(h.full_hash));
  return h;
}

// SBCachedFullHashResult ------------------------------------------------------

SBCachedFullHashResult::SBCachedFullHashResult() {}

SBCachedFullHashResult::SBCachedFullHashResult(
    const base::Time& in_expire_after)
    : expire_after(in_expire_after) {}

SBCachedFullHashResult::~SBCachedFullHashResult() {}

// SBChunkData -----------------------------------------------------------------

// TODO(shess): Right now this contains a scoped_ptr<ChunkData> so that the
// proto buffer isn't copied all over the place, then these are contained in a
// ScopedVector for purposes of passing things around between tasks.  This seems
// convoluted.  Maybe it would make sense to have an overall container class
// returning references to a nested per-chunk class?

SBChunkData::SBChunkData() {
}

SBChunkData::SBChunkData(safe_browsing::ChunkData* raw_data)
    : chunk_data_(raw_data) {
  DCHECK(chunk_data_.get());
}

SBChunkData::~SBChunkData() {
}

bool SBChunkData::ParseFrom(const unsigned char* data, size_t length) {
  scoped_ptr<safe_browsing::ChunkData> chunk(new safe_browsing::ChunkData());
  if (!chunk->ParseFromArray(data, length))
    return false;

  if (chunk->chunk_type() != safe_browsing::ChunkData::ADD &&
      chunk->chunk_type() != safe_browsing::ChunkData::SUB) {
    return false;
  }

  size_t hash_size = 0;
  if (chunk->prefix_type() == safe_browsing::ChunkData::PREFIX_4B) {
    hash_size = sizeof(SBPrefix);
  } else if (chunk->prefix_type() == safe_browsing::ChunkData::FULL_32B) {
    hash_size = sizeof(SBFullHash);
  } else {
    return false;
  }

  const size_t hash_count = chunk->hashes().size() / hash_size;
  if (hash_count * hash_size != chunk->hashes().size())
    return false;

  if (chunk->chunk_type() == safe_browsing::ChunkData::SUB &&
      static_cast<size_t>(chunk->add_numbers_size()) != hash_count) {
    return false;
  }

  chunk_data_.swap(chunk);
  return true;
}

int SBChunkData::ChunkNumber() const {
  return chunk_data_->chunk_number();
}

bool SBChunkData::IsAdd() const {
  return chunk_data_->chunk_type() == safe_browsing::ChunkData::ADD;
}

bool SBChunkData::IsSub() const {
  return chunk_data_->chunk_type() == safe_browsing::ChunkData::SUB;
}

int SBChunkData::AddChunkNumberAt(size_t i) const {
  DCHECK(IsSub());
  DCHECK((IsPrefix() && i < PrefixCount()) ||
         (IsFullHash() && i < FullHashCount()));
  return chunk_data_->add_numbers(i);
}

bool SBChunkData::IsPrefix() const {
  return chunk_data_->prefix_type() == safe_browsing::ChunkData::PREFIX_4B;
}

size_t SBChunkData::PrefixCount() const {
  DCHECK(IsPrefix());
  return chunk_data_->hashes().size() / sizeof(SBPrefix);
}

SBPrefix SBChunkData::PrefixAt(size_t i) const {
  DCHECK(IsPrefix());
  DCHECK_LT(i, PrefixCount());

  SBPrefix prefix;
  memcpy(&prefix, chunk_data_->hashes().data() + i * sizeof(SBPrefix),
         sizeof(SBPrefix));
  return prefix;
}

bool SBChunkData::IsFullHash() const {
  return chunk_data_->prefix_type() == safe_browsing::ChunkData::FULL_32B;
}

size_t SBChunkData::FullHashCount() const {
  DCHECK(IsFullHash());
  return chunk_data_->hashes().size() / sizeof(SBFullHash);
}

SBFullHash SBChunkData::FullHashAt(size_t i) const {
  DCHECK(IsFullHash());
  DCHECK_LT(i, FullHashCount());

  SBFullHash full_hash;
  memcpy(&full_hash, chunk_data_->hashes().data() + i * sizeof(SBFullHash),
         sizeof(SBFullHash));
  return full_hash;
}

// SBListChunkRanges -----------------------------------------------------------

SBListChunkRanges::SBListChunkRanges(const std::string& n)
    : name(n) {
}

// SBChunkDelete ---------------------------------------------------------------

SBChunkDelete::SBChunkDelete() : is_sub_del(false) {}

SBChunkDelete::~SBChunkDelete() {}

// Utility functions -----------------------------------------------------------

namespace {
bool IsKnownList(const std::string& name) {
  for (size_t i = 0; i < arraysize(safe_browsing_util::kAllLists); ++i) {
    if (!strcmp(safe_browsing_util::kAllLists[i], name.c_str())) {
      return true;
    }
  }
  return false;
}
}  // namespace

namespace safe_browsing_util {

// Listnames that browser can process.
// TODO(shess): This shouldn't be OS-driven <http://crbug.com/394379>
#if defined(OS_ANDROID)
// NOTE(shess): This difference is also reflected in the store name in
// safe_browsing_database.cc.
const char kMalwareList[] = "goog-mobilemalware-shavar";
const char kPhishingList[] = "goog-mobilephish-shavar";
#else
const char kMalwareList[] = "goog-malware-shavar";
const char kPhishingList[] = "goog-phish-shavar";
#endif
const char kBinUrlList[] = "goog-badbinurl-shavar";
const char kCsdWhiteList[] = "goog-csdwhite-sha256";
const char kDownloadWhiteList[] = "goog-downloadwhite-digest256";
const char kExtensionBlacklist[] = "goog-badcrxids-digestvar";
const char kSideEffectFreeWhitelist[] = "goog-sideeffectfree-shavar";
const char kIPBlacklist[] = "goog-badip-digest256";

const char* kAllLists[8] = {
  kMalwareList,
  kPhishingList,
  kBinUrlList,
  kCsdWhiteList,
  kDownloadWhiteList,
  kExtensionBlacklist,
  kSideEffectFreeWhitelist,
  kIPBlacklist,
};

ListType GetListId(const base::StringPiece& name) {
  ListType id;
  if (name == safe_browsing_util::kMalwareList) {
    id = MALWARE;
  } else if (name == safe_browsing_util::kPhishingList) {
    id = PHISH;
  } else if (name == safe_browsing_util::kBinUrlList) {
    id = BINURL;
  } else if (name == safe_browsing_util::kCsdWhiteList) {
    id = CSDWHITELIST;
  } else if (name == safe_browsing_util::kDownloadWhiteList) {
    id = DOWNLOADWHITELIST;
  } else if (name == safe_browsing_util::kExtensionBlacklist) {
    id = EXTENSIONBLACKLIST;
  } else if (name == safe_browsing_util::kSideEffectFreeWhitelist) {
    id = SIDEEFFECTFREEWHITELIST;
  } else if (name == safe_browsing_util::kIPBlacklist) {
    id = IPBLACKLIST;
  } else {
    id = INVALID;
  }
  return id;
}

bool GetListName(ListType list_id, std::string* list) {
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
    case CSDWHITELIST:
      *list = safe_browsing_util::kCsdWhiteList;
      break;
    case DOWNLOADWHITELIST:
      *list = safe_browsing_util::kDownloadWhiteList;
      break;
    case EXTENSIONBLACKLIST:
      *list = safe_browsing_util::kExtensionBlacklist;
      break;
    case SIDEEFFECTFREEWHITELIST:
      *list = safe_browsing_util::kSideEffectFreeWhitelist;
      break;
    case IPBLACKLIST:
      *list = safe_browsing_util::kIPBlacklist;
      break;
    default:
      return false;
  }
  DCHECK(IsKnownList(*list));
  return true;
}

std::string Unescape(const std::string& url) {
  std::string unescaped_str(url);
  std::string old_unescaped_str;
  const int kMaxLoopIterations = 1024;
  int loop_var = 0;
  do {
    old_unescaped_str = unescaped_str;
    unescaped_str = net::UnescapeURLComponent(old_unescaped_str,
        net::UnescapeRule::CONTROL_CHARS | net::UnescapeRule::SPACES |
        net::UnescapeRule::URL_SPECIAL_CHARS);
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
  url::Parsed parsed;
  url::ParseStandardURL(url_unescaped_str.data(), url_unescaped_str.length(),
                        &parsed);

  // 3. In hostname, remove all leading and trailing dots.
  const std::string host =
      (parsed.host.len > 0)
          ? url_unescaped_str.substr(parsed.host.begin, parsed.host.len)
          : std::string();
  std::string host_without_end_dots;
  base::TrimString(host, ".", &host_without_end_dots);

  // 4. In hostname, replace consecutive dots with a single dot.
  std::string host_without_consecutive_dots(RemoveConsecutiveChars(
      host_without_end_dots, '.'));

  // 5. In path, replace runs of consecutive slashes with a single slash.
  std::string path =
      (parsed.path.len > 0)
          ? url_unescaped_str.substr(parsed.path.begin, parsed.path.len)
          : std::string();
  std::string path_without_consecutive_slash(RemoveConsecutiveChars(path, '/'));

  url::Replacements<char> hp_replacements;
  hp_replacements.SetHost(
      host_without_consecutive_dots.data(),
      url::Component(0, host_without_consecutive_dots.length()));
  hp_replacements.SetPath(
      path_without_consecutive_slash.data(),
      url::Component(0, path_without_consecutive_slash.length()));

  std::string url_unescaped_with_can_hostpath;
  url::StdStringCanonOutput output(&url_unescaped_with_can_hostpath);
  url::Parsed temp_parsed;
  url::ReplaceComponents(url_unescaped_str.data(),
                         url_unescaped_str.length(),
                         parsed,
                         hp_replacements,
                         NULL,
                         &output,
                         &temp_parsed);
  output.Complete();

  // 6. Step needed to revert escaping done in url::ReplaceComponents.
  url_unescaped_with_can_hostpath = Unescape(url_unescaped_with_can_hostpath);

  // 7. After performing all above steps, percent-escape all chars in url which
  // are <= ASCII 32, >= 127, #, %. Escapes must be uppercase hex characters.
  std::string escaped_canon_url_str(Escape(url_unescaped_with_can_hostpath));
  url::Parsed final_parsed;
  url::ParseStandardURL(escaped_canon_url_str.data(),
                        escaped_canon_url_str.length(),
                        &final_parsed);

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

void GeneratePatternsToCheck(const GURL& url, std::vector<std::string>* urls) {
  std::vector<std::string> hosts, paths;
  GenerateHostsToCheck(url, &hosts);
  GeneratePathsToCheck(url, &paths);
  for (size_t h = 0; h < hosts.size(); ++h) {
    for (size_t p = 0; p < paths.size(); ++p) {
      urls->push_back(hosts[h] + paths[p]);
    }
  }
}

GURL GeneratePhishingReportUrl(const std::string& report_page,
                               const std::string& url_to_report,
                               bool is_client_side_detection) {
  const std::string current_esc = net::EscapeQueryParamValue(url_to_report,
                                                             true);

#if defined(OS_WIN)
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  std::string client_name(dist->GetSafeBrowsingName());
#else
  std::string client_name("googlechrome");
#endif
  if (is_client_side_detection)
    client_name.append("_csd");

  GURL report_url(report_page + base::StringPrintf(kReportParams,
                                                   client_name.c_str(),
                                                   current_esc.c_str()));
  return google_util::AppendGoogleLocaleParam(
      report_url, g_browser_process->GetApplicationLocale());
}

SBFullHash StringToSBFullHash(const std::string& hash_in) {
  DCHECK_EQ(crypto::kSHA256Length, hash_in.size());
  SBFullHash hash_out;
  memcpy(hash_out.full_hash, hash_in.data(), crypto::kSHA256Length);
  return hash_out;
}

std::string SBFullHashToString(const SBFullHash& hash) {
  DCHECK_EQ(crypto::kSHA256Length, sizeof(hash.full_hash));
  return std::string(hash.full_hash, sizeof(hash.full_hash));
}

}  // namespace safe_browsing_util
