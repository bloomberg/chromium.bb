// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_top_host_provider_impl.h"

#include "base/metrics/histogram_macros.h"
#include "base/values.h"
#include "chrome/browser/engagement/site_engagement_details.mojom.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/optimization_guide/optimization_guide_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/previews/content/previews_hints_util.h"
#include "content/public/browser/browser_thread.h"

namespace {

bool IsHostBlacklisted(const base::DictionaryValue& top_host_blacklist,
                       const std::string& host) {
  return top_host_blacklist.FindKey(previews::HashHostForDictionary(host));
}

}  // namespace

namespace previews {

PreviewsTopHostProviderImpl::PreviewsTopHostProviderImpl(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context),
      pref_service_(Profile::FromBrowserContext(browser_context_)->GetPrefs()) {
}

PreviewsTopHostProviderImpl::~PreviewsTopHostProviderImpl() {}

std::vector<std::string> PreviewsTopHostProviderImpl::GetTopHosts(
    size_t max_sites) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(browser_context_);
  DCHECK(pref_service_);

  std::vector<std::string> top_hosts;
  top_hosts.reserve(max_sites);

  // Create SiteEngagementService to request site engagement scores.
  Profile* profile = Profile::FromBrowserContext(browser_context_);
  SiteEngagementService* engagement_service =
      SiteEngagementService::Get(profile);

  const base::DictionaryValue* top_host_blacklist =
      pref_service_->GetDictionary(
          optimization_guide::prefs::kHintsFetcherTopHostBlacklist);

  // Create a vector of the top hosts by engagement score up to |max_sites|
  // size. Currently utilizes just the first |max_sites| entries. Only HTTPS
  // schemed hosts are included. Hosts are filtered by the blacklist that is
  // populated when DataSaver is first enabled.
  std::vector<mojom::SiteEngagementDetails> engagement_details =
      engagement_service->GetAllDetails();
  for (const auto& detail : engagement_details) {
    if (top_hosts.size() >= max_sites)
      return top_hosts;
    // TODO(b/968542): Skip origins that are local hosts (e.g., IP addresses,
    // localhost:8080 etc.).
    if (detail.origin.SchemeIs(url::kHttpsScheme) &&
        !IsHostBlacklisted(*top_host_blacklist, detail.origin.host())) {
      top_hosts.push_back(detail.origin.host());
    }
  }

  return top_hosts;
}

}  // namespace previews
