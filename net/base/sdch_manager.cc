// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/sdch_manager.h"

#include "base/base64.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/default_clock.h"
#include "base/values.h"
#include "crypto/sha2.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/sdch_observer.h"
#include "net/url_request/url_request_http_job.h"

namespace {

void StripTrailingDot(GURL* gurl) {
  std::string host(gurl->host());

  if (host.empty())
    return;

  if (*host.rbegin() != '.')
    return;

  host.resize(host.size() - 1);

  GURL::Replacements replacements;
  replacements.SetHostStr(host);
  *gurl = gurl->ReplaceComponents(replacements);
  return;
}

}  // namespace

namespace net {

// static
bool SdchManager::g_sdch_enabled_ = true;

// static
bool SdchManager::g_secure_scheme_supported_ = true;

SdchManager::Dictionary::Dictionary(const std::string& dictionary_text,
                                    size_t offset,
                                    const std::string& client_hash,
                                    const std::string& server_hash,
                                    const GURL& gurl,
                                    const std::string& domain,
                                    const std::string& path,
                                    const base::Time& expiration,
                                    const std::set<int>& ports)
    : text_(dictionary_text, offset),
      client_hash_(client_hash),
      server_hash_(server_hash),
      url_(gurl),
      domain_(domain),
      path_(path),
      expiration_(expiration),
      ports_(ports),
      clock_(new base::DefaultClock) {
}

SdchManager::Dictionary::Dictionary(const SdchManager::Dictionary& rhs)
    : text_(rhs.text_),
      client_hash_(rhs.client_hash_),
      server_hash_(rhs.server_hash_),
      url_(rhs.url_),
      domain_(rhs.domain_),
      path_(rhs.path_),
      expiration_(rhs.expiration_),
      ports_(rhs.ports_),
      clock_(new base::DefaultClock) {
}

SdchManager::Dictionary::~Dictionary() {}

// Security functions restricting loads and use of dictionaries.

// static
SdchProblemCode SdchManager::Dictionary::CanSet(const std::string& domain,
                                                const std::string& path,
                                                const std::set<int>& ports,
                                                const GURL& dictionary_url) {
  /*
  A dictionary is invalid and must not be stored if any of the following are
  true:
    1. The dictionary has no Domain attribute.
    2. The effective host name that derives from the referer URL host name does
      not domain-match the Domain attribute.
    3. The Domain attribute is a top level domain.
    4. The referer URL host is a host domain name (not IP address) and has the
      form HD, where D is the value of the Domain attribute, and H is a string
      that contains one or more dots.
    5. If the dictionary has a Port attribute and the referer URL's port was not
      in the list.
  */

  // TODO(jar): Redirects in dictionary fetches might plausibly be problematic,
  // and hence the conservative approach is to not allow any redirects (if there
  // were any... then don't allow the dictionary to be set).

  if (domain.empty())
    return SDCH_DICTIONARY_MISSING_DOMAIN_SPECIFIER;  // Domain is required.

  if (registry_controlled_domains::GetDomainAndRegistry(
          domain, registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)
          .empty()) {
    return SDCH_DICTIONARY_SPECIFIES_TOP_LEVEL_DOMAIN;  // domain was a TLD.
  }

  if (!Dictionary::DomainMatch(dictionary_url, domain))
    return SDCH_DICTIONARY_DOMAIN_NOT_MATCHING_SOURCE_URL;

  std::string referrer_url_host = dictionary_url.host();
  size_t postfix_domain_index = referrer_url_host.rfind(domain);
  // See if it is indeed a postfix, or just an internal string.
  if (referrer_url_host.size() == postfix_domain_index + domain.size()) {
    // It is a postfix... so check to see if there's a dot in the prefix.
    size_t end_of_host_index = referrer_url_host.find_first_of('.');
    if (referrer_url_host.npos != end_of_host_index &&
        end_of_host_index < postfix_domain_index) {
      return SDCH_DICTIONARY_REFERER_URL_HAS_DOT_IN_PREFIX;
    }
  }

  if (!ports.empty() && 0 == ports.count(dictionary_url.EffectiveIntPort()))
    return SDCH_DICTIONARY_PORT_NOT_MATCHING_SOURCE_URL;

  return SDCH_OK;
}

SdchProblemCode SdchManager::Dictionary::CanUse(
    const GURL& target_url) const {
  /*
    1. The request URL's host name domain-matches the Domain attribute of the
      dictionary.
    2. If the dictionary has a Port attribute, the request port is one of the
      ports listed in the Port attribute.
    3. The request URL path-matches the path attribute of the dictionary.
    4. The request is not an HTTPS request.
    We can override (ignore) item (4) only when we have explicitly enabled
    HTTPS support AND the dictionary acquisition scheme matches the target
     url scheme.
  */
  if (!DomainMatch(target_url, domain_))
    return SDCH_DICTIONARY_FOUND_HAS_WRONG_DOMAIN;

  if (!ports_.empty() && 0 == ports_.count(target_url.EffectiveIntPort()))
    return SDCH_DICTIONARY_FOUND_HAS_WRONG_PORT_LIST;

  if (path_.size() && !PathMatch(target_url.path(), path_))
    return SDCH_DICTIONARY_FOUND_HAS_WRONG_PATH;

  if (!SdchManager::secure_scheme_supported() && target_url.SchemeIsSecure())
    return SDCH_DICTIONARY_FOUND_HAS_WRONG_SCHEME;

  if (target_url.SchemeIsSecure() != url_.SchemeIsSecure())
    return SDCH_DICTIONARY_FOUND_HAS_WRONG_SCHEME;

  // TODO(jar): Remove overly restrictive failsafe test (added per security
  // review) when we have a need to be more general.
  if (!target_url.SchemeIsHTTPOrHTTPS())
    return SDCH_ATTEMPT_TO_DECODE_NON_HTTP_DATA;

  return SDCH_OK;
}

// static
bool SdchManager::Dictionary::PathMatch(const std::string& path,
                                        const std::string& restriction) {
  /*  Must be either:
  1. P2 is equal to P1
  2. P2 is a prefix of P1 and either the final character in P2 is "/" or the
      character following P2 in P1 is "/".
      */
  if (path == restriction)
    return true;
  size_t prefix_length = restriction.size();
  if (prefix_length > path.size())
    return false;  // Can't be a prefix.
  if (0 != path.compare(0, prefix_length, restriction))
    return false;
  return restriction[prefix_length - 1] == '/' || path[prefix_length] == '/';
}

// static
bool SdchManager::Dictionary::DomainMatch(const GURL& gurl,
                                          const std::string& restriction) {
  // TODO(jar): This is not precisely a domain match definition.
  return gurl.DomainIs(restriction.data(), restriction.size());
}

bool SdchManager::Dictionary::Expired() const {
  return clock_->Now() > expiration_;
}

void SdchManager::Dictionary::SetClockForTesting(
    scoped_ptr<base::Clock> clock) {
  clock_ = clock.Pass();
}

SdchManager::DictionarySet::DictionarySet() {}

SdchManager::DictionarySet::~DictionarySet() {}

std::string SdchManager::DictionarySet::GetDictionaryClientHashList() const {
  std::string result;
  bool first = true;
  for (const auto& entry: dictionaries_) {
    if (!first)
      result.append(",");

    result.append(entry.second->data.client_hash());
    first = false;
  }
  return result;
}

const SdchManager::Dictionary* SdchManager::DictionarySet::GetDictionary(
    const std::string& hash) const {
  auto it = dictionaries_.find(hash);
  if (it == dictionaries_.end())
    return NULL;

  return &it->second->data;
}

bool SdchManager::DictionarySet::Empty() const {
  return dictionaries_.empty();
}

void SdchManager::DictionarySet::AddDictionary(
    const std::string& server_hash,
    const scoped_refptr<base::RefCountedData<SdchManager::Dictionary>>&
    dictionary) {
  DCHECK(dictionaries_.end() == dictionaries_.find(server_hash));

  dictionaries_[server_hash] = dictionary;
}

SdchManager::SdchManager() : factory_(this) {
  DCHECK(thread_checker_.CalledOnValidThread());
}

SdchManager::~SdchManager() {
  DCHECK(thread_checker_.CalledOnValidThread());
  while (!dictionaries_.empty()) {
    auto it = dictionaries_.begin();
    dictionaries_.erase(it->first);
  }
#if defined(OS_CHROMEOS)
  // For debugging http://crbug.com/454198; remove when resolved.

  // Explicitly confirm that we can't notify any observers anymore.
  CHECK(!observers_.might_have_observers());
#endif
}

void SdchManager::ClearData() {
  blacklisted_domains_.clear();
  allow_latency_experiment_.clear();
  dictionaries_.clear();
  FOR_EACH_OBSERVER(SdchObserver, observers_, OnClearDictionaries(this));
}

// static
void SdchManager::SdchErrorRecovery(SdchProblemCode problem) {
  UMA_HISTOGRAM_ENUMERATION("Sdch3.ProblemCodes_5", problem,
                            SDCH_MAX_PROBLEM_CODE);
}

// static
void SdchManager::EnableSdchSupport(bool enabled) {
  g_sdch_enabled_ = enabled;
}

// static
void SdchManager::EnableSecureSchemeSupport(bool enabled) {
  g_secure_scheme_supported_ = enabled;
}

void SdchManager::BlacklistDomain(const GURL& url,
                                  SdchProblemCode blacklist_reason) {
  SetAllowLatencyExperiment(url, false);

  BlacklistInfo* blacklist_info =
      &blacklisted_domains_[base::StringToLowerASCII(url.host())];

  if (blacklist_info->count > 0)
    return;  // Domain is already blacklisted.

  if (blacklist_info->exponential_count > (INT_MAX - 1) / 2) {
    blacklist_info->exponential_count = INT_MAX;
  } else {
    blacklist_info->exponential_count =
        blacklist_info->exponential_count * 2 + 1;
  }

  blacklist_info->count = blacklist_info->exponential_count;
  blacklist_info->reason = blacklist_reason;
}

void SdchManager::BlacklistDomainForever(const GURL& url,
                                         SdchProblemCode blacklist_reason) {
  SetAllowLatencyExperiment(url, false);

  BlacklistInfo* blacklist_info =
      &blacklisted_domains_[base::StringToLowerASCII(url.host())];
  blacklist_info->count = INT_MAX;
  blacklist_info->exponential_count = INT_MAX;
  blacklist_info->reason = blacklist_reason;
}

void SdchManager::ClearBlacklistings() {
  blacklisted_domains_.clear();
}

void SdchManager::ClearDomainBlacklisting(const std::string& domain) {
  BlacklistInfo* blacklist_info = &blacklisted_domains_[
      base::StringToLowerASCII(domain)];
  blacklist_info->count = 0;
  blacklist_info->reason = SDCH_OK;
}

int SdchManager::BlackListDomainCount(const std::string& domain) {
  std::string domain_lower(base::StringToLowerASCII(domain));

  if (blacklisted_domains_.end() == blacklisted_domains_.find(domain_lower))
    return 0;
  return blacklisted_domains_[domain_lower].count;
}

int SdchManager::BlacklistDomainExponential(const std::string& domain) {
  std::string domain_lower(base::StringToLowerASCII(domain));

  if (blacklisted_domains_.end() == blacklisted_domains_.find(domain_lower))
    return 0;
  return blacklisted_domains_[domain_lower].exponential_count;
}

SdchProblemCode SdchManager::IsInSupportedDomain(const GURL& url) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!g_sdch_enabled_ )
    return SDCH_DISABLED;

  if (!secure_scheme_supported() && url.SchemeIsSecure())
    return SDCH_SECURE_SCHEME_NOT_SUPPORTED;

  if (blacklisted_domains_.empty())
    return SDCH_OK;

  DomainBlacklistInfo::iterator it =
      blacklisted_domains_.find(base::StringToLowerASCII(url.host()));
  if (blacklisted_domains_.end() == it || it->second.count == 0)
    return SDCH_OK;

  UMA_HISTOGRAM_ENUMERATION("Sdch3.BlacklistReason", it->second.reason,
                            SDCH_MAX_PROBLEM_CODE);

  int count = it->second.count - 1;
  if (count > 0) {
    it->second.count = count;
  } else {
    it->second.count = 0;
    it->second.reason = SDCH_OK;
  }

  return SDCH_DOMAIN_BLACKLIST_INCLUDES_TARGET;
}

