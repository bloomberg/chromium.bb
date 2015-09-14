// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/site_details.h"

#include "base/metrics/histogram.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/common/constants.h"

using content::BrowserThread;
using content::RenderProcessHost;
using content::SiteInstance;
using content::WebContents;

namespace {

bool ShouldIsolate(IsolationScenarioType policy, const GURL& site) {
  switch (policy) {
    case ISOLATE_ALL_SITES:
      return true;
    case ISOLATE_HTTPS_SITES:
      // Note: For estimation purposes "isolate https sites" is really
      // implemented as "isolate non-http sites". This means that, for example,
      // the New Tab Page gets counted as two processes under this policy, and
      // extensions are isolated as well.
      return !site.SchemeIs(url::kHttpScheme);
    case ISOLATE_EXTENSIONS:
      return site.SchemeIs(extensions::kExtensionScheme);
  }
  NOTREACHED();
  return true;
}

}  // namespace

IsolationScenario::IsolationScenario() : policy(ISOLATE_ALL_SITES) {}

IsolationScenario::~IsolationScenario() {}

void IsolationScenario::CollectSiteInfoForScenario(SiteInstance* primary,
                                                   const GURL& site) {
  const GURL& isolated = ShouldIsolate(policy, site) ? site : GURL("http://");
  sites.insert(isolated);
  browsing_instance_site_map[primary->GetId()].insert(isolated);
}

SiteData::SiteData() {
  scenarios[ISOLATE_ALL_SITES].policy = ISOLATE_ALL_SITES;
  scenarios[ISOLATE_HTTPS_SITES].policy = ISOLATE_HTTPS_SITES;
  scenarios[ISOLATE_EXTENSIONS].policy = ISOLATE_EXTENSIONS;
}

SiteData::~SiteData() {}

SiteDetails::SiteDetails() {}

SiteDetails::~SiteDetails() {}

void SiteDetails::CollectSiteInfo(WebContents* contents,
                                  SiteData* site_data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::BrowserContext* browser_context = contents->GetBrowserContext();

  // Find the BrowsingInstance this WebContents belongs to by iterating over
  // the "primary" SiteInstances of each BrowsingInstance we've seen so far.
  SiteInstance* instance = contents->GetSiteInstance();
  SiteInstance* primary = NULL;
  for (SiteInstance* already_collected_instance : site_data->instances) {
    if (instance->IsRelatedSiteInstance(already_collected_instance)) {
      primary = already_collected_instance;
      break;
    }
  }
  if (!primary) {
    // Remember this as the "primary" SiteInstance of a new BrowsingInstance.
    primary = instance;
    site_data->instances.push_back(instance);
  }

  // Now keep track of how many sites we have in this BrowsingInstance (and
  // overall), including sites in iframes.
  for (const GURL& site : contents->GetSitesInTab()) {
    // Make sure we don't overcount process-per-site sites, like the NTP or
    // extensions, by skipping over them if they're already logged for
    // ISOLATE_ALL_SITES.
    if (RenderProcessHost::ShouldUseProcessPerSite(browser_context, site) &&
        site_data->scenarios[ISOLATE_ALL_SITES].sites.find(site) !=
            site_data->scenarios[ISOLATE_ALL_SITES].sites.end()) {
      continue;
    }

    for (IsolationScenario& scenario : site_data->scenarios) {
      scenario.CollectSiteInfoForScenario(primary, site);
    }
  }
}

