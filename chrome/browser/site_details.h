// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SITE_DETAILS_H_
#define CHROME_BROWSER_SITE_DETAILS_H_

#include "base/containers/hash_tables.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"

// Maps an ID representing each BrowsingInstance to a set of site URLs.
typedef base::hash_map<int32, std::set<GURL>> BrowsingInstanceSiteMap;

// This enum represents various alternative process model policies that we want
// to evaluate. We'll estimate the process cost of each scenario.
enum IsolationScenarioType {
  ISOLATE_NOTHING,
  ISOLATE_ALL_SITES,
  ISOLATE_HTTPS_SITES,
  ISOLATE_EXTENSIONS,
  ISOLATION_SCENARIO_LAST = ISOLATE_EXTENSIONS
};

// Contains the state required to estimate the process count under a particular
// process model. We have one of these per IsolationScenarioType.
struct IsolationScenario {
  IsolationScenario();
  ~IsolationScenario();

  IsolationScenarioType policy;
  std::set<GURL> sites;
  BrowsingInstanceSiteMap browsing_instance_site_map;
};

// Information about the sites and SiteInstances in each BrowsingInstance, for
// use in estimating the number of processes needed for various process models.
struct SiteData {
  SiteData();
  ~SiteData();

  // One IsolationScenario object per IsolationScenarioType.
  IsolationScenario scenarios[ISOLATION_SCENARIO_LAST + 1];

  // Global list of all SiteInstances, used for de-duping related instances.
  std::vector<content::SiteInstance*> instances;
};

// Maps a BrowserContext to information about the sites it contains.
typedef base::hash_map<content::BrowserContext*, SiteData>
    BrowserContextSiteDataMap;

class SiteDetails {
 public:
  // Collect information about all committed sites in the given WebContents
  // on the UI thread.
  static void CollectSiteInfo(content::WebContents* contents,
                              SiteData* site_data);

  // Updates the global histograms for tracking memory usage.
  static void UpdateHistograms(const BrowserContextSiteDataMap& site_data_map,
                               int all_renderer_process_count,
                               int non_renderer_process_count);

 private:
  // Never needs to be constructed.
  SiteDetails();
  ~SiteDetails();

  DISALLOW_COPY_AND_ASSIGN(SiteDetails);
};

#endif  // CHROME_BROWSER_SITE_DETAILS_H_
