// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_TRIGGERS_TRIGGER_MANAGER_H_
#define COMPONENTS_SAFE_BROWSING_TRIGGERS_TRIGGER_MANAGER_H_

#include <unordered_map>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/safe_browsing/triggers/trigger_throttler.h"
#include "components/security_interstitials/content/unsafe_resource.h"
#include "components/security_interstitials/core/base_safe_browsing_error_ui.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

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

// A wrapper around different kinds of data collectors that can be active on a
// given browser tab. Any given field can be null or empty if the associated
// data is not being collected.
struct DataCollectorsContainer {
 public:
  DataCollectorsContainer();
  ~DataCollectorsContainer();

  // Note: new data collection types should be added below as additional fields.

  // Collects ThreatDetails which contains resource URLs and partial DOM.
  scoped_refptr<ThreatDetails> threat_details;

 private:
  DISALLOW_COPY_AND_ASSIGN(DataCollectorsContainer);
};

// Stores the data collectors that are active on each WebContents (ie: browser
// tab).
using DataCollectorsMap =
    std::unordered_map<content::WebContents*, DataCollectorsContainer>;

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
  virtual ~TriggerManager();

  // Returns a SBErrorDisplayOptions struct containing user state that is
  // relevant for TriggerManager to decide whether to start/finish data
  // collection. Looks at incognito state from |web_contents|, and opt-ins from
  // |pref_service|. Only the fields needed by TriggerManager will be set.
  static SBErrorOptions GetSBErrorDisplayOptions(
      const PrefService& pref_service,
      const content::WebContents& web_contents);

  // Returns whether data collection can be started for the |trigger_type| based
  // on the settings specified in |error_display_options| as well as quota.
  bool CanStartDataCollection(const SBErrorOptions& error_display_options,
                              const TriggerType trigger_type);

  // Begins collecting a ThreatDetails report on the specified |web_contents|.
  // |resource| is the unsafe resource that cause the collection to occur.
  // |request_context_getter| is used to retrieve data from the HTTP cache.
  // |history_service| is used to get data about redirects.
  // |error_display_options| contains the current state of relevant user
  // preferences. We use this object for interop with WebView, in Chrome it
  // should be created by TriggerManager::GetSBErrorDisplayOptions().
  // Returns true if the collection began, or false if it didn't.
  virtual bool StartCollectingThreatDetails(
      TriggerType trigger_type,
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
  virtual bool FinishCollectingThreatDetails(
      TriggerType trigger_type,
      content::WebContents* web_contents,
      const base::TimeDelta& delay,
      bool did_proceed,
      int num_visits,
      const SBErrorOptions& error_display_options);

  // Called when a ThreatDetails report finishes for the specified
  // |web_contents|.
  void ThreatDetailsDone(content::WebContents* web_contents);

  // Called when the specified |web_contents| is being destroyed. Used to clean
  // up our map.
  void WebContentsDestroyed(content::WebContents* web_contents);

 private:
  friend class TriggerManagerTest;

  // For testing only - allows injecting a mock Throttler.
  void set_trigger_throttler(TriggerThrottler* throttler);

  // The UI manager is used to send reports to Google. Not owned.
  // TODO(lpz): we may only need a the PingManager here.
  BaseUIManager* ui_manager_;

  // Map of the data collectors running on each tabs. New keys are added the
  // first time any trigger tries to collect data on a tab and are removed when
  // the tab is destroyed. The values can be null if a trigger has finished on
  // a tab but the tab remains open.
  DataCollectorsMap data_collectors_map_;

  // Keeps track of how often triggers fire and throttles them when needed.
  std::unique_ptr<TriggerThrottler> trigger_throttler_;

  base::WeakPtrFactory<TriggerManager> weak_factory_;
  // WeakPtrFactory should be last, don't add any members below it.
  DISALLOW_COPY_AND_ASSIGN(TriggerManager);
};

// A helper class that listens for events happening on a WebContents and can
// notify TriggerManager of any that are relevant.
class TriggerManagerWebContentsHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<TriggerManagerWebContentsHelper> {
 public:
  ~TriggerManagerWebContentsHelper() override;

  // Creates a TriggerManagerWebContentsHelper and scopes its lifetime to the
  // specified |web_contents|.
  static void CreateForWebContents(content::WebContents* web_contents,
                                   TriggerManager* trigger_manager);

  // WebContentsObserver implementation.
  void WebContentsDestroyed() override;

 private:
  friend class content::WebContentsUserData<TriggerManagerWebContentsHelper>;

  TriggerManagerWebContentsHelper(content::WebContents* web_contents,
                                  TriggerManager* trigger_manager);

  // Trigger Manager will be notified of any relevant WebContents events.
  TriggerManager* trigger_manager_;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_TRIGGERS_TRIGGER_MANAGER_H_
