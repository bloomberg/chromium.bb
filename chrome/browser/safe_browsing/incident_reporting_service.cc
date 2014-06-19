// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting_service.h"

#include <math.h>

#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/process/process_info.h"
#include "base/stl_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/prefs/tracked/tracked_preference_validation_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/database_manager.h"
#include "chrome/browser/safe_browsing/environment_data_collection.h"
#include "chrome/browser/safe_browsing/incident_report_uploader_impl.h"
#include "chrome/browser/safe_browsing/preference_validation_delegate.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "net/url_request/url_request_context_getter.h"

namespace safe_browsing {

namespace {

enum IncidentType {
  // Start with 1 rather than zero; otherwise there won't be enough buckets for
  // the histogram.
  TRACKED_PREFERENCE = 1,
  NUM_INCIDENT_TYPES
};

enum IncidentDisposition {
  DROPPED,
  ACCEPTED,
};

const int64 kDefaultUploadDelayMs = 1000 * 60;  // one minute

void LogIncidentDataType(
    IncidentDisposition disposition,
    const ClientIncidentReport_IncidentData& incident_data) {
  static const char kAcceptedMetric[] = "SBIRS.Incident";
  static const char kDroppedMetric[] = "SBIRS.DroppedIncident";

  IncidentType type = TRACKED_PREFERENCE;

  // Add a switch statement once other types are supported.
  DCHECK(incident_data.has_tracked_preference());

  if (disposition == ACCEPTED) {
    UMA_HISTOGRAM_ENUMERATION(kAcceptedMetric, type, NUM_INCIDENT_TYPES);
  } else {
    DCHECK_EQ(disposition, DROPPED);
    UMA_HISTOGRAM_ENUMERATION(kDroppedMetric, type, NUM_INCIDENT_TYPES);
  }
}

}  // namespace

struct IncidentReportingService::ProfileContext {
  ProfileContext();
  ~ProfileContext();

  // The incidents collected for this profile pending creation and/or upload.
  ScopedVector<ClientIncidentReport_IncidentData> incidents;

  // False until PROFILE_CREATED notification is received.
  bool created;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProfileContext);
};

class IncidentReportingService::UploadContext {
 public:
  explicit UploadContext(scoped_ptr<ClientIncidentReport> report);
  ~UploadContext();

  scoped_ptr<ClientIncidentReport> report;
  scoped_ptr<IncidentReportUploader> uploader;

 private:
  DISALLOW_COPY_AND_ASSIGN(UploadContext);
};

IncidentReportingService::ProfileContext::ProfileContext() : created() {
}

IncidentReportingService::ProfileContext::~ProfileContext() {
}

IncidentReportingService::UploadContext::UploadContext(
    scoped_ptr<ClientIncidentReport> report)
    : report(report.Pass()) {
}

IncidentReportingService::UploadContext::~UploadContext() {
}

IncidentReportingService::IncidentReportingService(
    SafeBrowsingService* safe_browsing_service,
    const scoped_refptr<net::URLRequestContextGetter>& request_context_getter)
    : database_manager_(safe_browsing_service ?
                        safe_browsing_service->database_manager() : NULL),
      url_request_context_getter_(request_context_getter),
      collect_environment_data_fn_(&CollectEnvironmentData),
      environment_collection_task_runner_(
          content::BrowserThread::GetBlockingPool()
              ->GetTaskRunnerWithShutdownBehavior(
                  base::SequencedWorkerPool::SKIP_ON_SHUTDOWN)),
      environment_collection_pending_(),
      collection_timeout_pending_(),
      upload_timer_(FROM_HERE,
                    base::TimeDelta::FromMilliseconds(kDefaultUploadDelayMs),
                    this,
                    &IncidentReportingService::OnCollectionTimeout),
      receiver_weak_ptr_factory_(this),
      weak_ptr_factory_(this) {
  notification_registrar_.Add(this,
                              chrome::NOTIFICATION_PROFILE_CREATED,
                              content::NotificationService::AllSources());
  notification_registrar_.Add(this,
                              chrome::NOTIFICATION_PROFILE_DESTROYED,
                              content::NotificationService::AllSources());
}

