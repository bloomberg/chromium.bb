// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting_service.h"

#include <math.h>

#include <algorithm>
#include <vector>

#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/process/process_info.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/prefs/tracked/tracked_preference_validation_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/binary_integrity_incident_handlers.h"
#include "chrome/browser/safe_browsing/database_manager.h"
#include "chrome/browser/safe_browsing/environment_data_collection.h"
#include "chrome/browser/safe_browsing/incident_report_uploader_impl.h"
#include "chrome/browser/safe_browsing/preference_validation_delegate.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/tracked_preference_incident_handlers.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "net/url_request/url_request_context_getter.h"

namespace safe_browsing {

namespace {

// The type of an incident. Used for user metrics and for pruning of
// previously-reported incidents.
enum IncidentType {
  // Start with 1 rather than zero; otherwise there won't be enough buckets for
  // the histogram.
  TRACKED_PREFERENCE = 1,
  BINARY_INTEGRITY = 2,
  // Values for new incident types go here.
  NUM_INCIDENT_TYPES
};

// The action taken for an incident; used for user metrics (see
// LogIncidentDataType).
enum IncidentDisposition {
  DROPPED,
  ACCEPTED,
};

// The state persisted for a specific instance of an incident to enable pruning
// of previously-reported incidents.
struct PersistentIncidentState {
  // The type of the incident.
  IncidentType type;

  // The key for a specific instance of an incident.
  std::string key;

  // A hash digest representing a specific instance of an incident.
  uint32_t digest;
};

// The amount of time the service will wait to collate incidents.
const int64 kDefaultUploadDelayMs = 1000 * 60;  // one minute

// The amount of time between running delayed analysis callbacks.
const int64 kDefaultCallbackIntervalMs = 1000 * 20;

// Returns the number of incidents contained in |incident|. The result is
// expected to be 1. Used in DCHECKs.
size_t CountIncidents(const ClientIncidentReport_IncidentData& incident) {
  size_t result = 0;
  if (incident.has_tracked_preference())
    ++result;
  if (incident.has_binary_integrity())
    ++result;
  // Add detection for new incident types here.
  return result;
}

// Returns the type of incident contained in |incident_data|.
IncidentType GetIncidentType(
    const ClientIncidentReport_IncidentData& incident_data) {
  if (incident_data.has_tracked_preference())
    return TRACKED_PREFERENCE;
  if (incident_data.has_binary_integrity())
    return BINARY_INTEGRITY;

  // Add detection for new incident types here.
  COMPILE_ASSERT(BINARY_INTEGRITY + 1 == NUM_INCIDENT_TYPES,
                 add_support_for_new_types);
  NOTREACHED();
  return NUM_INCIDENT_TYPES;
}

// Logs the type of incident in |incident_data| to a user metrics histogram.
void LogIncidentDataType(
    IncidentDisposition disposition,
    const ClientIncidentReport_IncidentData& incident_data) {
  IncidentType type = GetIncidentType(incident_data);
  if (disposition == ACCEPTED) {
    UMA_HISTOGRAM_ENUMERATION("SBIRS.Incident", type, NUM_INCIDENT_TYPES);
  } else {
    DCHECK_EQ(disposition, DROPPED);
    UMA_HISTOGRAM_ENUMERATION("SBIRS.DroppedIncident", type,
                              NUM_INCIDENT_TYPES);
  }
}

// Computes the persistent state for an incident.
PersistentIncidentState ComputeIncidentState(
    const ClientIncidentReport_IncidentData& incident) {
  PersistentIncidentState state = {GetIncidentType(incident)};
  switch (state.type) {
    case TRACKED_PREFERENCE:
      state.key = GetTrackedPreferenceIncidentKey(incident);
      state.digest = GetTrackedPreferenceIncidentDigest(incident);
      break;
    case BINARY_INTEGRITY:
      state.key = GetBinaryIntegrityIncidentKey(incident);
      state.digest = GetBinaryIntegrityIncidentDigest(incident);
      break;
    // Add handling for new incident types here.
    default:
      COMPILE_ASSERT(BINARY_INTEGRITY + 1 == NUM_INCIDENT_TYPES,
                     add_support_for_new_types);
      NOTREACHED();
      break;
  }
  return state;
}

// Returns true if the incident described by |state| has already been reported
// based on the bookkeeping in the |incidents_sent| preference dictionary.
bool IncidentHasBeenReported(const base::DictionaryValue* incidents_sent,
                             const PersistentIncidentState& state) {
  const base::DictionaryValue* type_dict = NULL;
  std::string digest_string;
  return (incidents_sent &&
          incidents_sent->GetDictionaryWithoutPathExpansion(
              base::IntToString(state.type), &type_dict) &&
          type_dict->GetStringWithoutPathExpansion(state.key, &digest_string) &&
          digest_string == base::UintToString(state.digest));
}

// Marks the incidents described by |states| as having been reported
// in |incidents_set|.
void MarkIncidentsAsReported(const std::vector<PersistentIncidentState>& states,
                             base::DictionaryValue* incidents_sent) {
  for (size_t i = 0; i < states.size(); ++i) {
    const PersistentIncidentState& data = states[i];
    base::DictionaryValue* type_dict = NULL;
    const std::string type_string(base::IntToString(data.type));
    if (!incidents_sent->GetDictionaryWithoutPathExpansion(type_string,
                                                           &type_dict)) {
      type_dict = new base::DictionaryValue();
      incidents_sent->SetWithoutPathExpansion(type_string, type_dict);
    }
    type_dict->SetStringWithoutPathExpansion(data.key,
                                             base::UintToString(data.digest));
  }
}

// Runs |callback| on the thread to which |thread_runner| belongs. The callback
// is run immediately if this function is called on |thread_runner|'s thread.
void AddIncidentOnOriginThread(
    const AddIncidentCallback& callback,
    scoped_refptr<base::SingleThreadTaskRunner> thread_runner,
    scoped_ptr<ClientIncidentReport_IncidentData> incident) {
  if (thread_runner->BelongsToCurrentThread())
    callback.Run(incident.Pass());
  else
    thread_runner->PostTask(FROM_HERE,
                            base::Bind(callback, base::Passed(&incident)));
}

}  // namespace

struct IncidentReportingService::ProfileContext {
  ProfileContext();
  ~ProfileContext();

