// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/safe_browsing_util.h"

#include "base/base64.h"
#include "base/hmac.h"
#include "base/sha2.h"
#include "base/string_util.h"
#include "chrome/browser/google_util.h"
#include "googleurl/src/gurl.h"
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


// SBChunkList -----------------------------------------------------------------

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

SBEntry* SBEntry::Enlarge(int extra_prefixes) {
  int new_prefix_count = prefix_count() + extra_prefixes;
  SBEntry* rv = SBEntry::Create(type(), new_prefix_count);
  memcpy(rv, this, Size());  // NOTE: Blows away rv.data_!
  // We have to re-set |rv|'s prefix count since we just copied our own over it.
  rv->set_prefix_count(new_prefix_count);
  Destroy();
  return rv;
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

const char kMalwareList[] = "goog-malware-shavar";
const char kPhishingList[] = "goog-phish-shavar";

int GetListId(const std::string& name) {
  if (name == kMalwareList)
    return MALWARE;
  return (name == kPhishingList) ? PHISH : INVALID;
}

std::string GetListName(int list_id) {
  if (list_id == MALWARE)
    return kMalwareList;
  return (list_id == PHISH) ? kPhishingList : std::string();
}

void GenerateHostsToCheck(const GURL& url, std::vector<std::string>* hosts) {
  hosts->clear();
  const std::string host = url.host();  // const sidesteps GCC bugs below!
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
  const std::string path = url.path();  // const sidesteps GCC bugs below!
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

  if (paths->back() != path)
    paths->push_back(path);

  if (url.has_query())
    paths->push_back(path + "?" + url.query());
}

int CompareFullHashes(const GURL& url,
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

      for (size_t i = 0; i < full_hashes.size(); ++i) {
        if (key == full_hashes[i].hash)
          return static_cast<int>(i);
      }
    }
  }

  return -1;
}

bool IsPhishingList(const std::string& list_name) {
  return list_name.find("-phish-") != std::string::npos;
}

bool IsMalwareList(const std::string& list_name) {
  return list_name.find("-malware-") != std::string::npos;
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

}  // namespace safe_browsing_util
