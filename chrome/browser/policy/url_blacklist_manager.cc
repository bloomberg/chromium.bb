// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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

namespace policy {

namespace {

// Maximum filters per policy. Filters over this index are ignored.
const size_t kMaxFiltersPerPolicy = 100;

typedef std::vector<std::string> StringVector;

StringVector* ListValueToStringVector(const base::ListValue* list) {
  StringVector* vector = new StringVector;

  if (list == NULL)
    return vector;

  vector->reserve(list->GetSize());
  std::string s;
  for (base::ListValue::const_iterator it = list->begin();
       it != list->end() && vector->size() < kMaxFiltersPerPolicy; ++it) {
    if ((*it)->GetAsString(&s))
      vector->push_back(s);
  }

  return vector;
}

// A task that builds the blacklist on the FILE thread. Takes ownership
// of |block| and |allow| but not of |blacklist|.
void BuildBlacklist(URLBlacklist* blacklist,
                    StringVector* block,
                    StringVector* allow) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  scoped_ptr<StringVector> scoped_block(block);
  scoped_ptr<StringVector> scoped_allow(allow);

  for (StringVector::iterator it = block->begin(); it != block->end(); ++it) {
    blacklist->Block(*it);
  }
  for (StringVector::iterator it = allow->begin(); it != allow->end(); ++it) {
    blacklist->Allow(*it);
  }
}

// A task that owns the URLBlacklist, and passes it to the URLBlacklistManager
// on the IO thread, if the URLBlacklistManager still exists.
void SetBlacklistOnIO(base::WeakPtr<URLBlacklistManager> blacklist_manager,
                      URLBlacklist* blacklist) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (blacklist_manager) {
    blacklist_manager->SetBlacklist(blacklist);
  } else {
    delete blacklist;
  }
}

}  // namespace

struct URLBlacklist::PathFilter {
  explicit PathFilter(const std::string& path, uint16 port, bool match)
      : path_prefix(path),
        port(port),
        blocked_schemes(0),
        allowed_schemes(0),
        match_subdomains(match) {}

  std::string path_prefix;
  uint16 port;
  uint8 blocked_schemes;
  uint8 allowed_schemes;
  bool match_subdomains;
};

URLBlacklist::URLBlacklist() {
}

URLBlacklist::~URLBlacklist() {
  STLDeleteValues(&host_filters_);
}

void URLBlacklist::Block(const std::string& filter) {
  AddFilter(filter, true);
}

void URLBlacklist::Allow(const std::string& filter) {
  AddFilter(filter, false);
}

bool URLBlacklist::IsURLBlocked(const GURL& url) const {
  SchemeFlag flag = SCHEME_ALL;
  if (!SchemeToFlag(url.scheme(), &flag)) {
    // Not a scheme that can be filtered.
    return false;
  }

  std::string host(url.host());
  int int_port = url.EffectiveIntPort();
  const uint16 port = int_port > 0 ? int_port : 0;
  const std::string& path = url.path();

  // The first iteration through the loop will be an exact host match.
  // Subsequent iterations are subdomain matches, and some filters don't apply
  // to those.
  bool is_matching_subdomains = false;
  const bool host_is_ip = url.HostIsIPAddress();
  while (1) {
    HostFilterTable::const_iterator host_filter = host_filters_.find(host);
    if (host_filter != host_filters_.end()) {
      const PathFilterList* list = host_filter->second;
      size_t longest_length = 0;
      bool is_blocked = false;
      bool has_match = false;
      bool has_exact_host_match = false;
      for (PathFilterList::const_iterator it = list->begin();
           it != list->end(); ++it) {
        // Filters that apply to an exact hostname only take precedence over
        // filters that can apply to subdomains too.
        // E.g. ".google.com" filters take priority over "google.com".
        if (has_exact_host_match && it->match_subdomains)
          continue;

        // Skip if filter doesn't apply to subdomains, and this is a subdomain.
        if (is_matching_subdomains && !it->match_subdomains)
          continue;

        if (it->port != 0 && it->port != port)
          continue;

        // Skip if the filter doesn't apply to the scheme.
        if ((it->allowed_schemes & flag) == 0 &&
            (it->blocked_schemes & flag) == 0)
          continue;

        // If this match can't be longer than the current match, skip it.
        // For same size matches, the first rule to match takes precedence.
        // If this is an exact host match, it can be actually shorter than
        // a previous, non-exact match.
        if ((has_match && it->path_prefix.length() <= longest_length) &&
            (has_exact_host_match || it->match_subdomains)) {
          continue;
        }

        // Skip if the filter's |path_prefix| is not a prefix of |path|.
        if (path.compare(0, it->path_prefix.length(), it->path_prefix) != 0)
          continue;

        // This is the best match so far.
        has_match = true;
        has_exact_host_match = !it->match_subdomains;
        longest_length = it->path_prefix.length();
        // If both blocked and allowed bits are set, allowed takes precedence.
        is_blocked = !(it->allowed_schemes & flag);
      }
      // If a match was found, return its decision.
      if (has_match)
        return is_blocked;
    }

    // Quit after trying the empty string (corresponding to host '*').
    // Also skip subdomain matching for IP addresses.
    if (host.empty() || host_is_ip)
      break;

    // No match found for this host. Try a subdomain match, by removing the
    // leftmost subdomain from the hostname.
    is_matching_subdomains = true;
    size_t i = host.find('.');
    if (i != std::string::npos)
      ++i;
    host.erase(0, i);
  }

  // Default is to allow.
  return false;
}