SdchProblemCode SdchManager::OnGetDictionary(const GURL& request_url,
                                             const GURL& dictionary_url) {
  DCHECK(thread_checker_.CalledOnValidThread());
  SdchProblemCode rv = CanFetchDictionary(request_url, dictionary_url);
  if (rv != SDCH_OK)
    return rv;

  FOR_EACH_OBSERVER(SdchObserver,
                    observers_,
                    OnGetDictionary(this, request_url, dictionary_url));

  return SDCH_OK;
}

void SdchManager::OnDictionaryUsed(const std::string& server_hash) {
  FOR_EACH_OBSERVER(SdchObserver, observers_,
                    OnDictionaryUsed(this, server_hash));
}

SdchProblemCode SdchManager::CanFetchDictionary(
    const GURL& referring_url,
    const GURL& dictionary_url) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  /* The user agent may retrieve a dictionary from the dictionary URL if all of
     the following are true:
       1 The dictionary URL host name matches the referrer URL host name and
           scheme.
       2 The dictionary URL host name domain matches the parent domain of the
           referrer URL host name
       3 The parent domain of the referrer URL host name is not a top level
           domain
   */
  // Item (1) above implies item (2). Spec should be updated.
  // I take "host name match" to be "is identical to"
  if (referring_url.host() != dictionary_url.host() ||
      referring_url.scheme() != dictionary_url.scheme())
    return SDCH_DICTIONARY_LOAD_ATTEMPT_FROM_DIFFERENT_HOST;

  if (!secure_scheme_supported() && referring_url.SchemeIsSecure())
    return SDCH_DICTIONARY_SELECTED_FOR_SSL;

  // TODO(jar): Remove this failsafe conservative hack which is more restrictive
  // than current SDCH spec when needed, and justified by security audit.
  if (!referring_url.SchemeIsHTTPOrHTTPS())
    return SDCH_DICTIONARY_SELECTED_FROM_NON_HTTP;

  return SDCH_OK;
}

