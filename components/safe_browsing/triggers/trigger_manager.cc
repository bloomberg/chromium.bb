// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/triggers/trigger_manager.h"

#include "components/prefs/pref_service.h"
#include "components/safe_browsing/base_ui_manager.h"
#include "components/safe_browsing/browser/threat_details.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/security_interstitials/content/unsafe_resource.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "net/url_request/url_request_context_getter.h"

namespace safe_browsing {

namespace {

bool TriggerNeedsScout(const TriggerType trigger_type) {
  switch (trigger_type) {
    case TriggerType::SECURITY_INTERSTITIAL:
      // Security interstitials only need legacy SBER opt-in.
      return false;
    case TriggerType::AD_SAMPLE:
      // Ad samples need Scout-level opt-in.
      return true;
    case TriggerType::GAIA_PASSWORD_REUSE:
      // Gaia password reuses only need legacy SBER opt-in.
      return false;
  }
  // By default, require Scout so we are more restrictive on data collection.
  return true;
}

bool TriggerNeedsOptInForCollection(const TriggerType trigger_type) {
  switch (trigger_type) {
    case TriggerType::SECURITY_INTERSTITIAL:
      // For security interstitials, users can change the opt-in while the
      // trigger runs, so collection can begin without opt-in.
      return false;
    case TriggerType::AD_SAMPLE:
      // Ad samples happen in the background so the user must already be opted
      // in before the trigger is allowed to run.
      return true;
    case TriggerType::GAIA_PASSWORD_REUSE:
      // For Gaia password reuses, it is unlikely for users to change opt-in
      // while the trigger runs, so we require opt-in for collection to avoid
      // overheads.
      return true;
  }
  // By default, require opt-in for all triggers.
  return true;
}

bool CanSendReport(const SBErrorOptions& error_display_options,
                   const TriggerType trigger_type) {
  // Some triggers require that users are eligible for elevated Scout data
  // collection in order to run.
  bool scout_check_ok = !TriggerNeedsScout(trigger_type) ||
                        error_display_options.is_scout_reporting_enabled;

  // Reports are only sent for non-incoginito users who are allowed to modify
  // the Extended Reporting setting and have opted-in to Extended Reporting.
  return !error_display_options.is_off_the_record &&
         error_display_options.is_extended_reporting_opt_in_allowed &&
         error_display_options.is_extended_reporting_enabled && scout_check_ok;
}

}  // namespace

DataCollectorsContainer::DataCollectorsContainer() {}
DataCollectorsContainer::~DataCollectorsContainer() {}

TriggerManager::TriggerManager(BaseUIManager* ui_manager)
    : ui_manager_(ui_manager), trigger_throttler_(new TriggerThrottler()) {}

TriggerManager::~TriggerManager() {}

void TriggerManager::set_trigger_throttler(TriggerThrottler* throttler) {
  trigger_throttler_.reset(throttler);
}

// static
SBErrorOptions TriggerManager::GetSBErrorDisplayOptions(
    const PrefService& pref_service,
    const content::WebContents& web_contents) {
  return SBErrorOptions(/*is_main_frame_load_blocked=*/false,
                        IsExtendedReportingOptInAllowed(pref_service),
                        web_contents.GetBrowserContext()->IsOffTheRecord(),
                        IsExtendedReportingEnabled(pref_service),
                        IsScout(pref_service),
                        /*is_proceed_anyway_disabled=*/false,
                        /*should_open_links_in_new_tab=*/false,
                        /*show_back_to_safety_button=*/true,
                        /*help_center_article_link=*/std::string());
}

bool TriggerManager::CanStartDataCollection(
    const SBErrorOptions& error_display_options,
    const TriggerType trigger_type) {
  // Some triggers require that the user be opted-in to extended reporting in
  // order to run, while others can run without opt-in (eg: because users are
  // prompted for opt-in as part of the trigger).
  bool optin_required_check_ok =
      !TriggerNeedsOptInForCollection(trigger_type) ||
      error_display_options.is_extended_reporting_enabled;

  // Some triggers require that users are eligible for elevated Scout data
  // collection in order to run.
  bool scout_check_ok = !TriggerNeedsScout(trigger_type) ||
                        error_display_options.is_scout_reporting_enabled;

  // We start data collection as long as user is not incognito and is able to
  // change the Extended Reporting opt-in, and the |trigger_type| has available
  // quota. For some triggers we also require Scout or extended reporting opt-in
  // in order to start data collection.
  return !error_display_options.is_off_the_record &&
         error_display_options.is_extended_reporting_opt_in_allowed &&
         optin_required_check_ok && scout_check_ok &&
         trigger_throttler_->TriggerCanFire(trigger_type);
}

bool TriggerManager::StartCollectingThreatDetails(
    const TriggerType trigger_type,
    content::WebContents* web_contents,
    const security_interstitials::UnsafeResource& resource,
    net::URLRequestContextGetter* request_context_getter,
    history::HistoryService* history_service,
    const SBErrorOptions& error_display_options) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!CanStartDataCollection(error_display_options, trigger_type))
    return false;

  // Ensure we're not already collecting data on this tab.
  // TODO(lpz): this check should be more specific to check that this type of
  // data collector is not running on this tab (once additional data collectors
  // exist).
  if (base::ContainsKey(data_collectors_map_, web_contents))
    return false;

  DataCollectorsContainer* collectors = &data_collectors_map_[web_contents];
  bool should_trim_threat_details = trigger_type == TriggerType::AD_SAMPLE;
  collectors->threat_details =
      scoped_refptr<ThreatDetails>(ThreatDetails::NewThreatDetails(
          ui_manager_, web_contents, resource, request_context_getter,
          history_service, should_trim_threat_details));
  return true;
}

bool TriggerManager::FinishCollectingThreatDetails(
    const TriggerType trigger_type,
    content::WebContents* web_contents,
    const base::TimeDelta& delay,
    bool did_proceed,
    int num_visits,
    const SBErrorOptions& error_display_options) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Make sure there's a data collector running on this tab.
  // TODO(lpz): this check should be more specific to check that the right type
  // of data collector is running on this tab (once additional data collectors
  // exist).
  if (!base::ContainsKey(data_collectors_map_, web_contents))
    return false;

  // Determine whether a report should be sent.
  bool should_send_report = CanSendReport(error_display_options, trigger_type);

  DataCollectorsContainer* collectors = &data_collectors_map_[web_contents];
  // Find the data collector and tell it to finish collecting data, and then
  // remove it from our map. We release ownership of the data collector here but
  // it will live until the end of the FinishCollection call because it
  // implements RefCountedThreadSafe.
  if (should_send_report) {
    scoped_refptr<ThreatDetails> threat_details = collectors->threat_details;
    content::BrowserThread::PostDelayedTask(
        content::BrowserThread::IO, FROM_HERE,
        base::BindOnce(&ThreatDetails::FinishCollection, threat_details,
                       did_proceed, num_visits),
        delay);

    // Record that this trigger fired and collected data.
    trigger_throttler_->TriggerFired(trigger_type);
  }

  // Regardless of whether the report got sent, clean up the data collector on
  // this tab.
  collectors->threat_details = nullptr;
  data_collectors_map_.erase(web_contents);

  return should_send_report;
}

}  // namespace safe_browsing
