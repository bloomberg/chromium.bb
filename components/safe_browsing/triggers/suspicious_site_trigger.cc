// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/triggers/suspicious_site_trigger.h"

#include "components/history/core/browser/history_service.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/triggers/trigger_manager.h"
#include "components/safe_browsing/triggers/trigger_throttler.h"
#include "components/security_interstitials/content/unsafe_resource.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(safe_browsing::SuspiciousSiteTrigger);

namespace safe_browsing {

namespace {
// Number of milliseconds to allow data collection to run before sending a
// report (since this trigger runs in the background).
const int64_t kSuspiciousSiteCollectionPeriodMilliseconds = 5000;
}  // namespace

SuspiciousSiteTrigger::SuspiciousSiteTrigger(
    content::WebContents* web_contents,
    TriggerManager* trigger_manager,
    PrefService* prefs,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    history::HistoryService* history_service)
    : content::WebContentsObserver(web_contents),
      finish_report_delay_ms_(kSuspiciousSiteCollectionPeriodMilliseconds),
      current_state_(TriggerState::IDLE),
      trigger_manager_(trigger_manager),
      prefs_(prefs),
      url_loader_factory_(url_loader_factory),
      history_service_(history_service),
      task_runner_(content::BrowserThread::GetTaskRunnerForThread(
          content::BrowserThread::UI)),
      weak_ptr_factory_(this) {}

SuspiciousSiteTrigger::~SuspiciousSiteTrigger() {}

// static
void SuspiciousSiteTrigger::CreateForWebContents(
    content::WebContents* web_contents,
    TriggerManager* trigger_manager,
    PrefService* prefs,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    history::HistoryService* history_service) {
  if (!FromWebContents(web_contents)) {
    web_contents->SetUserData(UserDataKey(),
                              base::WrapUnique(new SuspiciousSiteTrigger(
                                  web_contents, trigger_manager, prefs,
                                  url_loader_factory, history_service)));
  }
}

bool SuspiciousSiteTrigger::MaybeStartReport() {
  SBErrorOptions error_options =
      TriggerManager::GetSBErrorDisplayOptions(*prefs_, *web_contents());

  security_interstitials::UnsafeResource resource;
  resource.threat_type = SB_THREAT_TYPE_SUSPICIOUS_SITE;
  resource.url = web_contents()->GetURL();
  resource.web_contents_getter = resource.GetWebContentsGetter(
      web_contents()->GetMainFrame()->GetProcess()->GetID(),
      web_contents()->GetMainFrame()->GetRoutingID());

  if (!trigger_manager_->StartCollectingThreatDetails(
          TriggerType::SUSPICIOUS_SITE, web_contents(), resource,
          url_loader_factory_, history_service_, error_options)) {
    return false;
  }

  // Call back into the trigger after a short delay, allowing the report
  // to complete.
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&SuspiciousSiteTrigger::FinishReport,
                     weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(finish_report_delay_ms_));

  return true;
}

void SuspiciousSiteTrigger::FinishReport() {
  SBErrorOptions error_options =
      TriggerManager::GetSBErrorDisplayOptions(*prefs_, *web_contents());
  trigger_manager_->FinishCollectingThreatDetails(
      TriggerType::SUSPICIOUS_SITE, web_contents(), base::TimeDelta(),
      /*did_proceed=*/false, /*num_visits=*/0, error_options);
}

void SuspiciousSiteTrigger::DidStartLoading() {
  switch (current_state_) {
    case TriggerState::IDLE:
      // Load started, move to loading state.
      current_state_ = TriggerState::LOADING;
      return;

    case TriggerState::LOADING:
      // No-op, still loading.
      return;

    case TriggerState::LOADING_WILL_REPORT:
      // This happens if the user leaves the suspicious page before it
      // finishes loading. A report can't be created in this case since the
      // page is now gone.
      current_state_ = TriggerState::LOADING;
      return;

    case TriggerState::REPORT_STARTED:
      // A new page load has started while creating the current report.
      // Finish the report immediately with whatever data has been captured
      // so far. A report timer will have already started, but it will be
      // ignored when it fires.
      current_state_ = TriggerState::LOADING;
      FinishReport();
      return;
  }
}

void SuspiciousSiteTrigger::DidStopLoading() {
  switch (current_state_) {
    case TriggerState::IDLE:
      // No-op, load stopped and we're already idle.
      return;

    case TriggerState::LOADING:
      // Load finished, return to Idle state.
      current_state_ = TriggerState::IDLE;
      return;

    case TriggerState::LOADING_WILL_REPORT:
      // Suspicious site detected mid-load and the page has now
      // finished loading, so try starting a report now.
      // If we fail to start a report for whatever reason, return to Idle.
      if (MaybeStartReport()) {
        current_state_ = TriggerState::REPORT_STARTED;
      } else {
        current_state_ = TriggerState::IDLE;
      }
      return;

    case TriggerState::REPORT_STARTED:
      // No-op. Let the report continue running.
      return;
  }
}

void SuspiciousSiteTrigger::SuspiciousSiteDetected() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  switch (current_state_) {
    case TriggerState::IDLE:
      // Suspicious site detected while idle, start a report immediately.
      // If we fail to start a report for whatever reason, remain Idle.
      if (MaybeStartReport()) {
        current_state_ = TriggerState::REPORT_STARTED;
      }
      return;

    case TriggerState::LOADING:
      // Suspicious site detected in the middle of the load, remember this
      // and let the page finish loading. The report will be started after
      // the page has loaded.
      current_state_ = TriggerState::LOADING_WILL_REPORT;
      return;

    case TriggerState::LOADING_WILL_REPORT:
      // No-op. Current page has multiple suspicious URLs in it, remain in
      // the LOADING_WILL_REPORT state. A report will begin when the page
      // finishes loading.
      return;

    case TriggerState::REPORT_STARTED:
      // No-op. The current report should capture all suspicious sites.
      return;
  }
}

void SuspiciousSiteTrigger::ReportDelayTimerFired() {
  switch (current_state_) {
    case TriggerState::IDLE:
    case TriggerState::LOADING:
    case TriggerState::LOADING_WILL_REPORT:
      // Invalid, expecting to be in REPORT_STARTED state.
      return;

    case TriggerState::REPORT_STARTED:
      // The delay timer has fired so complete the current report.
      current_state_ = TriggerState::IDLE;
      FinishReport();
      return;
  }
}

void SuspiciousSiteTrigger::SetTaskRunnerForTest(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  task_runner_ = task_runner;
}

}  // namespace safe_browsing