// static
bool URLBlacklist::SchemeToFlag(const std::string& scheme, SchemeFlag* flag) {
  if (scheme.empty()) {
    *flag = SCHEME_ALL;
  } else if (scheme == "http") {
    *flag = SCHEME_HTTP;
  } else if (scheme == "https") {
    *flag = SCHEME_HTTPS;
  } else if (scheme == "ftp") {
    *flag = SCHEME_FTP;
  } else {
    return false;
  }
  return true;
}

// static
bool URLBlacklist::FilterToComponents(const std::string& filter,
                                   std::string* scheme,
                                   std::string* host,
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
  if (*host == "*")
    host->clear();

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

  return true;
}

void URLBlacklist::AddFilter(const std::string& filter, bool block) {
  std::string scheme;
  std::string host;
  uint16 port;
  std::string path;
  if (!FilterToComponents(filter, &scheme, &host, &port, &path)) {
    LOG(WARNING) << "Invalid filter, ignoring: " << filter;
    return;
  }

  SchemeFlag flag;
  if (!SchemeToFlag(scheme, &flag)) {
    LOG(WARNING) << "Unsupported scheme in filter, ignoring filter: " << filter;
    return;
  }

  bool match_subdomains = true;
  // Special syntax to disable subdomain matching.
  if (!host.empty() && host[0] == '.') {
    host.erase(0, 1);
    match_subdomains = false;
  }

  // Try to find an existing PathFilter with the same path prefix, port and
  // match_subdomains value.
  PathFilterList* list;
  HostFilterTable::iterator host_filter = host_filters_.find(host);
  if (host_filter == host_filters_.end()) {
    list = new PathFilterList;
    host_filters_[host] = list;
  } else {
    list = host_filter->second;
  }
  PathFilterList::iterator it;
  for (it = list->begin(); it != list->end(); ++it) {
    if (it->port == port && it->match_subdomains == match_subdomains &&
        it->path_prefix == path) {
      break;
    }
  }
  PathFilter* path_filter;
  if (it == list->end()) {
    list->push_back(PathFilter(path, port, match_subdomains));
    path_filter = &list->back();
  } else {
    path_filter = &(*it);
  }

  if (block) {
    path_filter->blocked_schemes |= flag;
  } else {
    path_filter->allowed_schemes |= flag;
  }
}

URLBlacklistManager::URLBlacklistManager(PrefService* pref_service)
    : ALLOW_THIS_IN_INITIALIZER_LIST(ui_weak_ptr_factory_(this)),
      pref_service_(pref_service),
      ALLOW_THIS_IN_INITIALIZER_LIST(io_weak_ptr_factory_(this)),
      blacklist_(new URLBlacklist) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(prefs::kUrlBlacklist, this);
  pref_change_registrar_.Add(prefs::kUrlWhitelist, this);

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

void URLBlacklistManager::Observe(int type,
                                  const content::NotificationSource& source,
                                  const content::NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(type == chrome::NOTIFICATION_PREF_CHANGED);
  PrefService* prefs = content::Source<PrefService>(source).ptr();
  DCHECK(prefs == pref_service_);
  std::string* pref_name = content::Details<std::string>(details).ptr();
  DCHECK(*pref_name == prefs::kUrlBlacklist ||
         *pref_name == prefs::kUrlWhitelist);
  ScheduleUpdate();
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
  StringVector* block =
      ListValueToStringVector(pref_service_->GetList(prefs::kUrlBlacklist));
  StringVector* allow =
      ListValueToStringVector(pref_service_->GetList(prefs::kUrlWhitelist));

  // Go through the IO thread to grab a WeakPtr to |this|. This is safe from
  // here, since this task will always execute before a potential deletion of
  // ProfileIOData on IO.
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&URLBlacklistManager::UpdateOnIO,
                                     base::Unretained(this), block, allow));
}

void URLBlacklistManager::UpdateOnIO(StringVector* block, StringVector* allow) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  URLBlacklist* blacklist = new URLBlacklist;
  // The URLBlacklist is built on the FILE thread. Once it's ready, it is passed
  // to the URLBlacklistManager on IO.
  // |blacklist|, |block| and |allow| can leak on the unlikely event of a
  // policy update and shutdown happening at the same time.
  BrowserThread::PostTaskAndReply(BrowserThread::FILE, FROM_HERE,
                                  base::Bind(&BuildBlacklist,
                                             blacklist, block, allow),
                                  base::Bind(&SetBlacklistOnIO,
                                             io_weak_ptr_factory_.GetWeakPtr(),
                                             blacklist));
}

void URLBlacklistManager::SetBlacklist(URLBlacklist* blacklist) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  blacklist_.reset(blacklist);
}

bool URLBlacklistManager::IsURLBlocked(const GURL& url) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return blacklist_->IsURLBlocked(url);
}

// static
void URLBlacklistManager::RegisterPrefs(PrefService* pref_service) {
  pref_service->RegisterListPref(prefs::kUrlBlacklist,
                                 PrefService::UNSYNCABLE_PREF);
  pref_service->RegisterListPref(prefs::kUrlWhitelist,
                                 PrefService::UNSYNCABLE_PREF);
}

}  // namespace policy