IncidentReportingService::~IncidentReportingService() {
  CancelIncidentCollection();

  // Cancel all internal asynchronous tasks.
  weak_ptr_factory_.InvalidateWeakPtrs();

  CancelEnvironmentCollection();
  CancelAllReportUploads();

  STLDeleteValues(&profiles_);
}

AddIncidentCallback IncidentReportingService::GetAddIncidentCallback(
    Profile* profile) {
  // Force the context to be created so that incidents added before
  // OnProfileCreated is called are held until the profile's preferences can be
  // queried.
  ignore_result(GetOrCreateProfileContext(profile));

  return base::Bind(&IncidentReportingService::AddIncident,
                    receiver_weak_ptr_factory_.GetWeakPtr(),
                    profile);
}

scoped_ptr<TrackedPreferenceValidationDelegate>
IncidentReportingService::CreatePreferenceValidationDelegate(Profile* profile) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (profile->IsOffTheRecord())
    return scoped_ptr<TrackedPreferenceValidationDelegate>();
  return scoped_ptr<TrackedPreferenceValidationDelegate>(
      new PreferenceValidationDelegate(GetAddIncidentCallback(profile)));
}

void IncidentReportingService::SetCollectEnvironmentHook(
    CollectEnvironmentDataFn collect_environment_data_hook,
    const scoped_refptr<base::TaskRunner>& task_runner) {
  if (collect_environment_data_hook) {
    collect_environment_data_fn_ = collect_environment_data_hook;
    environment_collection_task_runner_ = task_runner;
  } else {
    collect_environment_data_fn_ = &CollectEnvironmentData;
    environment_collection_task_runner_ =
        content::BrowserThread::GetBlockingPool()
            ->GetTaskRunnerWithShutdownBehavior(
                base::SequencedWorkerPool::SKIP_ON_SHUTDOWN);
  }
}

void IncidentReportingService::OnProfileCreated(Profile* profile) {
  DCHECK(thread_checker_.CalledOnValidThread());

  ProfileContext* context = GetOrCreateProfileContext(profile);
  context->created = true;

  // Drop all incidents if this profile is not participating in safe browsing.
  if (!context->incidents.empty() &&
      !profile->GetPrefs()->GetBoolean(prefs::kSafeBrowsingEnabled)) {
    for (size_t i = 0; i < context->incidents.size(); ++i) {
      LogIncidentDataType(DROPPED, *context->incidents[i]);
    }
    context->incidents.clear();
  }
}

scoped_ptr<IncidentReportUploader> IncidentReportingService::StartReportUpload(
    const IncidentReportUploader::OnResultCallback& callback,
    const scoped_refptr<net::URLRequestContextGetter>& request_context_getter,
    const ClientIncidentReport& report) {
#if 0
  return IncidentReportUploaderImpl::UploadReport(
             callback, request_context_getter, report).Pass();
#else
  // TODO(grt): Remove this temporary suppression of all uploads.
  return scoped_ptr<IncidentReportUploader>();
#endif
}

IncidentReportingService::ProfileContext*
IncidentReportingService::GetOrCreateProfileContext(Profile* profile) {
  ProfileContextCollection::iterator it =
      profiles_.insert(ProfileContextCollection::value_type(profile, NULL))
          .first;
  if (!it->second)
    it->second = new ProfileContext();
  return it->second;
}

IncidentReportingService::ProfileContext*
IncidentReportingService::GetProfileContext(Profile* profile) {
  ProfileContextCollection::iterator it = profiles_.find(profile);
  return it == profiles_.end() ? NULL : it->second;
}

