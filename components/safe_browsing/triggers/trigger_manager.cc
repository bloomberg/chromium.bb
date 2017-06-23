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

bool CanStartDataCollection(const SBErrorOptions& error_display_options) {
  // We start data collection as long as user is not incognito and is able to
  // change the Extended Reporting opt-in. We don't require them to be opted-in
  // to SBER to begin collecting data, since they may be able to change the
  // setting while data collection is running (eg: on a security interstitial).
  return !error_display_options.is_off_the_record &&
         error_display_options.is_extended_reporting_opt_in_allowed;
}

bool CanSendReport(const SBErrorOptions& error_display_options) {
  // Reports are only sent for non-incoginito users who are allowed to modify
  // the Extended Reporting setting and have opted-in to Extended Reporting.
  return !error_display_options.is_off_the_record &&
         error_display_options.is_extended_reporting_opt_in_allowed &&
         error_display_options.is_extended_reporting_enabled;
}

}  // namespace

TriggerManager::TriggerManager(BaseUIManager* ui_manager)
    : ui_manager_(ui_manager) {}

TriggerManager::~TriggerManager() {}

// static
SBErrorOptions TriggerManager::GetSBErrorDisplayOptions(
    const PrefService& pref_service,
    const content::WebContents& web_contents) {
  return SBErrorOptions(/*is_main_frame_load_blocked=*/false,
                        IsExtendedReportingOptInAllowed(pref_service),
                        web_contents.GetBrowserContext()->IsOffTheRecord(),
                        IsExtendedReportingEnabled(pref_service),
                        /*is_scout_reporting_enabled=*/false,
                        /*is_proceed_anyway_disabled=*/false,
                        /*help_center_article_link=*/std::string());
}

bool TriggerManager::StartCollectingThreatDetails(
    content::WebContents* web_contents,
    const security_interstitials::UnsafeResource& resource,
    net::URLRequestContextGetter* request_context_getter,
    history::HistoryService* history_service,
    const SBErrorOptions& error_display_options) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!CanStartDataCollection(error_display_options))
    return false;

  // Ensure we're not already collecting data on this tab.
  if (base::ContainsKey(data_collectors_map_, web_contents))
    return false;

  data_collectors_map_[web_contents] = scoped_refptr<ThreatDetails>(
      ThreatDetails::NewThreatDetails(ui_manager_, web_contents, resource,
                                      request_context_getter, history_service));
  return true;
}

bool TriggerManager::FinishCollectingThreatDetails(
    content::WebContents* web_contents,
    const base::TimeDelta& delay,
    bool did_proceed,
    int num_visits,
    const SBErrorOptions& error_display_options) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Make sure there's a data collector running on this tab.
  if (!base::ContainsKey(data_collectors_map_, web_contents))
    return false;

  // Determine whether a report should be sent.
  bool should_send_report = CanSendReport(error_display_options);

  // Find the data collector and tell it to finish collecting data, and then
  // remove it from our map. We release ownership of the data collector here but
  // it will live until the end of the FinishCollection call because it
  // implements RefCountedThreadSafe.
  if (should_send_report) {
    scoped_refptr<ThreatDetails> threat_details =
        data_collectors_map_[web_contents];
    content::BrowserThread::PostDelayedTask(
        content::BrowserThread::IO, FROM_HERE,
        base::BindOnce(&ThreatDetails::FinishCollection, threat_details,
                       did_proceed, num_visits),
        delay);
  }

  // Regardless of whether the report got sent, clean up the data collector on
  // this tab.
  data_collectors_map_.erase(web_contents);

  return should_send_report;
}

}  // namespace safe_browsing
