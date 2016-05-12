// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_HOSTED_APPS_COUNTER_H_
#define CHROME_BROWSER_BROWSING_DATA_HOSTED_APPS_COUNTER_H_

#include <vector>

#include "base/macros.h"
#include "chrome/browser/browsing_data/browsing_data_counter.h"

// A BrowsingDataCounter that returns the number of hosted apps and names
// of up to two of them as examples.
class HostedAppsCounter: public BrowsingDataCounter {
 public:
  class HostedAppsResult : public FinishedResult {
   public:
    HostedAppsResult(const HostedAppsCounter* source,
                     ResultInt num_apps,
                     const std::vector<std::string>& examples);
    ~HostedAppsResult() override;

    const std::vector<std::string>& examples() const { return examples_; }

   private:
    const std::vector<std::string> examples_;

    DISALLOW_COPY_AND_ASSIGN(HostedAppsResult);
  };

  HostedAppsCounter();
  ~HostedAppsCounter() override;

  // BrowsingDataCounter:
  const std::string& GetPrefName() const override;

 private:
  // BrowsingDataCounter:
  void Count() override;

  const std::string pref_name_;

  DISALLOW_COPY_AND_ASSIGN(HostedAppsCounter);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_HOSTED_APPS_COUNTER_H_
