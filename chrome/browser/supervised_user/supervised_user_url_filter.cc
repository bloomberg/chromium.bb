// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_url_filter.h"

#include <stddef.h>
#include <stdint.h>

#include <set>
#include <utility>

#include "base/containers/hash_tables.h"
#include "base/files/file_path.h"
#include "base/json/json_file_value_serializer.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/supervised_user/experimental/supervised_user_async_url_checker.h"
#include "chrome/browser/supervised_user/experimental/supervised_user_blacklist.h"
#include "chrome/grit/generated_resources.h"
#include "components/policy/core/browser/url_blacklist_manager.h"
#include "components/url_formatter/url_fixer.h"
#include "components/url_matcher/url_matcher.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"

using content::BrowserThread;
using net::registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES;
using net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES;
using net::registry_controlled_domains::GetRegistryLength;
using policy::URLBlacklist;
using url_matcher::URLMatcher;
using url_matcher::URLMatcherConditionSet;

using HostnameHash = SupervisedUserSiteList::HostnameHash;

namespace {

struct HashHostnameHash {
  size_t operator()(const HostnameHash& value) const {
    return value.hash();
  }
};

}  // namespace

struct SupervisedUserURLFilter::Contents {
  URLMatcher url_matcher;
  base::hash_multimap<HostnameHash,
                      scoped_refptr<SupervisedUserSiteList>,
                      HashHostnameHash> hostname_hashes;
  // This only tracks pattern lists.
  std::map<URLMatcherConditionSet::ID, scoped_refptr<SupervisedUserSiteList>>
      site_lists_by_matcher_id;
};

namespace {

// URL schemes not in this list (e.g., file:// and chrome://) will always be
// allowed.
const char* kFilteredSchemes[] = {
  "http",
  "https",
  "ftp",
  "gopher",
  "ws",
  "wss"
};

// This class encapsulates all the state that is required during construction of
// a new SupervisedUserURLFilter::Contents.
class FilterBuilder {
 public:
  FilterBuilder();
  ~FilterBuilder();

  // Adds a single URL pattern and returns the id of its matcher.
  URLMatcherConditionSet::ID AddPattern(const std::string& pattern);

  // Adds all the sites in |site_list|, with URL patterns and hostname hashes.
  void AddSiteList(const scoped_refptr<SupervisedUserSiteList>& site_list);

  // Finalizes construction of the SupervisedUserURLFilter::Contents and returns
  // them. This method should be called before this object is destroyed.
  std::unique_ptr<SupervisedUserURLFilter::Contents> Build();

 private:
  std::unique_ptr<SupervisedUserURLFilter::Contents> contents_;
  URLMatcherConditionSet::Vector all_conditions_;
  URLMatcherConditionSet::ID matcher_id_;
  std::map<URLMatcherConditionSet::ID, scoped_refptr<SupervisedUserSiteList>>
      site_lists_by_matcher_id_;
};

FilterBuilder::FilterBuilder()
    : contents_(new SupervisedUserURLFilter::Contents()),
      matcher_id_(0) {}

FilterBuilder::~FilterBuilder() {
  DCHECK(!contents_.get());
}

URLMatcherConditionSet::ID FilterBuilder::AddPattern(
    const std::string& pattern) {
  std::string scheme;
  std::string host;
  uint16_t port = 0;
  std::string path;
  std::string query;
  bool match_subdomains = true;
  URLBlacklist::SegmentURLCallback callback =
      static_cast<URLBlacklist::SegmentURLCallback>(url_formatter::SegmentURL);
  if (!URLBlacklist::FilterToComponents(
          callback, pattern,
          &scheme, &host, &match_subdomains, &port, &path, &query)) {
    LOG(ERROR) << "Invalid pattern " << pattern;
    return -1;
  }

  scoped_refptr<URLMatcherConditionSet> condition_set =
      URLBlacklist::CreateConditionSet(
          &contents_->url_matcher, ++matcher_id_,
          scheme, host, match_subdomains, port, path, query, true);
  all_conditions_.push_back(std::move(condition_set));
  return matcher_id_;
}

void FilterBuilder::AddSiteList(
    const scoped_refptr<SupervisedUserSiteList>& site_list) {
  for (const std::string& pattern : site_list->patterns()) {
    URLMatcherConditionSet::ID id = AddPattern(pattern);
    if (id >= 0) {
      site_lists_by_matcher_id_[id] = site_list;
    }
  }

  for (const HostnameHash& hash : site_list->hostname_hashes())
    contents_->hostname_hashes.insert(std::make_pair(hash, site_list));
}

std::unique_ptr<SupervisedUserURLFilter::Contents> FilterBuilder::Build() {
  contents_->url_matcher.AddConditionSets(all_conditions_);
  contents_->site_lists_by_matcher_id.insert(site_lists_by_matcher_id_.begin(),
                                             site_lists_by_matcher_id_.end());
  return std::move(contents_);
}

std::unique_ptr<SupervisedUserURLFilter::Contents>
CreateWhitelistFromPatternsForTesting(
    const std::vector<std::string>& patterns) {
  FilterBuilder builder;
  for (const std::string& pattern : patterns)
    builder.AddPattern(pattern);

  return builder.Build();
}

std::unique_ptr<SupervisedUserURLFilter::Contents>
CreateWhitelistsFromSiteListsForTesting(
    const std::vector<scoped_refptr<SupervisedUserSiteList>>& site_lists) {
  FilterBuilder builder;
  for (const scoped_refptr<SupervisedUserSiteList>& site_list : site_lists)
    builder.AddSiteList(site_list);
  return builder.Build();
}

std::unique_ptr<SupervisedUserURLFilter::Contents>
LoadWhitelistsOnBlockingPoolThread(
    const std::vector<scoped_refptr<SupervisedUserSiteList>>& site_lists) {
  FilterBuilder builder;
  for (const scoped_refptr<SupervisedUserSiteList>& site_list : site_lists)
    builder.AddSiteList(site_list);

  return builder.Build();
}

}  // namespace

