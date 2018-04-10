// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_TRIGGERS_SUSPICIOUS_SITE_TRIGGER_H_
#define COMPONENTS_SAFE_BROWSING_TRIGGERS_SUSPICIOUS_SITE_TRIGGER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class PrefService;

namespace history {
class HistoryService;
}

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace safe_browsing {
class TriggerManager;

// This class watches tab-level events such as the start and end of a page
// load, and also listens for events from the SuspiciousSiteURLThrottle that
// indicate there was a hit on the suspicious site list. This trigger is
// repsonsible for creating reports about the page at the right time, based on
// the sequence of such events.
class SuspiciousSiteTrigger
    : public content::WebContentsObserver,
      public content::WebContentsUserData<SuspiciousSiteTrigger> {
 public:
  ~SuspiciousSiteTrigger() override;

  static void CreateForWebContents(
      content::WebContents* web_contents,
      TriggerManager* trigger_manager,
      PrefService* prefs,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      history::HistoryService* history_service);

  // content::WebContentsObserver implementations.
  void DidStartLoading() override;
  void DidStopLoading() override;

  // Called when a suspicious site has been detected on the tab that this
  // trigger is running on.
  void SuspiciousSiteDetected();

 private:
  friend class content::WebContentsUserData<SuspiciousSiteTrigger>;
  friend class SuspiciousSiteTriggerTest;

  // The different states the trigger could be in.
  enum class TriggerState {
    // Trigger is idle, page is not loading, no report requested.
    IDLE,
    // Page load has started, no report requested.
    LOADING,
    // Page load has started and a report is requested. The report will be
    // created when the page load finishes.
    LOADING_WILL_REPORT,
    // A page load finished and a report for the page has started.
    REPORT_STARTED,
  };

  SuspiciousSiteTrigger(
      content::WebContents* web_contents,
      TriggerManager* trigger_manager,
      PrefService* prefs,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      history::HistoryService* history_service);

  // Tries to start a report. Returns whether a report started successfully.
  // If a report is started, a delayed callback will also begin to notify
  // the trigger when the report should be completed and sent.
  bool MaybeStartReport();

  // Calls into the trigger manager to finish the active report and send it.
  void FinishReport();

  // Called when the report delay timer fires, indicating that the active
  // report should be completed and sent.
  void ReportDelayTimerFired();

  // Sets a task runner to use for tests.
  void SetTaskRunnerForTest(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // The delay (in milliseconds) to wait before finishing a report. Can be
  // overwritten for tests.
  int64_t finish_report_delay_ms_;

  // Current state of the trigger. Used to synchronize page load events with
  // suspicious site list hit events so that reports can be generated at the
  // right time.
  TriggerState current_state_;

  // TriggerManager gets called if this trigger detects a suspicious site and
  // wants to collect data abou tit. Not owned.
  TriggerManager* trigger_manager_;

  PrefService* prefs_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  history::HistoryService* history_service_;

  // Task runner for posting delayed tasks. Normally set to the runner for the
  // UI thread, but can be overwritten for tests.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  base::WeakPtrFactory<SuspiciousSiteTrigger> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SuspiciousSiteTrigger);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_TRIGGERS_SUSPICIOUS_SITE_TRIGGER_H_