void SiteDetails::UpdateHistograms(
    const BrowserContextSiteDataMap& site_data_map,
    int all_renderer_process_count,
    int non_renderer_process_count) {
  // Reports a set of site-based process metrics to UMA.
  int process_limit = RenderProcessHost::GetMaxRendererProcessCount();

  // Sum the number of sites and SiteInstances in each BrowserContext.
  int num_sites[ISOLATION_SCENARIO_LAST + 1] = {};
  int num_isolated_site_instances[ISOLATION_SCENARIO_LAST + 1] = {};
  int num_browsing_instances = 0;
  for (BrowserContextSiteDataMap::const_iterator i = site_data_map.begin();
       i != site_data_map.end(); ++i) {
    for (const IsolationScenario& scenario : i->second.scenarios) {
      num_sites[scenario.policy] += scenario.sites.size();
      for (auto& browsing_instance : scenario.browsing_instance_site_map) {
        num_isolated_site_instances[scenario.policy] +=
            browsing_instance.second.size();
      }
    }
    num_browsing_instances += i->second.scenarios[ISOLATE_ALL_SITES]
                                  .browsing_instance_site_map.size();
  }

  // Predict the number of processes needed when isolating all sites, when
  // isolating only HTTPS sites, and when isolating extensions.
  int process_count_lower_bound[ISOLATION_SCENARIO_LAST + 1];
  int process_count_upper_bound[ISOLATION_SCENARIO_LAST + 1];
  int process_count_estimate[ISOLATION_SCENARIO_LAST + 1];
  for (int policy = 0; policy <= ISOLATION_SCENARIO_LAST; policy++) {
    process_count_lower_bound[policy] = num_sites[policy];
    process_count_upper_bound[policy] = num_sites[policy] + process_limit - 1;
    process_count_estimate[policy] = std::min(
        num_isolated_site_instances[policy], process_count_upper_bound[policy]);
  }

  // Just renderer process count:
  UMA_HISTOGRAM_COUNTS_100("SiteIsolation.CurrentRendererProcessCount",
                           all_renderer_process_count);
  UMA_HISTOGRAM_COUNTS_100(
      "SiteIsolation.BrowsingInstanceCount",
      num_browsing_instances);
  UMA_HISTOGRAM_COUNTS_100("SiteIsolation.IsolateAllSitesProcessCountNoLimit",
                           num_isolated_site_instances[ISOLATE_ALL_SITES]);
  UMA_HISTOGRAM_COUNTS_100(
      "SiteIsolation.IsolateAllSitesProcessCountLowerBound",
      process_count_lower_bound[ISOLATE_ALL_SITES]);
  UMA_HISTOGRAM_COUNTS_100("SiteIsolation.IsolateAllSitesProcessCountEstimate",
                           process_count_estimate[ISOLATE_ALL_SITES]);

  UMA_HISTOGRAM_COUNTS_100("SiteIsolation.IsolateHttpsSitesProcessCountNoLimit",
                           num_isolated_site_instances[ISOLATE_HTTPS_SITES]);
  UMA_HISTOGRAM_COUNTS_100(
      "SiteIsolation.IsolateHttpsSitesProcessCountLowerBound",
      process_count_lower_bound[ISOLATE_HTTPS_SITES]);
  UMA_HISTOGRAM_COUNTS_100(
      "SiteIsolation.IsolateHttpsSitesProcessCountEstimate",
      process_count_estimate[ISOLATE_HTTPS_SITES]);

  UMA_HISTOGRAM_COUNTS_100("SiteIsolation.IsolateExtensionsProcessCountNoLimit",
                           num_isolated_site_instances[ISOLATE_EXTENSIONS]);
  UMA_HISTOGRAM_COUNTS_100(
      "SiteIsolation.IsolateExtensionsProcessCountLowerBound",
      process_count_lower_bound[ISOLATE_EXTENSIONS]);
  UMA_HISTOGRAM_COUNTS_100(
      "SiteIsolation.IsolateExtensionsProcessCountEstimate",
      process_count_estimate[ISOLATE_EXTENSIONS]);

  // Total process count:
  UMA_HISTOGRAM_COUNTS_100(
      "SiteIsolation.IsolateAllSitesTotalProcessCountEstimate",
      process_count_estimate[ISOLATE_ALL_SITES] + non_renderer_process_count);
  UMA_HISTOGRAM_COUNTS_100(
      "SiteIsolation.IsolateHttpsSitesTotalProcessCountEstimate",
      process_count_estimate[ISOLATE_HTTPS_SITES] + non_renderer_process_count);
  UMA_HISTOGRAM_COUNTS_100(
      "SiteIsolation.IsolateExtensionsTotalProcessCountEstimate",
      process_count_estimate[ISOLATE_EXTENSIONS] + non_renderer_process_count);
}