SupervisedUserURLFilter::SupervisedUserURLFilter()
    : default_behavior_(ALLOW),
      contents_(new Contents()),
      blacklist_(nullptr),
      blocking_task_runner_(
          BrowserThread::GetBlockingPool()
              ->GetTaskRunnerWithShutdownBehavior(
                  base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN).get()) {
  // Detach from the current thread so we can be constructed on a different
  // thread than the one where we're used.
  DetachFromThread();
}

SupervisedUserURLFilter::~SupervisedUserURLFilter() {
  DCHECK(CalledOnValidThread());
}

// static
SupervisedUserURLFilter::FilteringBehavior
SupervisedUserURLFilter::BehaviorFromInt(int behavior_value) {
  DCHECK_GE(behavior_value, ALLOW);
  DCHECK_LE(behavior_value, BLOCK);
  return static_cast<FilteringBehavior>(behavior_value);
}

// static
GURL SupervisedUserURLFilter::Normalize(const GURL& url) {
  GURL normalized_url = url;
  GURL::Replacements replacements;
  // Strip username, password, query, and ref.
  replacements.ClearUsername();
  replacements.ClearPassword();
  replacements.ClearQuery();
  replacements.ClearRef();
  return url.ReplaceComponents(replacements);
}

// static
bool SupervisedUserURLFilter::HasFilteredScheme(const GURL& url) {
  for (size_t i = 0; i < arraysize(kFilteredSchemes); ++i) {
    if (url.scheme() == kFilteredSchemes[i])
      return true;
  }
  return false;
}

// static
bool SupervisedUserURLFilter::HostMatchesPattern(const std::string& host,
                                                 const std::string& pattern) {
  std::string trimmed_pattern = pattern;
  std::string trimmed_host = host;
  if (base::EndsWith(pattern, ".*", base::CompareCase::SENSITIVE)) {
    size_t registry_length = GetRegistryLength(
        trimmed_host, EXCLUDE_UNKNOWN_REGISTRIES, EXCLUDE_PRIVATE_REGISTRIES);
    // A host without a known registry part does not match.
    if (registry_length == 0)
      return false;

    trimmed_pattern.erase(trimmed_pattern.length() - 2);
    trimmed_host.erase(trimmed_host.length() - (registry_length + 1));
  }

  if (base::StartsWith(trimmed_pattern, "*.", base::CompareCase::SENSITIVE)) {
    trimmed_pattern.erase(0, 2);

    // The remaining pattern should be non-empty, and it should not contain
    // further stars. Also the trimmed host needs to end with the trimmed
    // pattern.
    if (trimmed_pattern.empty() ||
        trimmed_pattern.find('*') != std::string::npos ||
        !base::EndsWith(trimmed_host, trimmed_pattern,
                        base::CompareCase::SENSITIVE)) {
      return false;
    }

    // The trimmed host needs to have a dot separating the subdomain from the
    // matched pattern piece, unless there is no subdomain.
    int pos = trimmed_host.length() - trimmed_pattern.length();
    DCHECK_GE(pos, 0);
    return (pos == 0) || (trimmed_host[pos - 1] == '.');
  }

  return trimmed_host == trimmed_pattern;
}

