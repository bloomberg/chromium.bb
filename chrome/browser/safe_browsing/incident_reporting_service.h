// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_SERVICE_H_
#define CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_SERVICE_H_

#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/safe_browsing/add_incident_callback.h"
#include "chrome/browser/safe_browsing/incident_report_uploader.h"

class SafeBrowsingDatabaseManager;
class SafeBrowsingService;
class TrackedPreferenceValidationDelegate;

namespace base {
class TaskRunner;
}

namespace net {
class URLRequestContextGetter;
}

namespace safe_browsing {

class ClientIncidentReport;
class ClientIncidentReport_DownloadDetails;
class ClientIncidentReport_EnvironmentData;
class ClientIncidentReport_IncidentData;

// A class that manages the collection of incidents and submission of incident
// reports to the safe browsing client-side detection service. The service
// begins operation when an incident is reported via the AddIncident method.
// Following this, the service collects environmental data and waits a bit.
// Additional incidents that arrive during this time are collated with the
// initial incident. Finally, already-reported incidents are pruned and any
// remaining are uploaded in an incident report.
class IncidentReportingService {
 public:
  IncidentReportingService(SafeBrowsingService* safe_browsing_service,
                           const scoped_refptr<net::URLRequestContextGetter>&
                               request_context_getter);
  virtual ~IncidentReportingService();

  // Enables or disables the service. When disabling, incident or data
  // collection in progress is dropped.
  void SetEnabled(bool enabled);

  // Returns a callback by which external components can add an incident to the
  // service.
  AddIncidentCallback GetAddIncidentCallback();

  // Returns a preference validation delegate that adds incidents to the service
  // for validation failures.
  scoped_ptr<TrackedPreferenceValidationDelegate>
      CreatePreferenceValidationDelegate();

 protected:
  // A pointer to a function that populates a protobuf with environment data.
  typedef void (*CollectEnvironmentDataFn)(
      ClientIncidentReport_EnvironmentData*);

  // Sets the function called by the service to collect environment data and the
  // task runner on which it is called. Used by unit tests to provide a fake
  // environment data collector.
  void SetCollectEnvironmentHook(
      CollectEnvironmentDataFn collect_environment_data_hook,
      const scoped_refptr<base::TaskRunner>& task_runner);

  // Initiates an upload. Overridden by unit tests to provide a fake uploader.
  virtual scoped_ptr<IncidentReportUploader> StartReportUpload(
      const IncidentReportUploader::OnResultCallback& callback,
      const scoped_refptr<net::URLRequestContextGetter>& request_context_getter,
      const ClientIncidentReport& report);

 private:
  class UploadContext;

  // Adds |incident_data| to the service. The incident_time_msec field is
  // populated with the current time if the caller has not already done so.
  void AddIncident(scoped_ptr<ClientIncidentReport_IncidentData> incident_data);

  // Starts a task to collect environment data in the blocking pool.
  void BeginEnvironmentCollection();

  // Cancels any pending environment collection task and drops any data that has
  // already been collected.
  void CancelEnvironmentCollection();

  // A callback invoked on the UI thread when environment data collection is
  // complete. Incident report processing continues, either by waiting for the
  // collection timeout or by sending an incident report.
  void OnEnvironmentDataCollected(
      scoped_ptr<ClientIncidentReport_EnvironmentData> environment_data);

  // Cancels the collection timeout.
  void CancelIncidentCollection();

  // A callback invoked on the UI thread after which incident collection has
  // completed. Incident report processing continues, either by waiting for
  // environment data to arrive or by sending an incident report.
  void OnCollectionTimeout();

  // Populates |download_details| with information about the most recent
  // download.
  void CollectDownloadDetails(
      ClientIncidentReport_DownloadDetails* download_details);

  // Record the incidents that were reported for future pruning.
  void RecordReportedIncidents();

  // Prunes incidents that have previously been reported.
  void PruneReportedIncidents(ClientIncidentReport* report);

  // Uploads an incident report if all data collection is complete.
  void UploadIfCollectionComplete();

  // Cancels all uploads, discarding all reports and responses in progress.
  void CancelAllReportUploads();

  // Continues an upload after checking for the CSD whitelist killswitch.
  void OnKillSwitchResult(UploadContext* context, bool is_killswitch_on);

  // Performs processing for a report after succesfully receiving a response.
  void HandleResponse(scoped_ptr<ClientIncidentReport> report,
                      scoped_ptr<ClientIncidentResponse> response);

  // IncidentReportUploader::OnResultCallback implementation.
  void OnReportUploadResult(UploadContext* context,
                            IncidentReportUploader::Result result,
                            scoped_ptr<ClientIncidentResponse> response);

  base::ThreadChecker thread_checker_;

  // The safe browsing database manager, through which the whitelist killswitch
  // is checked.
  scoped_refptr<SafeBrowsingDatabaseManager> database_manager_;

  // Accessor for an URL context with which reports will be sent.
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;

  // A pointer to a function that collects environment data. The function will
  // be run by |environment_collection_task_runner_|. This is ordinarily
  // CollectEnvironmentData, but may be overridden by tests; see
  // SetCollectEnvironmentHook.
  CollectEnvironmentDataFn collect_environment_data_fn_;

  // The task runner on which environment collection takes place. This is
  // ordinarily a runner in the browser's blocking pool that will skip the
  // collection task at shutdown if it has not yet started.
  scoped_refptr<base::TaskRunner> environment_collection_task_runner_;

  // True when the service has been enabled.
  bool enabled_;

  // True when the asynchronous environment collection task has been fired off
  // but has not yet completed.
  bool environment_collection_pending_;

  // True when an incident has been received and the service is waiting for the
  // upload_timer_ to fire.
  bool collection_timeout_pending_;

  // A timer upon the firing of which the service will report received
  // incidents.
  base::DelayTimer<IncidentReportingService> upload_timer_;

  // The report currently being assembled. This becomes non-NULL when an initial
  // incident is reported, and returns to NULL when the report is sent for
  // upload.
  scoped_ptr<ClientIncidentReport> report_;

  // The time at which the initial incident is reported.
  base::Time first_incident_time_;

  // The time at which the last incident is reported.
  base::TimeTicks last_incident_time_;

  // The time at which environmental data collection was initiated.
  base::TimeTicks environment_collection_begin_;

  // The collection of uploads in progress.
  ScopedVector<UploadContext> uploads_;

  // A factory for handing out weak pointers for AddIncident callbacks.
  base::WeakPtrFactory<IncidentReportingService> receiver_weak_ptr_factory_;

  // A factory for handing out weak pointers for internal asynchronous tasks
  // that are posted during normal processing (e.g., environment collection,
  // safe browsing database checks, and report uploads).
  base::WeakPtrFactory<IncidentReportingService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(IncidentReportingService);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_SERVICE_H_
