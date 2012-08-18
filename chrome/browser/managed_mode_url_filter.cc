// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/managed_mode_url_filter.h"

#include "base/file_path.h"
#include "base/json/json_file_value_serializer.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/policy/url_blacklist_manager.h"
#include "chrome/common/extensions/matcher/url_matcher.h"
#include "content/public/browser/browser_thread.h"
#include "googleurl/src/gurl.h"

using content::BrowserThread;
using extensions::URLMatcher;
using extensions::URLMatcherConditionSet;

namespace {

scoped_ptr<URLMatcher> CreateWhitelistOnBlockingPoolThread(
    const std::vector<std::string>& patterns) {
  DCHECK(BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());

  scoped_ptr<URLMatcher> url_matcher(new URLMatcher());
  URLMatcherConditionSet::Vector all_conditions;
  URLMatcherConditionSet::ID id = 0;
  for (std::vector<std::string>::const_iterator it = patterns.begin();
       it != patterns.end(); ++it) {
    std::string scheme;
    std::string host;
    uint16 port;
    std::string path;
    bool match_subdomains = true;
    if (!policy::URLBlacklist::FilterToComponents(
            *it, &scheme, &host, &match_subdomains, &port, &path)) {
      LOG(ERROR) << "Invalid pattern " << *it;
      continue;
    }

    scoped_refptr<extensions::URLMatcherConditionSet> condition_set =
        policy::URLBlacklist::CreateConditionSet(
            url_matcher.get(), ++id,
            scheme, host, match_subdomains, port, path);
    all_conditions.push_back(condition_set);
  }
  url_matcher->AddConditionSets(all_conditions);
  return url_matcher.Pass();
}

}  // namespace

ManagedModeURLFilter::ManagedModeURLFilter()
    : ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)),
      active_(false),
      url_matcher_(new URLMatcher()) {
  // Detach from the current thread so we can be constructed on a different
  // thread than the one where we're used.
  DetachFromThread();
}

ManagedModeURLFilter::~ManagedModeURLFilter() {
  DCHECK(CalledOnValidThread());
}

bool ManagedModeURLFilter::IsURLWhitelisted(const GURL& url) const {
  DCHECK(CalledOnValidThread());

  if (!active_)
    return true;

#if defined(ENABLE_CONFIGURATION_POLICY)
  if (!policy::URLBlacklist::HasStandardScheme(url))
    return true;
#endif

  std::set<URLMatcherConditionSet::ID> matching_ids =
      url_matcher_->MatchURL(url);
  return !matching_ids.empty();
}

void ManagedModeURLFilter::SetActive(bool active) {
  DCHECK(CalledOnValidThread());
  active_ = active;
}

void ManagedModeURLFilter::SetWhitelist(
    const std::vector<std::string>& patterns,
    const base::Closure& continuation) {
  DCHECK(CalledOnValidThread());

  base::PostTaskAndReplyWithResult(
      BrowserThread::GetBlockingPool(),
      FROM_HERE,
      base::Bind(&CreateWhitelistOnBlockingPoolThread, patterns),
      base::Bind(&ManagedModeURLFilter::SetURLMatcher,
                 weak_ptr_factory_.GetWeakPtr(), continuation));
}

void ManagedModeURLFilter::SetURLMatcher(const base::Closure& continuation,
                                         scoped_ptr<URLMatcher> url_matcher) {
  DCHECK(CalledOnValidThread());
  url_matcher_ = url_matcher.Pass();
  continuation.Run();
}