void IncidentReportingService::OnProfileDestroyed(Profile* profile) {
  DCHECK(thread_checker_.CalledOnValidThread());

  ProfileContextCollection::iterator it = profiles_.find(profile);
  if (it == profiles_.end())
    return;

  // TODO(grt): Persist incidents for upload on future profile load.

  // Forget about this profile. Incidents not yet sent for upload are lost.
  // No new incidents will be accepted for it.
  delete it->second;
  profiles_.erase(it);
}

void IncidentReportingService::AddIncident(
    Profile* profile,
    scoped_ptr<ClientIncidentReport_IncidentData> incident_data) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Incidents outside the context of a profile are not supported at the moment.
  DCHECK(profile);

  ProfileContext* context = GetProfileContext(profile);
  // It is forbidden to call this function with a destroyed profile.
  DCHECK(context);

  // Drop the incident immediately if profile creation has completed and the
  // profile is not participating in safe browsing.
  if (context->created &&
      !profile->GetPrefs()->GetBoolean(prefs::kSafeBrowsingEnabled)) {
    LogIncidentDataType(DROPPED, *incident_data);
    return;
  }

  // Create a new report if this is the first incident ever or first since last
  // upload.
  if (!report_) {
    report_.reset(new ClientIncidentReport());
    first_incident_time_ = base::Time::Now();
  }

  // Provide time to the new incident if the caller didn't provide it.
  if (!incident_data->has_incident_time_msec())
    incident_data->set_incident_time_msec(base::Time::Now().ToJavaTime());

  // Take ownership of the incident.
  context->incidents.push_back(incident_data.release());

  if (!last_incident_time_.is_null()) {
    UMA_HISTOGRAM_TIMES("SBIRS.InterIncidentTime",
                        base::TimeTicks::Now() - last_incident_time_);
  }
  last_incident_time_ = base::TimeTicks::Now();

  // Persist the incident data.

  // Restart the delay timer to send the report upon expiration.
  collection_timeout_pending_ = true;
  upload_timer_.Reset();

  BeginEnvironmentCollection();
}

void IncidentReportingService::BeginEnvironmentCollection() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(report_);
  if (environment_collection_pending_ || report_->has_environment())
    return;

  environment_collection_begin_ = base::TimeTicks::Now();
  ClientIncidentReport_EnvironmentData* environment_data =
      new ClientIncidentReport_EnvironmentData();
  environment_collection_pending_ =
      environment_collection_task_runner_->PostTaskAndReply(
          FROM_HERE,
          base::Bind(collect_environment_data_fn_, environment_data),
          base::Bind(&IncidentReportingService::OnEnvironmentDataCollected,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::Passed(make_scoped_ptr(environment_data))));

  // Posting the task will fail if the runner has been shut down. This should
  // never happen since the blocking pool is shut down after this service.
  DCHECK(environment_collection_pending_);
}

void IncidentReportingService::CancelEnvironmentCollection() {
  environment_collection_begin_ = base::TimeTicks();
  environment_collection_pending_ = false;
  if (report_)
    report_->clear_environment();
}

void IncidentReportingService::OnEnvironmentDataCollected(
    scoped_ptr<ClientIncidentReport_EnvironmentData> environment_data) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(environment_collection_pending_);
  DCHECK(report_ && !report_->has_environment());
  environment_collection_pending_ = false;

// CurrentProcessInfo::CreationTime() is missing on some platforms.
#if defined(OS_MACOSX) || defined(OS_WIN) || defined(OS_LINUX)
  base::TimeDelta uptime =
      first_incident_time_ - base::CurrentProcessInfo::CreationTime();
  environment_data->mutable_process()->set_uptime_msec(uptime.InMilliseconds());
#endif
  first_incident_time_ = base::Time();

  report_->set_allocated_environment(environment_data.release());

  UMA_HISTOGRAM_TIMES("SBIRS.EnvCollectionTime",
                      base::TimeTicks::Now() - environment_collection_begin_);
  environment_collection_begin_ = base::TimeTicks();

  UploadIfCollectionComplete();
}