scoped_ptr<SdchManager::DictionarySet>
SdchManager::GetDictionarySet(const GURL& target_url) {
  if (IsInSupportedDomain(target_url) != SDCH_OK)
    return NULL;

  int count = 0;
  scoped_ptr<SdchManager::DictionarySet> result(new DictionarySet);
  for (const auto& entry: dictionaries_) {
    if (entry.second->data.CanUse(target_url) != SDCH_OK)
      continue;
    if (entry.second->data.Expired())
      continue;
    ++count;
    result->AddDictionary(entry.first, entry.second);
  }

  if (count == 0)
    return NULL;

  UMA_HISTOGRAM_COUNTS("Sdch3.Advertisement_Count", count);

  return result.Pass();
}

scoped_ptr<SdchManager::DictionarySet>
SdchManager::GetDictionarySetByHash(
    const GURL& target_url,
    const std::string& server_hash,
    SdchProblemCode* problem_code) {
  scoped_ptr<SdchManager::DictionarySet> result;

  *problem_code = SDCH_DICTIONARY_HASH_NOT_FOUND;
  const auto& it = dictionaries_.find(server_hash);
  if (it == dictionaries_.end())
    return result.Pass();

  *problem_code = it->second->data.CanUse(target_url);
  if (*problem_code != SDCH_OK)
    return result.Pass();

  result.reset(new DictionarySet);
  result->AddDictionary(it->first, it->second);
  return result.Pass();
}