SupervisedUserURLFilter::FilteringBehavior
SupervisedUserURLFilter::GetFilteringBehaviorForURL(const GURL& url) const {
  supervised_user_error_page::FilteringBehaviorReason reason;
  return GetFilteringBehaviorForURL(url, false, &reason);
}

bool SupervisedUserURLFilter::GetManualFilteringBehaviorForURL(
    const GURL& url, FilteringBehavior* behavior) const {
  supervised_user_error_page::FilteringBehaviorReason reason;
  *behavior = GetFilteringBehaviorForURL(url, true, &reason);
  return reason == supervised_user_error_page::MANUAL;
}

SupervisedUserURLFilter::FilteringBehavior
SupervisedUserURLFilter::GetFilteringBehaviorForURL(
    const GURL& url,
    bool manual_only,
    supervised_user_error_page::FilteringBehaviorReason* reason) const {
  DCHECK(CalledOnValidThread());

  *reason = supervised_user_error_page::MANUAL;

  // URLs with a non-standard scheme (e.g. chrome://) are always allowed.
  if (!HasFilteredScheme(url))
    return ALLOW;

  // Check manual overrides for the exact URL.
  std::map<GURL, bool>::const_iterator url_it = url_map_.find(Normalize(url));
  if (url_it != url_map_.end())
    return url_it->second ? ALLOW : BLOCK;

  // Check manual overrides for the hostname.
  std::string host = url.host();
  std::map<std::string, bool>::const_iterator host_it = host_map_.find(host);
  if (host_it != host_map_.end())
    return host_it->second ? ALLOW : BLOCK;

  // Look for patterns matching the hostname, with a value that is different
  // from the default (a value of true in the map meaning allowed).
  for (const auto& host_entry : host_map_) {
    if ((host_entry.second == (default_behavior_ == BLOCK)) &&
        HostMatchesPattern(host, host_entry.first)) {
      return host_entry.second ? ALLOW : BLOCK;
    }
  }

  // Check the list of URL patterns.
  std::set<URLMatcherConditionSet::ID> matching_ids =
      contents_->url_matcher.MatchURL(url);

  if (!matching_ids.empty()) {
    *reason = supervised_user_error_page::WHITELIST;
    return ALLOW;
  }

  // Check the list of hostname hashes.
  if (contents_->hostname_hashes.count(HostnameHash(url.host()))) {
    *reason = supervised_user_error_page::WHITELIST;
    return ALLOW;
  }

  // Check the static blacklist, unless the default is to block anyway.
  if (!manual_only && default_behavior_ != BLOCK &&
      blacklist_ && blacklist_->HasURL(url)) {
    *reason = supervised_user_error_page::BLACKLIST;
    return BLOCK;
  }

  // Fall back to the default behavior.
  *reason = supervised_user_error_page::DEFAULT;
  return default_behavior_;
}

bool SupervisedUserURLFilter::GetFilteringBehaviorForURLWithAsyncChecks(
    const GURL& url,
    const FilteringBehaviorCallback& callback) const {
  supervised_user_error_page::FilteringBehaviorReason reason =
      supervised_user_error_page::DEFAULT;
  FilteringBehavior behavior = GetFilteringBehaviorForURL(url, false, &reason);
  // Any non-default reason trumps the async checker.
  // Also, if we're blocking anyway, then there's no need to check it.
  if (reason != supervised_user_error_page::DEFAULT || behavior == BLOCK ||
      !async_url_checker_) {
    callback.Run(behavior, reason, false);
    FOR_EACH_OBSERVER(Observer, observers_,
                      OnURLChecked(url, behavior, reason, false));
    return true;
  }

  return async_url_checker_->CheckURL(
      Normalize(url),
      base::Bind(&SupervisedUserURLFilter::CheckCallback,
                 base::Unretained(this),
                 callback));
}