  // The incidents collected for this profile pending creation and/or upload.
  ScopedVector<ClientIncidentReport_IncidentData> incidents;

  // False until PROFILE_ADDED notification is received.
  bool added;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProfileContext);
};

class IncidentReportingService::UploadContext {
 public:
  typedef std::map<Profile*, std::vector<PersistentIncidentState> >
      PersistentIncidentStateCollection;

  explicit UploadContext(scoped_ptr<ClientIncidentReport> report);
  ~UploadContext();

  // The report being uploaded.
  scoped_ptr<ClientIncidentReport> report;

  // The uploader in use. This is NULL until the CSD killswitch is checked.
  scoped_ptr<IncidentReportUploader> uploader;

  // A mapping of profiles to the data to be persisted upon successful upload.
  PersistentIncidentStateCollection profiles_to_state;

 private:
  DISALLOW_COPY_AND_ASSIGN(UploadContext);
};

IncidentReportingService::ProfileContext::ProfileContext() : added() {
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
      collation_timeout_pending_(),
      collation_timer_(FROM_HERE,
                       base::TimeDelta::FromMilliseconds(kDefaultUploadDelayMs),
                       this,
                       &IncidentReportingService::OnCollationTimeout),
      delayed_analysis_callbacks_(
          base::TimeDelta::FromMilliseconds(kDefaultCallbackIntervalMs),
          content::BrowserThread::GetBlockingPool()
              ->GetTaskRunnerWithShutdownBehavior(
                  base::SequencedWorkerPool::SKIP_ON_SHUTDOWN)),
      receiver_weak_ptr_factory_(this),
      weak_ptr_factory_(this) {
  notification_registrar_.Add(this,
                              chrome::NOTIFICATION_PROFILE_ADDED,
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
  CancelDownloadCollection();
  CancelAllReportUploads();

  STLDeleteValues(&profiles_);
}

AddIncidentCallback IncidentReportingService::GetAddIncidentCallback(
    Profile* profile) {
  // Force the context to be created so that incidents added before
  // OnProfileAdded is called are held until the profile's preferences can be
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

void IncidentReportingService::RegisterDelayedAnalysisCallback(
    const DelayedAnalysisCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // |callback| will be run on the blocking pool, so it will likely run the
  // AddIncidentCallback there as well. Bounce the run of that callback back to
  // the current thread via AddIncidentOnOriginThread.
  delayed_analysis_callbacks_.RegisterCallback(
      base::Bind(callback,
                 base::Bind(&AddIncidentOnOriginThread,
                            GetAddIncidentCallback(NULL),
                            base::ThreadTaskRunnerHandle::Get())));

  // Start running the callbacks if any profiles are participating in safe
  // browsing. If none are now, running will commence if/when a participaing
  // profile is added.
  if (FindEligibleProfile())
    delayed_analysis_callbacks_.Start();
}

IncidentReportingService::IncidentReportingService(
    SafeBrowsingService* safe_browsing_service,
    const scoped_refptr<net::URLRequestContextGetter>& request_context_getter,
    base::TimeDelta delayed_task_interval,
    const scoped_refptr<base::TaskRunner>& delayed_task_runner)
    : database_manager_(safe_browsing_service ?
                        safe_browsing_service->database_manager() : NULL),
      url_request_context_getter_(request_context_getter),
      collect_environment_data_fn_(&CollectEnvironmentData),
      environment_collection_task_runner_(
          content::BrowserThread::GetBlockingPool()
              ->GetTaskRunnerWithShutdownBehavior(
                  base::SequencedWorkerPool::SKIP_ON_SHUTDOWN)),
      environment_collection_pending_(),
      collation_timeout_pending_(),
      collation_timer_(FROM_HERE,
                       base::TimeDelta::FromMilliseconds(kDefaultUploadDelayMs),
                       this,
                       &IncidentReportingService::OnCollationTimeout),
      delayed_analysis_callbacks_(delayed_task_interval, delayed_task_runner),
      receiver_weak_ptr_factory_(this),
      weak_ptr_factory_(this) {
  notification_registrar_.Add(this,
                              chrome::NOTIFICATION_PROFILE_ADDED,
                              content::NotificationService::AllSources());
  notification_registrar_.Add(this,
                              chrome::NOTIFICATION_PROFILE_DESTROYED,
                              content::NotificationService::AllSources());
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

void IncidentReportingService::OnProfileAdded(Profile* profile) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Track the addition of all profiles even when no report is being assembled
  // so that the service can determine whether or not it can evaluate a
  // profile's preferences at the time of incident addition.
  ProfileContext* context = GetOrCreateProfileContext(profile);
  context->added = true;

  const bool safe_browsing_enabled =
      profile->GetPrefs()->GetBoolean(prefs::kSafeBrowsingEnabled);

  // Start processing delayed analysis callbacks if this new profile
  // participates in safe browsing. Start is idempotent, so this is safe even if
  // they're already running.
  if (safe_browsing_enabled)
    delayed_analysis_callbacks_.Start();

  // Start a new report if this profile participates in safe browsing and there
  // are process-wide incidents.
  if (safe_browsing_enabled && GetProfileContext(NULL))
    BeginReportProcessing();

  // TODO(grt): register for pref change notifications to start delayed analysis
  // and/or report processing if sb is currently disabled but subsequently
  // enabled.

  // Nothing else to do if a report is not being assembled.
  if (!report_)
    return;

  // Drop all incidents associated with this profile that were received prior to
  // its addition if the profile is not participating in safe browsing.
  if (!context->incidents.empty() && !safe_browsing_enabled) {
    for (size_t i = 0; i < context->incidents.size(); ++i)
      LogIncidentDataType(DROPPED, *context->incidents[i]);
    context->incidents.clear();
  }

  // Take another stab at finding the most recent download if a report is being
  // assembled and one hasn't been found yet (the LastDownloadFinder operates
  // only on profiles that have been added to the ProfileManager).
  BeginDownloadCollection();
}

scoped_ptr<LastDownloadFinder> IncidentReportingService::CreateDownloadFinder(
    const LastDownloadFinder::LastDownloadCallback& callback) {
  return LastDownloadFinder::Create(callback).Pass();
}

scoped_ptr<IncidentReportUploader> IncidentReportingService::StartReportUpload(
    const IncidentReportUploader::OnResultCallback& callback,
    const scoped_refptr<net::URLRequestContextGetter>& request_context_getter,
    const ClientIncidentReport& report) {
  return IncidentReportUploaderImpl::UploadReport(
             callback, request_context_getter, report).Pass();
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

  // Remove the association with this profile from all pending uploads.
  for (size_t i = 0; i < uploads_.size(); ++i)
    uploads_[i]->profiles_to_state.erase(profile);
}

Profile* IncidentReportingService::FindEligibleProfile() const {
  Profile* candidate = NULL;
  for (ProfileContextCollection::const_iterator scan = profiles_.begin();
       scan != profiles_.end();
       ++scan) {
    // Skip over profiles that have yet to be added to the profile manager.
    // This will also skip over the NULL-profile context used to hold
    // process-wide incidents.
    if (!scan->second->added)
      continue;
    PrefService* prefs = scan->first->GetPrefs();
    if (prefs->GetBoolean(prefs::kSafeBrowsingEnabled)) {
      if (!candidate)
        candidate = scan->first;
      if (prefs->GetBoolean(prefs::kSafeBrowsingExtendedReportingEnabled)) {
        candidate = scan->first;
        break;
      }
    }
  }
  return candidate;
}

void IncidentReportingService::AddIncident(
    Profile* profile,
    scoped_ptr<ClientIncidentReport_IncidentData> incident_data) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(1U, CountIncidents(*incident_data));

  ProfileContext* context = GetProfileContext(profile);
  // It is forbidden to call this function with a destroyed profile.
  DCHECK(context);
  // If this is a process-wide incident, the context must not indicate that the
  // profile (which is NULL) has been added to the profile manager.
  DCHECK(profile || !context->added);

  // Drop the incident immediately if the profile has already been added to the
  // manager and is not participating in safe browsing. Preference evaluation is
  // deferred until OnProfileAdded() otherwise.
  if (context->added &&
      !profile->GetPrefs()->GetBoolean(prefs::kSafeBrowsingEnabled)) {
    LogIncidentDataType(DROPPED, *incident_data);
    return;
  }

  // Provide time to the new incident if the caller didn't do so.
  if (!incident_data->has_incident_time_msec())
    incident_data->set_incident_time_msec(base::Time::Now().ToJavaTime());

  // Take ownership of the incident.
  context->incidents.push_back(incident_data.release());

  // Remember when the first incident for this report arrived.
  if (first_incident_time_.is_null())
    first_incident_time_ = base::Time::Now();
  // Log the time between the previous incident and this one.
  if (!last_incident_time_.is_null()) {
    UMA_HISTOGRAM_TIMES("SBIRS.InterIncidentTime",
                        base::TimeTicks::Now() - last_incident_time_);
  }
  last_incident_time_ = base::TimeTicks::Now();

  // Persist the incident data.

  // Start assembling a new report if this is the first incident ever or the
  // first since the last upload.
  BeginReportProcessing();
}

void IncidentReportingService::BeginReportProcessing() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Creates a new report if needed.
  if (!report_)
    report_.reset(new ClientIncidentReport());

  // Ensure that collection tasks are running (calls are idempotent).
  BeginIncidentCollation();
  BeginEnvironmentCollection();
  BeginDownloadCollection();
}

void IncidentReportingService::BeginIncidentCollation() {
  // Restart the delay timer to send the report upon expiration.
  collation_timeout_pending_ = true;
  collation_timer_.Reset();
}

void IncidentReportingService::BeginEnvironmentCollection() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(report_);
  // Nothing to do if environment collection is pending or has already
  // completed.
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

bool IncidentReportingService::WaitingForEnvironmentCollection() {
  return environment_collection_pending_;
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

  report_->set_allocated_environment(environment_data.release());

  UMA_HISTOGRAM_TIMES("SBIRS.EnvCollectionTime",
                      base::TimeTicks::Now() - environment_collection_begin_);
  environment_collection_begin_ = base::TimeTicks();

  UploadIfCollectionComplete();
}

bool IncidentReportingService::WaitingToCollateIncidents() {
  return collation_timeout_pending_;
}

void IncidentReportingService::CancelIncidentCollection() {
  collation_timeout_pending_ = false;
  last_incident_time_ = base::TimeTicks();
  report_.reset();
}

void IncidentReportingService::OnCollationTimeout() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Exit early if collection was cancelled.
  if (!collation_timeout_pending_)
    return;

  // Wait another round if profile-bound incidents have come in from a profile
  // that has yet to complete creation.
  for (ProfileContextCollection::iterator scan = profiles_.begin();
       scan != profiles_.end();
       ++scan) {
    if (scan->first && !scan->second->added &&
        !scan->second->incidents.empty()) {
      collation_timer_.Reset();
      return;
    }
  }

  collation_timeout_pending_ = false;

  UploadIfCollectionComplete();
}