void IncidentReportingService::CancelIncidentCollection() {
  collection_timeout_pending_ = false;
  last_incident_time_ = base::TimeTicks();
  report_.reset();
}

void IncidentReportingService::OnCollectionTimeout() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Exit early if collection was cancelled.
  if (!collection_timeout_pending_)
    return;

  // Wait another round if incidents have come in from a profile that has yet to
  // complete creation.
  for (ProfileContextCollection::iterator scan = profiles_.begin();
       scan != profiles_.end();
       ++scan) {
    if (!scan->second->created && !scan->second->incidents.empty()) {
      upload_timer_.Reset();
      return;
    }
  }

  collection_timeout_pending_ = false;

  UploadIfCollectionComplete();
}

void IncidentReportingService::CollectDownloadDetails(
    ClientIncidentReport_DownloadDetails* download_details) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // TODO(grt): collect download info; http://crbug.com/383042.
}

void IncidentReportingService::RecordReportedIncidents() {
  // TODO(grt): store state about reported incidents.
}

void IncidentReportingService::PruneReportedIncidents(
    ClientIncidentReport* report) {
  // TODO(grt): remove previously reported incidents; http://crbug.com/383043.
}

void IncidentReportingService::UploadIfCollectionComplete() {
  DCHECK(report_);
  // Bail out if there are still outstanding collection tasks.
  if (environment_collection_pending_ || collection_timeout_pending_)
    return;

  // Take ownership of the report and clear things for future reports.
  scoped_ptr<ClientIncidentReport> report(report_.Pass());
  last_incident_time_ = base::TimeTicks();

  ClientIncidentReport_EnvironmentData_Process* process =
      report->mutable_environment()->mutable_process();

  // Not all platforms have a metrics reporting preference.
  if (g_browser_process->local_state()->FindPreference(
          prefs::kMetricsReportingEnabled)) {
    process->set_metrics_consent(g_browser_process->local_state()->GetBoolean(
        prefs::kMetricsReportingEnabled));
  }
  // Check for extended consent in any profile while collecting incidents.
  process->set_extended_consent(false);
  // Collect incidents across all profiles participating in safe browsing. Drop
  // incidents if the profile stopped participating before collection completed.
  for (ProfileContextCollection::iterator scan = profiles_.begin();
       scan != profiles_.end();
       ++scan) {
    PrefService* prefs = scan->first->GetPrefs();
    if (process &&
        prefs->GetBoolean(prefs::kSafeBrowsingExtendedReportingEnabled)) {
      process->set_extended_consent(true);
      process = NULL;  // Don't check any more once one is found.
    }
    ProfileContext* context = scan->second;
    if (context->incidents.empty())
      continue;
    if (prefs->GetBoolean(prefs::kSafeBrowsingEnabled)) {
      for (size_t i = 0; i < context->incidents.size(); ++i) {
        ClientIncidentReport_IncidentData* incident = context->incidents[i];
        LogIncidentDataType(ACCEPTED, *incident);
        // Ownership of the incident is passed to the report.
        report->mutable_incident()->AddAllocated(incident);
      }
      context->incidents.weak_clear();
    } else {
      for (size_t i = 0; i < context->incidents.size(); ++i) {
        LogIncidentDataType(DROPPED, *context->incidents[i]);
      }
      context->incidents.clear();
    }
  }

  const int original_count = report->incident_size();
  // Abandon the request if all incidents were dropped.
  if (!original_count)
    return;

  UMA_HISTOGRAM_COUNTS_100("SBIRS.IncidentCount", original_count);

  PruneReportedIncidents(report.get());

  int final_count = report->incident_size();
  {
    double prune_pct = static_cast<double>(original_count - final_count);
    prune_pct = prune_pct * 100.0 / original_count;
    prune_pct = round(prune_pct);
    UMA_HISTOGRAM_PERCENTAGE("SBIRS.PruneRatio", static_cast<int>(prune_pct));
  }
  // Abandon the report if all incidents were pruned.
  if (!final_count)
    return;

  scoped_ptr<UploadContext> context(new UploadContext(report.Pass()));
  if (!database_manager_) {
    // No database manager during testing. Take ownership of the context and
    // continue processing.
    UploadContext* temp_context = context.get();
    uploads_.push_back(context.release());
    IncidentReportingService::OnKillSwitchResult(temp_context, false);
  } else {
    if (content::BrowserThread::PostTaskAndReplyWithResult(
            content::BrowserThread::IO,
            FROM_HERE,
            base::Bind(&SafeBrowsingDatabaseManager::IsCsdWhitelistKillSwitchOn,
                       database_manager_),
            base::Bind(&IncidentReportingService::OnKillSwitchResult,
                       weak_ptr_factory_.GetWeakPtr(),
                       context.get()))) {
      uploads_.push_back(context.release());
    }  // else should not happen. Let the context be deleted automatically.
  }
}

