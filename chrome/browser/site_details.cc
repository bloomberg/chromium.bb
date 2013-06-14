// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/site_details.h"

#include "base/metrics/histogram.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"

using content::BrowserThread;
using content::RenderProcessHost;
using content::SiteInstance;
using content::WebContents;

SiteData::SiteData() {}

SiteData::~SiteData() {}

SiteDetails::SiteDetails() {}

SiteDetails::~SiteDetails() {}

void SiteDetails::CollectSiteInfo(WebContents* contents,
                                  SiteData* site_data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  content::BrowserContext* browser_context = contents->GetBrowserContext();

  // Find the BrowsingInstance this WebContents belongs to by iterating over
  // the "primary" SiteInstances of each BrowsingInstance we've seen so far.
  SiteInstance* instance = contents->GetSiteInstance();
  SiteInstance* primary = NULL;
  for (size_t i = 0; i < site_data->instances.size(); ++i) {
    if (instance->IsRelatedSiteInstance(site_data->instances[i])) {
      primary = site_data->instances[i];
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
  std::set<GURL> sites_in_tab = contents->GetSitesInTab();
  for (std::set<GURL>::iterator iter = sites_in_tab.begin();
       iter != sites_in_tab.end(); ++iter) {
    // Skip about:blank, since we won't usually give it its own process.
    // Because about:blank has no host, its site URL will be blank.
    if (iter->is_empty())
      continue;

    // Make sure we don't overcount process-per-site sites, like the NTP.
    if (RenderProcessHost::ShouldUseProcessPerSite(browser_context, *iter) &&
        site_data->sites.find(*iter) != site_data->sites.end()) {
      continue;
    }

    site_data->sites.insert(*iter);
    site_data->instance_site_map[primary->GetId()].insert(*iter);

    // Also keep track of how things would look if we only isolated HTTPS sites.
    // In this model, all HTTP sites are grouped into one "http://" site.  HTTPS
    // and other schemes (e.g., chrome:) are still isolated.
    GURL https_site = iter->SchemeIs("http") ? GURL("http://") : *iter;
    site_data->https_sites.insert(https_site);
    site_data->instance_https_site_map[primary->GetId()].insert(https_site);
  }
}

void SiteDetails::UpdateHistograms(
    const BrowserContextSiteDataMap& site_data_map,
    int all_renderer_process_count,
    int non_renderer_process_count) {
  // Reports a set of site-based process metrics to UMA.
  int process_limit = RenderProcessHost::GetMaxRendererProcessCount();

  // Sum the number of sites and SiteInstances in each BrowserContext.
  int num_sites = 0;
  int num_https_sites = 0;
  int num_browsing_instances = 0;
  int num_isolated_site_instances = 0;
  int num_isolated_https_site_instances = 0;
  for (BrowserContextSiteDataMap::const_iterator i = site_data_map.begin();
       i != site_data_map.end(); ++i) {
    num_sites += i->second.sites.size();
    num_https_sites += i->second.https_sites.size();
    num_browsing_instances += i->second.instance_site_map.size();
    for (BrowsingInstanceSiteMap::const_iterator iter =
             i->second.instance_site_map.begin();
         iter != i->second.instance_site_map.end(); ++iter) {
      num_isolated_site_instances += iter->second.size();
    }
    for (BrowsingInstanceSiteMap::const_iterator iter =
             i->second.instance_https_site_map.begin();
         iter != i->second.instance_https_site_map.end(); ++iter) {
      num_isolated_https_site_instances += iter->second.size();
    }
  }

  // Predict the number of processes needed when isolating all sites and when
  // isolating only HTTPS sites.
  int process_count_lower_bound = num_sites;
  int process_count_upper_bound = num_sites + process_limit - 1;
  int process_count_estimate = std::min(
      num_isolated_site_instances, process_count_upper_bound);

  int process_count_https_lower_bound = num_https_sites;
  int process_count_https_upper_bound = num_https_sites + process_limit - 1;
  int process_count_https_estimate = std::min(
      num_isolated_https_site_instances, process_count_https_upper_bound);

  // Just renderer process count:
  UMA_HISTOGRAM_COUNTS_100("SiteIsolation.CurrentRendererProcessCount",
                           all_renderer_process_count);
  UMA_HISTOGRAM_COUNTS_100(
      "SiteIsolation.BrowsingInstanceCount",
      num_browsing_instances);
  UMA_HISTOGRAM_COUNTS_100(
      "SiteIsolation.IsolateAllSitesProcessCountNoLimit",
      num_isolated_site_instances);
  UMA_HISTOGRAM_COUNTS_100(
      "SiteIsolation.IsolateAllSitesProcessCountLowerBound",
      process_count_lower_bound);
  UMA_HISTOGRAM_COUNTS_100(
      "SiteIsolation.IsolateAllSitesProcessCountEstimate",
      process_count_estimate);
  UMA_HISTOGRAM_COUNTS_100(
      "SiteIsolation.IsolateHttpsSitesProcessCountNoLimit",
      num_isolated_https_site_instances);
  UMA_HISTOGRAM_COUNTS_100(
      "SiteIsolation.IsolateHttpsSitesProcessCountLowerBound",
      process_count_https_lower_bound);
  UMA_HISTOGRAM_COUNTS_100(
      "SiteIsolation.IsolateHttpsSitesProcessCountEstimate",
      process_count_https_estimate);

  // Total process count:
  UMA_HISTOGRAM_COUNTS_100(
      "SiteIsolation.IsolateAllSitesTotalProcessCountEstimate",
      process_count_estimate + non_renderer_process_count);
  UMA_HISTOGRAM_COUNTS_100(
      "SiteIsolation.IsolateHttpsSitesTotalProcessCountEstimate",
      process_count_https_estimate + non_renderer_process_count);
}