// static
void SdchManager::GenerateHash(const std::string& dictionary_text,
    std::string* client_hash, std::string* server_hash) {
  char binary_hash[32];
  crypto::SHA256HashString(dictionary_text, binary_hash, sizeof(binary_hash));

  std::string first_48_bits(&binary_hash[0], 6);
  std::string second_48_bits(&binary_hash[6], 6);
  UrlSafeBase64Encode(first_48_bits, client_hash);
  UrlSafeBase64Encode(second_48_bits, server_hash);

  DCHECK_EQ(server_hash->length(), 8u);
  DCHECK_EQ(client_hash->length(), 8u);
}

// Methods for supporting latency experiments.

bool SdchManager::AllowLatencyExperiment(const GURL& url) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return allow_latency_experiment_.end() !=
      allow_latency_experiment_.find(url.host());
}

void SdchManager::SetAllowLatencyExperiment(const GURL& url, bool enable) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (enable) {
    allow_latency_experiment_.insert(url.host());
    return;
  }
  ExperimentSet::iterator it = allow_latency_experiment_.find(url.host());
  if (allow_latency_experiment_.end() == it)
    return;  // It was already erased, or never allowed.
  SdchErrorRecovery(SDCH_LATENCY_TEST_DISALLOWED);
  allow_latency_experiment_.erase(it);
}

