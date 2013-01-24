// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/url_blacklist_manager.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "googleurl/src/gurl.h"

using content::BrowserThread;
using extensions::URLMatcher;
using extensions::URLMatcherCondition;
using extensions::URLMatcherConditionFactory;
using extensions::URLMatcherConditionSet;
using extensions::URLMatcherPortFilter;
using extensions::URLMatcherSchemeFilter;

namespace policy {

namespace {

// Maximum filters per policy. Filters over this index are ignored.
const size_t kMaxFiltersPerPolicy = 1000;

const char* kStandardSchemes[] = {
  "http",
  "https",
  "file",
  "ftp",
  "gopher",
  "ws",
  "wss"
};

bool IsStandardScheme(const std::string& scheme) {
  for (size_t i = 0; i < arraysize(kStandardSchemes); ++i) {
    if (scheme == kStandardSchemes[i])
      return true;
  }
  return false;
}

// A task that builds the blacklist on the FILE thread.
scoped_ptr<URLBlacklist> BuildBlacklist(scoped_ptr<base::ListValue> block,
                                        scoped_ptr<base::ListValue> allow) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  scoped_ptr<URLBlacklist> blacklist(new URLBlacklist);
  blacklist->Block(block.get());
  blacklist->Allow(allow.get());
  return blacklist.Pass();
}

}  // namespace

struct URLBlacklist::FilterComponents {
  FilterComponents() : port(0), match_subdomains(true), allow(true) {}
  ~FilterComponents() {}

  std::string scheme;
  std::string host;
  uint16 port;
  std::string path;
  bool match_subdomains;
  bool allow;
};

URLBlacklist::URLBlacklist() : id_(0),
                               url_matcher_(new URLMatcher) {
}

URLBlacklist::~URLBlacklist() {
}

void URLBlacklist::AddFilters(bool allow,
                              const base::ListValue* list) {
  URLMatcherConditionSet::Vector all_conditions;
  size_t size = std::min(kMaxFiltersPerPolicy, list->GetSize());
  for (size_t i = 0; i < size; ++i) {
    std::string pattern;
    bool success = list->GetString(i, &pattern);
    DCHECK(success);
    FilterComponents components;
    components.allow = allow;
    if (!FilterToComponents(pattern, &components.scheme, &components.host,
                            &components.match_subdomains, &components.port,
                            &components.path)) {
      LOG(ERROR) << "Invalid pattern " << pattern;
      continue;
    }

    all_conditions.push_back(
        CreateConditionSet(url_matcher_.get(), ++id_, components.scheme,
                           components.host, components.match_subdomains,
                           components.port, components.path));
    filters_[id_] = components;
  }
  url_matcher_->AddConditionSets(all_conditions);
}

void URLBlacklist::Block(const base::ListValue* filters) {
  AddFilters(false, filters);
}

void URLBlacklist::Allow(const base::ListValue* filters) {
  AddFilters(true, filters);
}

bool URLBlacklist::IsURLBlocked(const GURL& url) const {
  if (!HasStandardScheme(url))
    return false;

  std::set<URLMatcherConditionSet::ID> matching_ids =
      url_matcher_->MatchURL(url);

  const FilterComponents* max = NULL;
  for (std::set<URLMatcherConditionSet::ID>::iterator id = matching_ids.begin();
       id != matching_ids.end(); ++id) {
    std::map<int, FilterComponents>::const_iterator it = filters_.find(*id);
    DCHECK(it != filters_.end());
    const FilterComponents& filter = it->second;
    if (!max || FilterTakesPrecedence(filter, *max))
      max = &filter;
  }

  // Default to allow.
  if (!max)
    return false;

  return !max->allow;
}

// static
bool URLBlacklist::HasStandardScheme(const GURL& url) {
  return IsStandardScheme(url.scheme());
}

// static
bool URLBlacklist::FilterToComponents(const std::string& filter,
                                      std::string* scheme,
                                      std::string* host,
                                      bool* match_subdomains,
                                      uint16* port,
                                      std::string* path) {
  url_parse::Parsed parsed;
  URLFixerUpper::SegmentURL(filter, &parsed);

  if (!parsed.host.is_nonempty())
    return false;

  if (parsed.scheme.is_nonempty())
    scheme->assign(filter, parsed.scheme.begin, parsed.scheme.len);
  else
    scheme->clear();

  host->assign(filter, parsed.host.begin, parsed.host.len);
  // Special '*' host, matches all hosts.
  if (*host == "*") {
    host->clear();
    *match_subdomains = true;
  } else if ((*host)[0] == '.') {
    // A leading dot in the pattern syntax means that we don't want to match
    // subdomains.
    host->erase(0, 1);
    *match_subdomains = false;
  } else {
    url_canon::RawCanonOutputT<char> output;
    url_canon::CanonHostInfo host_info;
    url_canon::CanonicalizeHostVerbose(filter.c_str(), parsed.host,
                                       &output, &host_info);
    if (host_info.family == url_canon::CanonHostInfo::NEUTRAL) {
      // We want to match subdomains. Add a dot in front to make sure we only
      // match at domain component boundaries.
      *host = "." + *host;
      *match_subdomains = true;
    } else {
      *match_subdomains = false;
    }
  }

  if (parsed.port.is_nonempty()) {
    int int_port;
    if (!base::StringToInt(filter.substr(parsed.port.begin, parsed.port.len),
                           &int_port)) {
      return false;
    }
    if (int_port <= 0 || int_port > kuint16max)
      return false;
    *port = int_port;
  } else {
    // Match any port.
    *port = 0;
  }

  if (parsed.path.is_nonempty())
    path->assign(filter, parsed.path.begin, parsed.path.len);
  else
    path->clear();

  if (!scheme->empty() && !IsStandardScheme(*scheme))
    return false;

  return true;
}