std::map<std::string, base::string16>
SupervisedUserURLFilter::GetMatchingWhitelistTitles(const GURL& url) const {
  std::map<std::string, base::string16> whitelists;

  std::set<URLMatcherConditionSet::ID> matching_ids =
      contents_->url_matcher.MatchURL(url);

  for (const auto& matching_id : matching_ids) {
    const scoped_refptr<SupervisedUserSiteList>& site_list =
        contents_->site_lists_by_matcher_id[matching_id];
    whitelists[site_list->id()] = site_list->title();
  }

  // Add the site lists that match the URL hostname hash to the map of
  // whitelists (IDs -> titles).
  const auto& range =
      contents_->hostname_hashes.equal_range(HostnameHash(url.host()));
  for (auto it = range.first; it != range.second; ++it)
    whitelists[it->second->id()] = it->second->title();

  return whitelists;
}

void SupervisedUserURLFilter::SetDefaultFilteringBehavior(
    FilteringBehavior behavior) {
  DCHECK(CalledOnValidThread());
  default_behavior_ = behavior;
}

SupervisedUserURLFilter::FilteringBehavior
SupervisedUserURLFilter::GetDefaultFilteringBehavior() const {
  return default_behavior_;
}

void SupervisedUserURLFilter::LoadWhitelists(
    const std::vector<scoped_refptr<SupervisedUserSiteList>>& site_lists) {
  DCHECK(CalledOnValidThread());

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&LoadWhitelistsOnBlockingPoolThread, site_lists),
      base::Bind(&SupervisedUserURLFilter::SetContents, this));
}

void SupervisedUserURLFilter::SetBlacklist(
    const SupervisedUserBlacklist* blacklist) {
  blacklist_ = blacklist;
}

bool SupervisedUserURLFilter::HasBlacklist() const {
  return !!blacklist_;
}

void SupervisedUserURLFilter::SetFromPatternsForTesting(
    const std::vector<std::string>& patterns) {
  DCHECK(CalledOnValidThread());

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&CreateWhitelistFromPatternsForTesting, patterns),
      base::Bind(&SupervisedUserURLFilter::SetContents, this));
}

void SupervisedUserURLFilter::SetFromSiteListsForTesting(
    const std::vector<scoped_refptr<SupervisedUserSiteList>>& site_lists) {
  DCHECK(CalledOnValidThread());

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(), FROM_HERE,
      base::Bind(&CreateWhitelistsFromSiteListsForTesting, site_lists),
      base::Bind(&SupervisedUserURLFilter::SetContents, this));
}

void SupervisedUserURLFilter::SetManualHosts(
    const std::map<std::string, bool>* host_map) {
  DCHECK(CalledOnValidThread());
  host_map_ = *host_map;
}

void SupervisedUserURLFilter::SetManualURLs(
    const std::map<GURL, bool>* url_map) {
  DCHECK(CalledOnValidThread());
  url_map_ = *url_map;
}

void SupervisedUserURLFilter::InitAsyncURLChecker(
    net::URLRequestContextGetter* context) {
  async_url_checker_.reset(new SupervisedUserAsyncURLChecker(context));
}

void SupervisedUserURLFilter::ClearAsyncURLChecker() {
  async_url_checker_.reset();
}

bool SupervisedUserURLFilter::HasAsyncURLChecker() const {
  return !!async_url_checker_;
}

void SupervisedUserURLFilter::Clear() {
  default_behavior_ = ALLOW;
  SetContents(base::WrapUnique(new Contents()));
  url_map_.clear();
  host_map_.clear();
  blacklist_ = nullptr;
  async_url_checker_.reset();
}

void SupervisedUserURLFilter::AddObserver(Observer* observer) const {
  observers_.AddObserver(observer);
}

void SupervisedUserURLFilter::RemoveObserver(Observer* observer) const {
  observers_.RemoveObserver(observer);
}

void SupervisedUserURLFilter::SetBlockingTaskRunnerForTesting(
    const scoped_refptr<base::TaskRunner>& task_runner) {
  blocking_task_runner_ = task_runner;
}

void SupervisedUserURLFilter::SetContents(std::unique_ptr<Contents> contents) {
  DCHECK(CalledOnValidThread());
  contents_ = std::move(contents);
  FOR_EACH_OBSERVER(Observer, observers_, OnSiteListUpdated());
}

void SupervisedUserURLFilter::CheckCallback(
    const FilteringBehaviorCallback& callback,
    const GURL& url,
    FilteringBehavior behavior,
    bool uncertain) const {
  DCHECK(default_behavior_ != BLOCK);

  callback.Run(behavior, supervised_user_error_page::ASYNC_CHECKER, uncertain);
  FOR_EACH_OBSERVER(
      Observer, observers_,
      OnURLChecked(url, behavior, supervised_user_error_page::ASYNC_CHECKER,
                   uncertain));
}
