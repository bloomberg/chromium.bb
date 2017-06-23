// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_TRIGGERS_TRIGGER_MANAGER_H_
#define COMPONENTS_SAFE_BROWSING_TRIGGERS_TRIGGER_MANAGER_H_

#include <unordered_map>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/security_interstitials/content/unsafe_resource.h"
#include "components/security_interstitials/core/base_safe_browsing_error_ui.h"
#include "content/public/browser/web_contents.h"

class PrefService;

namespace history {
class HistoryService;
}

namespace net {
class URLRequestContextGetter;
}

namespace safe_browsing {

class BaseUIManager;
class ThreatDetails;

// Stores the data collectors that are active on each WebContents (ie: browser
// tab).
// TODO(lpz): This will be a multi-level map in the future so different
// collectors can be active on the same tab.
using DataCollectorsMap =
    std::unordered_map<content::WebContents*, scoped_refptr<ThreatDetails>>;

using SBErrorOptions =
    security_interstitials::BaseSafeBrowsingErrorUI::SBErrorDisplayOptions;

// This class manages SafeBrowsing data-reporting triggers. Triggers are
// activated for users opted-in to Extended Reporting and when security-related
// data collection is required.
//
// The TriggerManager has two main responsibilities: 1) ensuring triggers only
// run when appropriate, by honouring user opt-ins and incognito state, and 2)
// tracking how often triggers fire and throttling them when necessary.
class TriggerManager {
 public:
  TriggerManager(BaseUIManager* ui_manager);
  ~TriggerManager();

  // Returns a SBErrorDisplayOptions struct containing user state that is
  // relevant for TriggerManager to decide whether to start/finish data
  // collection. Looks at incognito state from |web_contents|, and opt-ins from
  // |pref_service|. Only the fields needed by TriggerManager will be set.
  static SBErrorOptions GetSBErrorDisplayOptions(
      const PrefService& pref_service,
      const content::WebContents& web_contents);

  // Begins collecting a ThreatDetails report on the specified |web_contents|.
  // |resource| is the unsafe resource that cause the collection to occur.
  // |request_context_getter| is used to retrieve data from the HTTP cache.
  // |history_service| is used to get data about redirects.
  // |error_display_options| contains the current state of relevant user
  // preferences. We use this object for interop with WebView, in Chrome it
  // should be created by TriggerManager::GetSBErrorDisplayOptions().
  // Returns true if the collection began, or false if it didn't.
  bool StartCollectingThreatDetails(
      content::WebContents* web_contents,
      const security_interstitials::UnsafeResource& resource,
      net::URLRequestContextGetter* request_context_getter,
      history::HistoryService* history_service,
      const SBErrorOptions& error_display_options);

  // Completes the collection of a ThreatDetails report on the specified
  // |web_contents| and sends the report. |delay| can be used to wait a period
  // of time before finishing the report. |did_proceed| indicates whether the
  // user proceeded through the security interstitial associated with this
  // report. |num_visits| is how many times the user has visited the site
  // before. |error_display_options| contains the current state of relevant user
  // preferences. We use this object for interop with WebView, in Chrome it
  // should be created by TriggerManager::GetSBErrorDisplayOptions().
  // Returns true if the report was completed and sent, or false otherwise (eg:
  // the user was not opted-in to extended reporting after collection began).
  bool FinishCollectingThreatDetails(
      content::WebContents* web_contents,
      const base::TimeDelta& delay,
      bool did_proceed,
      int num_visits,
      const SBErrorOptions& error_display_options);

 private:
  friend class TriggerManagerTest;

  // The UI manager is used to send reports to Google. Not owned.
  // TODO(lpz): we may only need a the PingManager here.
  BaseUIManager* ui_manager_;

  // Map of the data collectors running on each tabs.
  DataCollectorsMap data_collectors_map_;

  DISALLOW_COPY_AND_ASSIGN(TriggerManager);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_TRIGGERS_TRIGGER_MANAGER_H_