// static
scoped_refptr<extensions::URLMatcherConditionSet>
URLBlacklist::CreateConditionSet(
    extensions::URLMatcher* url_matcher,
    int id,
    const std::string& scheme,
    const std::string& host,
    bool match_subdomains,
    uint16 port,
    const std::string& path) {
  URLMatcherConditionFactory* condition_factory =
      url_matcher->condition_factory();
  std::set<URLMatcherCondition> conditions;
  conditions.insert(match_subdomains ?
      condition_factory->CreateHostSuffixPathPrefixCondition(host, path) :
      condition_factory->CreateHostEqualsPathPrefixCondition(host, path));

  scoped_ptr<URLMatcherSchemeFilter> scheme_filter;
  if (!scheme.empty())
    scheme_filter.reset(new URLMatcherSchemeFilter(scheme));

  scoped_ptr<URLMatcherPortFilter> port_filter;
  if (port != 0) {
    std::vector<URLMatcherPortFilter::Range> ranges;
    ranges.push_back(URLMatcherPortFilter::CreateRange(port));
    port_filter.reset(new URLMatcherPortFilter(ranges));
  }

  return new URLMatcherConditionSet(id, conditions,
                                    scheme_filter.Pass(), port_filter.Pass());
}

// static
bool URLBlacklist::FilterTakesPrecedence(const FilterComponents& lhs,
                                         const FilterComponents& rhs) {
  if (lhs.match_subdomains && !rhs.match_subdomains)
    return false;
  if (!lhs.match_subdomains && rhs.match_subdomains)
    return true;

  size_t host_length = lhs.host.length();
  size_t other_host_length = rhs.host.length();
  if (host_length != other_host_length)
    return host_length > other_host_length;

  size_t path_length = lhs.path.length();
  size_t other_path_length = rhs.path.length();
  if (path_length != other_path_length)
    return path_length > other_path_length;

  if (lhs.allow && !rhs.allow)
    return true;

  return false;
}

URLBlacklistManager::URLBlacklistManager(PrefService* pref_service)
    : ALLOW_THIS_IN_INITIALIZER_LIST(ui_weak_ptr_factory_(this)),
      pref_service_(pref_service),
      ALLOW_THIS_IN_INITIALIZER_LIST(io_weak_ptr_factory_(this)),
      blacklist_(new URLBlacklist) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  pref_change_registrar_.Init(pref_service_);
  base::Closure callback = base::Bind(&URLBlacklistManager::ScheduleUpdate,
                                      base::Unretained(this));
  pref_change_registrar_.Add(prefs::kUrlBlacklist, callback);
  pref_change_registrar_.Add(prefs::kUrlWhitelist, callback);

  // Start enforcing the policies without a delay when they are present at
  // startup.
  if (pref_service_->HasPrefPath(prefs::kUrlBlacklist))
    Update();
}

void URLBlacklistManager::ShutdownOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Cancel any pending updates, and stop listening for pref change updates.
  ui_weak_ptr_factory_.InvalidateWeakPtrs();
  pref_change_registrar_.RemoveAll();
}

URLBlacklistManager::~URLBlacklistManager() {
}

void URLBlacklistManager::ScheduleUpdate() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Cancel pending updates, if any. This can happen if two preferences that
  // change the blacklist are updated in one message loop cycle. In those cases,
  // only rebuild the blacklist after all the preference updates are processed.
  ui_weak_ptr_factory_.InvalidateWeakPtrs();
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&URLBlacklistManager::Update,
                 ui_weak_ptr_factory_.GetWeakPtr()));
}

void URLBlacklistManager::Update() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // The preferences can only be read on the UI thread.
  scoped_ptr<base::ListValue> block(
      pref_service_->GetList(prefs::kUrlBlacklist)->DeepCopy());
  scoped_ptr<base::ListValue> allow(
      pref_service_->GetList(prefs::kUrlWhitelist)->DeepCopy());

  // Go through the IO thread to grab a WeakPtr to |this|. This is safe from
  // here, since this task will always execute before a potential deletion of
  // ProfileIOData on IO.
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&URLBlacklistManager::UpdateOnIO,
                                     base::Unretained(this),
                                     base::Passed(&block),
                                     base::Passed(&allow)));
}

void URLBlacklistManager::UpdateOnIO(scoped_ptr<base::ListValue> block,
                                     scoped_ptr<base::ListValue> allow) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // The URLBlacklist is built on the FILE thread. Once it's ready, it is passed
  // to the URLBlacklistManager on IO.
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&BuildBlacklist,
                 base::Passed(&block),
                 base::Passed(&allow)),
      base::Bind(&URLBlacklistManager::SetBlacklist,
                 io_weak_ptr_factory_.GetWeakPtr()));
}

void URLBlacklistManager::SetBlacklist(scoped_ptr<URLBlacklist> blacklist) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  blacklist_ = blacklist.Pass();
}

bool URLBlacklistManager::IsURLBlocked(const GURL& url) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return blacklist_->IsURLBlocked(url);
}

// static
void URLBlacklistManager::RegisterUserPrefs(PrefServiceSyncable* pref_service) {
  pref_service->RegisterListPref(prefs::kUrlBlacklist,
                                 PrefServiceSyncable::UNSYNCABLE_PREF);
  pref_service->RegisterListPref(prefs::kUrlWhitelist,
                                 PrefServiceSyncable::UNSYNCABLE_PREF);
}

}  // namespace policy