void IncidentReportingService::CancelAllReportUploads() {
  for (size_t i = 0; i < uploads_.size(); ++i) {
    UMA_HISTOGRAM_ENUMERATION("SBIRS.UploadResult",
                              IncidentReportUploader::UPLOAD_CANCELLED,
                              IncidentReportUploader::NUM_UPLOAD_RESULTS);
  }
  uploads_.clear();
}

void IncidentReportingService::OnKillSwitchResult(UploadContext* context,
                                                  bool is_killswitch_on) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!is_killswitch_on) {
    // Initiate the upload.
    context->uploader =
        StartReportUpload(
            base::Bind(&IncidentReportingService::OnReportUploadResult,
                       weak_ptr_factory_.GetWeakPtr(),
                       context),
            url_request_context_getter_,
            *context->report).Pass();
    if (!context->uploader) {
      OnReportUploadResult(context,
                           IncidentReportUploader::UPLOAD_INVALID_REQUEST,
                           scoped_ptr<ClientIncidentResponse>());
    }
  } else {
    OnReportUploadResult(context,
                         IncidentReportUploader::UPLOAD_SUPPRESSED,
                         scoped_ptr<ClientIncidentResponse>());
  }
}

void IncidentReportingService::HandleResponse(
    scoped_ptr<ClientIncidentReport> report,
    scoped_ptr<ClientIncidentResponse> response) {
  RecordReportedIncidents();
}

void IncidentReportingService::OnReportUploadResult(
    UploadContext* context,
    IncidentReportUploader::Result result,
    scoped_ptr<ClientIncidentResponse> response) {
  DCHECK(thread_checker_.CalledOnValidThread());

  UMA_HISTOGRAM_ENUMERATION(
      "SBIRS.UploadResult", result, IncidentReportUploader::NUM_UPLOAD_RESULTS);

  // The upload is no longer outstanding, so take ownership of the context (from
  // the collection of outstanding uploads) in this scope.
  ScopedVector<UploadContext>::iterator it(
      std::find(uploads_.begin(), uploads_.end(), context));
  DCHECK(it != uploads_.end());
  scoped_ptr<UploadContext> upload(context);  // == *it
  *it = uploads_.back();
  uploads_.weak_erase(uploads_.end() - 1);

  if (result == IncidentReportUploader::UPLOAD_SUCCESS)
    HandleResponse(upload->report.Pass(), response.Pass());
  // else retry?
}

void IncidentReportingService::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_PROFILE_CREATED: {
      Profile* profile = content::Source<Profile>(source).ptr();
      if (!profile->IsOffTheRecord())
        OnProfileCreated(profile);
      break;
    }
    case chrome::NOTIFICATION_PROFILE_DESTROYED: {
      Profile* profile = content::Source<Profile>(source).ptr();
      if (!profile->IsOffTheRecord())
        OnProfileDestroyed(profile);
      break;
    }
    default:
      break;
  }
}

}  // namespace safe_browsing