void IncidentReportingService::BeginDownloadCollection() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(report_);
  // Nothing to do if a search for the most recent download is already pending
  // or if one has already been found.
  if (last_download_finder_ || report_->has_download())
    return;

  last_download_begin_ = base::TimeTicks::Now();
  last_download_finder_ = CreateDownloadFinder(
      base::Bind(&IncidentReportingService::OnLastDownloadFound,
                 weak_ptr_factory_.GetWeakPtr()));
  // No instance is returned if there are no eligible loaded profiles. Another
  // search will be attempted in OnProfileAdded() if another profile appears on
  // the scene.
  if (!last_download_finder_)
    last_download_begin_ = base::TimeTicks();
}

bool IncidentReportingService::WaitingForMostRecentDownload() {
  DCHECK(report_);  // Only call this when a report is being assembled.
  // The easy case: not waiting if a download has already been found.
  if (report_->has_download())
    return false;
  // The next easy case: waiting if the finder is operating.
  if (last_download_finder_)
    return true;
  // The harder case: waiting if a profile has not yet been added.
  for (ProfileContextCollection::const_iterator scan = profiles_.begin();
       scan != profiles_.end();
       ++scan) {
    if (!scan->second->added)
      return true;
  }
  // There is no most recent download and there's nothing more to wait for.
  return false;
}