void SdchManager::AddObserver(SdchObserver* observer) {
  observers_.AddObserver(observer);
}

void SdchManager::RemoveObserver(SdchObserver* observer) {
  observers_.RemoveObserver(observer);
}

SdchProblemCode SdchManager::AddSdchDictionary(
    const std::string& dictionary_text,
    const GURL& dictionary_url,
    std::string* server_hash_p) {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::string client_hash;
  std::string server_hash;
  GenerateHash(dictionary_text, &client_hash, &server_hash);
  if (dictionaries_.find(server_hash) != dictionaries_.end())
    return SDCH_DICTIONARY_ALREADY_LOADED;  // Already loaded.

  std::string domain, path;
  std::set<int> ports;
  base::Time expiration(base::Time::Now() + base::TimeDelta::FromDays(30));

  if (dictionary_text.empty())
    return SDCH_DICTIONARY_HAS_NO_TEXT;  // Missing header.

  size_t header_end = dictionary_text.find("\n\n");
  if (std::string::npos == header_end)
    return SDCH_DICTIONARY_HAS_NO_HEADER;  // Missing header.

  size_t line_start = 0;  // Start of line being parsed.
  while (1) {
    size_t line_end = dictionary_text.find('\n', line_start);
    DCHECK(std::string::npos != line_end);
    DCHECK_LE(line_end, header_end);

    size_t colon_index = dictionary_text.find(':', line_start);
    if (std::string::npos == colon_index)
      return SDCH_DICTIONARY_HEADER_LINE_MISSING_COLON;  // Illegal line missing
                                                         // a colon.

    if (colon_index > line_end)
      break;

    size_t value_start = dictionary_text.find_first_not_of(" \t",
                                                           colon_index + 1);
    if (std::string::npos != value_start) {
      if (value_start >= line_end)
        break;
      std::string name(dictionary_text, line_start, colon_index - line_start);
      std::string value(dictionary_text, value_start, line_end - value_start);
      name = base::StringToLowerASCII(name);
      if (name == "domain") {
        domain = value;
      } else if (name == "path") {
        path = value;
      } else if (name == "format-version") {
        if (value != "1.0")
          return SDCH_DICTIONARY_UNSUPPORTED_VERSION;
      } else if (name == "max-age") {
        int64 seconds;
        base::StringToInt64(value, &seconds);
        expiration = base::Time::Now() + base::TimeDelta::FromSeconds(seconds);
      } else if (name == "port") {
        int port;
        base::StringToInt(value, &port);
        if (port >= 0)
          ports.insert(port);
      }
    }

    if (line_end >= header_end)
      break;
    line_start = line_end + 1;
  }

  // Narrow fix for http://crbug.com/389451.
  GURL dictionary_url_normalized(dictionary_url);
  StripTrailingDot(&dictionary_url_normalized);

  SdchProblemCode rv = IsInSupportedDomain(dictionary_url_normalized);
  if (rv != SDCH_OK)
    return rv;

  rv = Dictionary::CanSet(domain, path, ports, dictionary_url_normalized);
  if (rv != SDCH_OK)
    return rv;

  UMA_HISTOGRAM_COUNTS("Sdch3.Dictionary size loaded", dictionary_text.size());
  DVLOG(1) << "Loaded dictionary with client hash " << client_hash
           << " and server hash " << server_hash;
  Dictionary dictionary(dictionary_text, header_end + 2, client_hash,
                        server_hash, dictionary_url_normalized, domain, path,
                        expiration, ports);
  dictionaries_[server_hash] =
      new base::RefCountedData<Dictionary>(dictionary);
  if (server_hash_p)
    *server_hash_p = server_hash;

  return SDCH_OK;
}

