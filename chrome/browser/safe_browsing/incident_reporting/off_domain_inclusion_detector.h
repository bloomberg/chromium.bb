// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_OFF_DOMAIN_INCLUSION_DETECTOR_H_
#define CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_OFF_DOMAIN_INCLUSION_DETECTOR_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "content/public/common/resource_type.h"

class GURL;
class Profile;

namespace base {
class SingleThreadTaskRunner;
}

namespace net {
class URLRequest;
}

namespace safe_browsing {

class SafeBrowsingDatabaseManager;

// Observes network requests and reports suspicious activity.
class OffDomainInclusionDetector {
 public:
  enum class AnalysisEvent {
    NO_EVENT,
    ABORT_EMPTY_MAIN_FRAME_URL,
    ABORT_NO_PROFILE,
    ABORT_INCOGNITO,
    ABORT_NO_HISTORY_SERVICE,
    ABORT_HISTORY_LOOKUP_FAILED,
    OFF_DOMAIN_INCLUSION_WHITELISTED,
    OFF_DOMAIN_INCLUSION_IN_HISTORY,
    OFF_DOMAIN_INCLUSION_SUSPICIOUS,
  };

  // TODO(gab): Hook the OffDomainInclusionDetector to the
  // IncidentReportingService and use an IncidentReceiver instead of this custom
  // callback type.
  typedef base::Callback<void(AnalysisEvent event)> ReportAnalysisEventCallback;

  // Constructs an OffDomainInclusionDetector which will use |database_manager|
  // over the course of its lifetime. The OffDomainInclusionDetector will use
  // the thread it is constructed on as its main thread going forward.
  explicit OffDomainInclusionDetector(
      const scoped_refptr<SafeBrowsingDatabaseManager>& database_manager);

  // Constructs an OffDomainInclusionDetector as above with an additional
  // ReportAnalysisEventCallback to get feedback from detection events, used
  // only in tests.
  OffDomainInclusionDetector(
      const scoped_refptr<SafeBrowsingDatabaseManager>& database_manager,
      const ReportAnalysisEventCallback& report_analysis_event_callback);

  virtual ~OffDomainInclusionDetector();

  // Begins the asynchronous analysis of |request| for off-domain inclusions.
  // Thread safe.
  void OnResourceRequest(const net::URLRequest* request);

 protected:
  // Returns a pointer to the Profile hosting the frame identified by
  // |render_process_id| or null if the resolution can't be completed (e.g., the
  // tab/renderer is gone by now). Must only be called on the UI thread. Virtual
  // so tests can override it.
  virtual Profile* ProfileFromRenderProcessId(int render_process_id);

 private:
  // Returns true if |request| should be analyzed for off-domain inclusions.
  static bool ShouldAnalyzeRequest(const net::URLRequest* request);

  // Queries the HistoryService for |request_url|. Must be called from the UI
  // thread so that it can call ProfileFromRenderProcessId.
  void ContinueAnalysisWithHistoryCheck(
      const content::ResourceType resource_type,
      const GURL& request_url,
      int render_process_id);

  // Reports an off-domain inclusion including the HistoryService's result.
  void ContinueAnalysisOnHistoryResult(
      const content::ResourceType resource_type,
      bool success,
      int num_visits,
      base::Time first_visit_time);

  // Reports the result of an off-domain inclusion analysis via UMA (as well as
  // via |report_analysis_event_callback_| if it is set). May be called from any
  // thread at any point in the above analysis;
  // |report_analysis_event_callback_| will always be called on the main thread.
  void ReportAnalysisResult(
      const content::ResourceType resource_type,
      AnalysisEvent analysis_event);

  scoped_refptr<SafeBrowsingDatabaseManager> database_manager_;

  const ReportAnalysisEventCallback report_analysis_event_callback_;

  // Tracks pending tasks posted to the HistoryService and cancels them if this
  // class is destroyed before they complete.
  base::CancelableTaskTracker history_task_tracker_;

  // This class may post tasks to its main thread (construction thread) via
  // |main_thread_task_runner_|, such tasks should be weakly bound via WeakPtrs
  // coming from |weak_ptr_factory_| (and conversely |weak_ptr_factory_|
  // shouldn't be used for tasks that aren't on the main thread).
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  base::WeakPtrFactory<OffDomainInclusionDetector> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(OffDomainInclusionDetector);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_OFF_DOMAIN_INCLUSION_DETECTOR_H_