void IncidentReportingService::CancelDownloadCollection() {
  last_download_finder_.reset();
  last_download_begin_ = base::TimeTicks();
  if (report_)
    report_->clear_download();
}

void IncidentReportingService::OnLastDownloadFound(
    scoped_ptr<ClientIncidentReport_DownloadDetails> last_download) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(report_);

  UMA_HISTOGRAM_TIMES("SBIRS.FindDownloadedBinaryTime",
                      base::TimeTicks::Now() - last_download_begin_);
  last_download_begin_ = base::TimeTicks();

  // Harvest the finder.
  last_download_finder_.reset();

  if (last_download)
    report_->set_allocated_download(last_download.release());

  UploadIfCollectionComplete();
}

void IncidentReportingService::UploadIfCollectionComplete() {
  DCHECK(report_);
  // Bail out if there are still outstanding collection tasks. Completion of any
  // of these will start another upload attempt.
  if (WaitingForEnvironmentCollection() ||
      WaitingToCollateIncidents() ||
      WaitingForMostRecentDownload()) {
    return;
  }

  // Take ownership of the report and clear things for future reports.
  scoped_ptr<ClientIncidentReport> report(report_.Pass());
  first_incident_time_ = base::Time();
  last_incident_time_ = base::TimeTicks();

  // Drop the report if no executable download was found.
  if (!report->has_download()) {
    UMA_HISTOGRAM_ENUMERATION("SBIRS.UploadResult",
                              IncidentReportUploader::UPLOAD_NO_DOWNLOAD,
                              IncidentReportUploader::NUM_UPLOAD_RESULTS);
    return;
  }

  ClientIncidentReport_EnvironmentData_Process* process =
      report->mutable_environment()->mutable_process();

  // Not all platforms have a metrics reporting preference.
  if (g_browser_process->local_state()->FindPreference(
          prefs::kMetricsReportingEnabled)) {
    process->set_metrics_consent(g_browser_process->local_state()->GetBoolean(
        prefs::kMetricsReportingEnabled));
  }

  // Find the profile that benefits from the strongest protections.
  Profile* eligible_profile = FindEligibleProfile();
  process->set_extended_consent(
      eligible_profile ? eligible_profile->GetPrefs()->GetBoolean(
                             prefs::kSafeBrowsingExtendedReportingEnabled) :
                       false);

  // Associate process-wide incidents with the profile that benefits from the
  // strongest safe browsing protections.
  ProfileContext* null_context = GetProfileContext(NULL);
  if (null_context && !null_context->incidents.empty() && eligible_profile) {
    ProfileContext* eligible_context = GetProfileContext(eligible_profile);
    // Move the incidents to the target context.
    eligible_context->incidents.insert(eligible_context->incidents.end(),
                                       null_context->incidents.begin(),
                                       null_context->incidents.end());
    null_context->incidents.weak_clear();
  }

  // Collect incidents across all profiles participating in safe browsing. Drop
  // incidents if the profile stopped participating before collection completed.
  // Prune previously submitted incidents.
  // Associate the profiles and their incident data with the upload.
  size_t prune_count = 0;
  UploadContext::PersistentIncidentStateCollection profiles_to_state;
  for (ProfileContextCollection::iterator scan = profiles_.begin();
       scan != profiles_.end();
       ++scan) {
    // Bypass process-wide incidents that have not yet been associated with a
    // profile.
    if (!scan->first)
      continue;
    PrefService* prefs = scan->first->GetPrefs();
    ProfileContext* context = scan->second;
    if (context->incidents.empty())
      continue;
    if (!prefs->GetBoolean(prefs::kSafeBrowsingEnabled)) {
      for (size_t i = 0; i < context->incidents.size(); ++i) {
        LogIncidentDataType(DROPPED, *context->incidents[i]);
      }
      context->incidents.clear();
      continue;
    }
    std::vector<PersistentIncidentState> states;
    const base::DictionaryValue* incidents_sent =
        prefs->GetDictionary(prefs::kSafeBrowsingIncidentsSent);
    // Prep persistent data and prune any incidents already sent.
    for (size_t i = 0; i < context->incidents.size(); ++i) {
      ClientIncidentReport_IncidentData* incident = context->incidents[i];
      const PersistentIncidentState state = ComputeIncidentState(*incident);
      if (IncidentHasBeenReported(incidents_sent, state)) {
        ++prune_count;
        delete context->incidents[i];
        context->incidents[i] = NULL;
      } else {
        states.push_back(state);
      }
    }
    if (prefs->GetBoolean(prefs::kSafeBrowsingIncidentReportSent)) {
      // Prune all incidents as if they had been reported, migrating to the new
      // technique. TODO(grt): remove this branch after it has shipped.
      for (size_t i = 0; i < context->incidents.size(); ++i) {
        if (context->incidents[i])
          ++prune_count;
      }
      context->incidents.clear();
      prefs->ClearPref(prefs::kSafeBrowsingIncidentReportSent);
      DictionaryPrefUpdate pref_update(prefs,
                                       prefs::kSafeBrowsingIncidentsSent);
      MarkIncidentsAsReported(states, pref_update.Get());
    } else {
      for (size_t i = 0; i < context->incidents.size(); ++i) {
        ClientIncidentReport_IncidentData* incident = context->incidents[i];
        if (incident) {
          LogIncidentDataType(ACCEPTED, *incident);
          // Ownership of the incident is passed to the report.
          report->mutable_incident()->AddAllocated(incident);
        }
      }
      context->incidents.weak_clear();
      std::vector<PersistentIncidentState>& profile_states =
          profiles_to_state[scan->first];
      profile_states.swap(states);
    }
  }

  const int count = report->incident_size();
  // Abandon the request if all incidents were dropped with none pruned.
  if (!count && !prune_count)
    return;

  UMA_HISTOGRAM_COUNTS_100("SBIRS.IncidentCount", count + prune_count);

  {
    double prune_pct = static_cast<double>(prune_count);
    prune_pct = prune_pct * 100.0 / (count + prune_count);
    prune_pct = round(prune_pct);
    UMA_HISTOGRAM_PERCENTAGE("SBIRS.PruneRatio", static_cast<int>(prune_pct));
  }
  // Abandon the report if all incidents were pruned.
  if (!count)
    return;

  scoped_ptr<UploadContext> context(new UploadContext(report.Pass()));
  context->profiles_to_state.swap(profiles_to_state);
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

void IncidentReportingService::HandleResponse(const UploadContext& context) {
  for (UploadContext::PersistentIncidentStateCollection::const_iterator scan =
           context.profiles_to_state.begin();
       scan != context.profiles_to_state.end();
       ++scan) {
    DictionaryPrefUpdate pref_update(scan->first->GetPrefs(),
                                     prefs::kSafeBrowsingIncidentsSent);
    MarkIncidentsAsReported(scan->second, pref_update.Get());
  }
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
    HandleResponse(*upload);
  // else retry?
}

void IncidentReportingService::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_PROFILE_ADDED: {
      Profile* profile = content::Source<Profile>(source).ptr();
      if (!profile->IsOffTheRecord())
        OnProfileAdded(profile);
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