SdchProblemCode SdchManager::RemoveSdchDictionary(
    const std::string& server_hash) {
  if (dictionaries_.find(server_hash) == dictionaries_.end())
    return SDCH_DICTIONARY_HASH_NOT_FOUND;

  dictionaries_.erase(server_hash);
  return SDCH_OK;
}

// static
scoped_ptr<SdchManager::DictionarySet>
SdchManager::CreateEmptyDictionarySetForTesting() {
  return scoped_ptr<DictionarySet>(new DictionarySet).Pass();
}

// For investigation of http://crbug.com/454198; remove when resolved.
base::WeakPtr<SdchManager> SdchManager::GetWeakPtr() {
  return factory_.GetWeakPtr();
}

// static
void SdchManager::UrlSafeBase64Encode(const std::string& input,
                                      std::string* output) {
  // Since this is only done during a dictionary load, and hashes are only 8
  // characters, we just do the simple fixup, rather than rewriting the encoder.
  base::Base64Encode(input, output);
  std::replace(output->begin(), output->end(), '+', '-');
  std::replace(output->begin(), output->end(), '/', '_');
}

base::Value* SdchManager::SdchInfoToValue() const {
  base::DictionaryValue* value = new base::DictionaryValue();

  value->SetBoolean("sdch_enabled", sdch_enabled());
  value->SetBoolean("secure_scheme_support", secure_scheme_supported());

  base::ListValue* entry_list = new base::ListValue();
  for (const auto& entry: dictionaries_) {
    base::DictionaryValue* entry_dict = new base::DictionaryValue();
    entry_dict->SetString("url", entry.second->data.url().spec());
    entry_dict->SetString("client_hash", entry.second->data.client_hash());
    entry_dict->SetString("domain", entry.second->data.domain());
    entry_dict->SetString("path", entry.second->data.path());
    base::ListValue* port_list = new base::ListValue();
    for (std::set<int>::const_iterator port_it =
             entry.second->data.ports().begin();
         port_it != entry.second->data.ports().end(); ++port_it) {
      port_list->AppendInteger(*port_it);
    }
    entry_dict->Set("ports", port_list);
    entry_dict->SetString("server_hash", entry.first);
    entry_list->Append(entry_dict);
  }
  value->Set("dictionaries", entry_list);

  entry_list = new base::ListValue();
  for (DomainBlacklistInfo::const_iterator it = blacklisted_domains_.begin();
       it != blacklisted_domains_.end(); ++it) {
    if (it->second.count == 0)
      continue;
    base::DictionaryValue* entry_dict = new base::DictionaryValue();
    entry_dict->SetString("domain", it->first);
    if (it->second.count != INT_MAX)
      entry_dict->SetInteger("tries", it->second.count);
    entry_dict->SetInteger("reason", it->second.reason);
    entry_list->Append(entry_dict);
  }
  value->Set("blacklisted", entry_list);

  return value;
}

}  // namespace net
