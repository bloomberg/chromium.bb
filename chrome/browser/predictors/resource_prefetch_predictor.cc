// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/resource_prefetch_predictor.h"

#include <map>
#include <set>
#include <utility>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/predictors/loading_data_collector.h"
#include "chrome/browser/predictors/predictor_database.h"
#include "chrome/browser/predictors/predictor_database_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/history/core/browser/history_database.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/url_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/web_contents.h"

using content::BrowserThread;

namespace predictors {

namespace {

const size_t kNumSampleHosts = 50;
const size_t kReportReadinessThreshold = 50;
const float kMinOriginConfidenceToTriggerPreconnect = 0.75f;
const float kMinOriginConfidenceToTriggerPreresolve = 0.2f;

// For reporting events of interest that are not tied to any navigation.
enum ReportingEvent {
  REPORTING_EVENT_ALL_HISTORY_CLEARED = 0,
  REPORTING_EVENT_PARTIAL_HISTORY_CLEARED = 1,
  REPORTING_EVENT_COUNT = 2
};

float ComputeRedirectConfidence(const predictors::RedirectStat& redirect) {
  return (redirect.number_of_hits() + 0.0) /
         (redirect.number_of_hits() + redirect.number_of_misses());
}

void InitializeOriginStatFromOriginRequestSummary(
    OriginStat* origin,
    const OriginRequestSummary& summary) {
  origin->set_origin(summary.origin.spec());
  origin->set_number_of_hits(1);
  origin->set_average_position(summary.first_occurrence + 1);
  origin->set_always_access_network(summary.always_access_network);
  origin->set_accessed_network(summary.accessed_network);
}

// Used to fetch the visit count for a URL from the History database.
class GetUrlVisitCountTask : public history::HistoryDBTask {
 public:
  typedef base::OnceCallback<void(size_t,  // URL visit count.
                                  const PageRequestSummary&)>
      VisitInfoCallback;

  GetUrlVisitCountTask(std::unique_ptr<PageRequestSummary> summary,
                       VisitInfoCallback callback);

  bool RunOnDBThread(history::HistoryBackend* backend,
                     history::HistoryDatabase* db) override;

  void DoneRunOnMainThread() override;

 private:
  ~GetUrlVisitCountTask() override;

  int visit_count_;
  std::unique_ptr<PageRequestSummary> summary_;
  VisitInfoCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(GetUrlVisitCountTask);
};

GetUrlVisitCountTask::GetUrlVisitCountTask(
    std::unique_ptr<PageRequestSummary> summary,
    VisitInfoCallback callback)
    : visit_count_(0),
      summary_(std::move(summary)),
      callback_(std::move(callback)) {
  DCHECK(summary_.get());
}

bool GetUrlVisitCountTask::RunOnDBThread(history::HistoryBackend* backend,
                                         history::HistoryDatabase* db) {
  history::URLRow url_row;
  if (db->GetRowForURL(summary_->main_frame_url, &url_row))
    visit_count_ = url_row.visit_count();
  return true;
}

void GetUrlVisitCountTask::DoneRunOnMainThread() {
  std::move(callback_).Run(visit_count_, *summary_);
}

GetUrlVisitCountTask::~GetUrlVisitCountTask() {}

void InitializeOnDBThread(
    ResourcePrefetchPredictor::PrefetchDataMap* url_resource_data,
    ResourcePrefetchPredictor::PrefetchDataMap* host_resource_data,
    ResourcePrefetchPredictor::RedirectDataMap* url_redirect_data,
    ResourcePrefetchPredictor::RedirectDataMap* host_redirect_data,
    ResourcePrefetchPredictor::OriginDataMap* origin_data) {
  url_resource_data->InitializeOnDBThread();
  host_resource_data->InitializeOnDBThread();
  url_redirect_data->InitializeOnDBThread();
  host_redirect_data->InitializeOnDBThread();
  origin_data->InitializeOnDBThread();
}

}  // namespace

PreconnectPrediction::PreconnectPrediction() = default;
PreconnectPrediction::PreconnectPrediction(
    const PreconnectPrediction& prediction) = default;
PreconnectPrediction::~PreconnectPrediction() = default;

////////////////////////////////////////////////////////////////////////////////
// ResourcePrefetchPredictor static functions.

bool ResourcePrefetchPredictor::GetRedirectEndpoint(
    const std::string& entry_point,
    const RedirectDataMap& redirect_data,
    std::string* redirect_endpoint) const {
  DCHECK(redirect_endpoint);

  RedirectData data;
  bool exists = redirect_data.TryGetData(entry_point, &data);
  if (!exists) {
    // Fallback to fetching URLs based on the incoming URL/host. By default
    // the predictor is confident that there is no redirect.
    *redirect_endpoint = entry_point;
    return true;
  }

  DCHECK_GT(data.redirect_endpoints_size(), 0);
  if (data.redirect_endpoints_size() > 1) {
    // The predictor observed multiple redirect destinations recently. Redirect
    // endpoint is ambiguous. The predictor predicts a redirect only if it
    // believes that the redirect is "permanent", i.e. subsequent navigations
    // will lead to the same destination.
    return false;
  }

  // The threshold is higher than the threshold for resources because the
  // redirect misprediction causes the waste of whole prefetch.
  const float kMinRedirectConfidenceToTriggerPrefetch = 0.9f;
  const int kMinRedirectHitsToTriggerPrefetch = 2;

  // The predictor doesn't apply a minimum-number-of-hits threshold to
  // the no-redirect case because the no-redirect is a default assumption.
  const RedirectStat& redirect = data.redirect_endpoints(0);
  if (ComputeRedirectConfidence(redirect) <
          kMinRedirectConfidenceToTriggerPrefetch ||
      (redirect.number_of_hits() < kMinRedirectHitsToTriggerPrefetch &&
       redirect.url() != entry_point)) {
    return false;
  }

  *redirect_endpoint = redirect.url();
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// ResourcePrefetchPredictor nested types.

ResourcePrefetchPredictor::Prediction::Prediction() = default;

ResourcePrefetchPredictor::Prediction::Prediction(
    const ResourcePrefetchPredictor::Prediction& other) = default;

ResourcePrefetchPredictor::Prediction::~Prediction() = default;

////////////////////////////////////////////////////////////////////////////////
// ResourcePrefetchPredictor.

ResourcePrefetchPredictor::ResourcePrefetchPredictor(
    const LoadingPredictorConfig& config,
    Profile* profile)
    : profile_(profile),
      observer_(nullptr),
      config_(config),
      initialization_state_(NOT_INITIALIZED),
      tables_(PredictorDatabaseFactory::GetForProfile(profile)
                  ->resource_prefetch_tables()),
      history_service_observer_(this),
      weak_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Some form of learning has to be enabled.
  DCHECK(config_.IsLearningEnabled());
}

ResourcePrefetchPredictor::~ResourcePrefetchPredictor() {}

void ResourcePrefetchPredictor::StartInitialization() {
  TRACE_EVENT0("browser", "ResourcePrefetchPredictor::StartInitialization");

  if (initialization_state_ != NOT_INITIALIZED)
    return;
  initialization_state_ = INITIALIZING;

  // Create local caches using the database as loaded.
  auto url_resource_data = base::MakeUnique<PrefetchDataMap>(
      tables_, tables_->url_resource_table(), config_.max_urls_to_track);
  auto host_resource_data = base::MakeUnique<PrefetchDataMap>(
      tables_, tables_->host_resource_table(), config_.max_hosts_to_track);
  auto url_redirect_data = base::MakeUnique<RedirectDataMap>(
      tables_, tables_->url_redirect_table(), config_.max_urls_to_track);
  auto host_redirect_data = base::MakeUnique<RedirectDataMap>(
      tables_, tables_->host_redirect_table(), config_.max_hosts_to_track);
  auto origin_data = base::MakeUnique<OriginDataMap>(
      tables_, tables_->origin_table(), config_.max_hosts_to_track);

  // Get raw pointers to pass to the first task. Ownership of the unique_ptrs
  // will be passed to the reply task.
  auto task = base::BindOnce(InitializeOnDBThread, url_resource_data.get(),
                             host_resource_data.get(), url_redirect_data.get(),
                             host_redirect_data.get(), origin_data.get());
  auto reply = base::BindOnce(
      &ResourcePrefetchPredictor::CreateCaches, weak_factory_.GetWeakPtr(),
      std::move(url_resource_data), std::move(host_resource_data),
      std::move(url_redirect_data), std::move(host_redirect_data),
      std::move(origin_data));

  BrowserThread::PostTaskAndReply(BrowserThread::DB, FROM_HERE, std::move(task),
                                  std::move(reply));
}

bool ResourcePrefetchPredictor::IsUrlPrefetchable(
    const GURL& main_frame_url) const {
  return GetPrefetchData(main_frame_url, nullptr);
}

bool ResourcePrefetchPredictor::IsResourcePrefetchable(
    const ResourceData& resource) const {
  float confidence = static_cast<float>(resource.number_of_hits()) /
                     (resource.number_of_hits() + resource.number_of_misses());
  return confidence >= config_.min_resource_confidence_to_trigger_prefetch &&
         resource.number_of_hits() >=
             config_.min_resource_hits_to_trigger_prefetch;
}

void ResourcePrefetchPredictor::SetObserverForTesting(TestObserver* observer) {
  observer_ = observer;
}

void ResourcePrefetchPredictor::Shutdown() {
  history_service_observer_.RemoveAll();
}

void ResourcePrefetchPredictor::RecordPageRequestSummary(
    std::unique_ptr<PageRequestSummary> summary) {
  // Make sure initialization is done or start initialization if necessary.
  if (initialization_state_ == NOT_INITIALIZED) {
    StartInitialization();
    return;
  } else if (initialization_state_ == INITIALIZING) {
    return;
  } else if (initialization_state_ != INITIALIZED) {
    NOTREACHED() << "Unexpected initialization_state_: "
                 << initialization_state_;
    return;
  }

  // Kick off history lookup to determine if we should record the URL.
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile_,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  DCHECK(history_service);
  history_service->ScheduleDBTask(
      std::unique_ptr<history::HistoryDBTask>(new GetUrlVisitCountTask(
          std::move(summary),
          base::BindOnce(&ResourcePrefetchPredictor::OnVisitCountLookup,
                         weak_factory_.GetWeakPtr()))),
      &history_lookup_consumer_);

  // Report readiness metric with 20% probability.
  if (base::RandInt(1, 5) == 5) {
    history_service->TopHosts(
        kNumSampleHosts,
        base::Bind(&ResourcePrefetchPredictor::ReportDatabaseReadiness,
                   weak_factory_.GetWeakPtr()));
  }
}

bool ResourcePrefetchPredictor::GetPrefetchData(
    const GURL& main_frame_url,
    ResourcePrefetchPredictor::Prediction* prediction) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (initialization_state_ != INITIALIZED)
    return false;

  std::vector<GURL>* urls =
      prediction ? &prediction->subresource_urls : nullptr;
  DCHECK(!urls || urls->empty());

  // Fetch resources using URL-keyed data first.
  std::string redirect_endpoint;
  const std::string& main_frame_url_spec = main_frame_url.spec();
  if (config_.is_url_learning_enabled &&
      GetRedirectEndpoint(main_frame_url_spec, *url_redirect_data_,
                          &redirect_endpoint) &&
      PopulatePrefetcherRequest(redirect_endpoint, *url_resource_data_, urls)) {
    if (prediction) {
      prediction->is_host = false;
      prediction->main_frame_key = redirect_endpoint;
      prediction->is_redirected = (redirect_endpoint != main_frame_url_spec);
    }
    return true;
  }

  // Use host data if the URL-based prediction isn't available.
  std::string main_frame_url_host = main_frame_url.host();
  if (GetRedirectEndpoint(main_frame_url_host, *host_redirect_data_,
                          &redirect_endpoint) &&
      PopulatePrefetcherRequest(redirect_endpoint, *host_resource_data_,
                                urls)) {
    if (prediction) {
      prediction->is_host = true;
      prediction->main_frame_key = redirect_endpoint;
      prediction->is_redirected = (redirect_endpoint != main_frame_url_host);
    }
    return true;
  }

  return false;
}

bool ResourcePrefetchPredictor::PredictPreconnectOrigins(
    const GURL& url,
    PreconnectPrediction* prediction) const {
  DCHECK(prediction);
  DCHECK(prediction->preconnect_origins.empty());
  DCHECK(prediction->preresolve_hosts.empty());
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (initialization_state_ != INITIALIZED)
    return false;

  std::string host = url.host();
  std::string redirect_endpoint;
  if (!GetRedirectEndpoint(host, *host_redirect_data_, &redirect_endpoint))
    return false;

  OriginData data;
  if (!origin_data_->TryGetData(redirect_endpoint, &data))
    return false;

  prediction->host = redirect_endpoint;
  prediction->is_redirected = (host != redirect_endpoint);
  for (const OriginStat& origin : data.origins()) {
    float confidence = static_cast<float>(origin.number_of_hits()) /
                       (origin.number_of_hits() + origin.number_of_misses());
    if (confidence > kMinOriginConfidenceToTriggerPreconnect) {
      prediction->preconnect_origins.emplace_back(origin.origin());
    } else if (confidence > kMinOriginConfidenceToTriggerPreresolve) {
      prediction->preresolve_hosts.emplace_back(origin.origin());
    }
  }

  return !prediction->preconnect_origins.empty() ||
         !prediction->preresolve_hosts.empty();
}

bool ResourcePrefetchPredictor::PopulatePrefetcherRequest(
    const std::string& main_frame_key,
    const PrefetchDataMap& resource_data,
    std::vector<GURL>* urls) const {
  PrefetchData data;
  bool exists = resource_data.TryGetData(main_frame_key, &data);
  if (!exists)
    return false;

  bool has_prefetchable_resource = false;
  for (const ResourceData& resource : data.resources()) {
    if (IsResourcePrefetchable(resource)) {
      has_prefetchable_resource = true;
      if (urls)
        urls->push_back(GURL(resource.resource_url()));
    }
  }

  return has_prefetchable_resource;
}

void ResourcePrefetchPredictor::CreateCaches(
    std::unique_ptr<PrefetchDataMap> url_resource_data,
    std::unique_ptr<PrefetchDataMap> host_resource_data,
    std::unique_ptr<RedirectDataMap> url_redirect_data,
    std::unique_ptr<RedirectDataMap> host_redirect_data,
    std::unique_ptr<OriginDataMap> origin_data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(INITIALIZING, initialization_state_);

  DCHECK(url_resource_data);
  DCHECK(host_resource_data);
  DCHECK(url_redirect_data);
  DCHECK(host_redirect_data);
  DCHECK(origin_data);

  url_resource_data_ = std::move(url_resource_data);
  host_resource_data_ = std::move(host_resource_data);
  url_redirect_data_ = std::move(url_redirect_data);
  host_redirect_data_ = std::move(host_redirect_data);
  origin_data_ = std::move(origin_data);

  ConnectToHistoryService();
}

void ResourcePrefetchPredictor::OnHistoryAndCacheLoaded() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(INITIALIZING, initialization_state_);

  initialization_state_ = INITIALIZED;
  if (observer_)
    observer_->OnPredictorInitialized();
}

void ResourcePrefetchPredictor::DeleteAllUrls() {
  url_resource_data_->DeleteAllData();
  host_resource_data_->DeleteAllData();
  url_redirect_data_->DeleteAllData();
  host_redirect_data_->DeleteAllData();
  origin_data_->DeleteAllData();
}

void ResourcePrefetchPredictor::DeleteUrls(const history::URLRows& urls) {
  std::vector<std::string> urls_to_delete;
  std::vector<std::string> hosts_to_delete;

  // Transform GURLs to keys for given database.
  for (const auto& it : urls) {
    urls_to_delete.emplace_back(it.url().spec());
    hosts_to_delete.emplace_back(it.url().host());
  }

  url_resource_data_->DeleteData(urls_to_delete);
  host_resource_data_->DeleteData(hosts_to_delete);
  url_redirect_data_->DeleteData(urls_to_delete);
  host_redirect_data_->DeleteData(hosts_to_delete);
  origin_data_->DeleteData(hosts_to_delete);
}

void ResourcePrefetchPredictor::OnVisitCountLookup(
    size_t url_visit_count,
    const PageRequestSummary& summary) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  UMA_HISTOGRAM_COUNTS("ResourcePrefetchPredictor.HistoryVisitCountForUrl",
                       url_visit_count);

  if (config_.is_url_learning_enabled) {
    // URL level data - merge only if we already saved the data, or it
    // meets the cutoff requirement.
    const std::string& url_spec = summary.main_frame_url.spec();
    bool already_tracking = url_resource_data_->TryGetData(url_spec, nullptr);
    bool should_track_url =
        already_tracking || (url_visit_count >= config_.min_url_visit_count);

    if (should_track_url) {
      LearnNavigation(url_spec, summary.subresource_requests,
                      url_resource_data_.get());
      LearnRedirect(summary.initial_url.spec(), url_spec,
                    url_redirect_data_.get());
    }
  }

  // Host level data - no cutoff, always learn the navigation if enabled.
  const std::string host = summary.main_frame_url.host();
  LearnNavigation(host, summary.subresource_requests,
                  host_resource_data_.get());
  LearnRedirect(summary.initial_url.host(), host, host_redirect_data_.get());

  if (config_.is_origin_learning_enabled)
    LearnOrigins(host, summary.origins);

  if (observer_)
    observer_->OnNavigationLearned(url_visit_count, summary);
}

void ResourcePrefetchPredictor::LearnNavigation(
    const std::string& key,
    const std::vector<URLRequestSummary>& new_resources,
    PrefetchDataMap* resource_data) {
  TRACE_EVENT1("browser", "ResourcePrefetchPredictor::LearnNavigation", "key",
               key);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // If the primary key is too long reject it.
  if (key.length() > ResourcePrefetchPredictorTables::kMaxStringLength)
    return;

  PrefetchData data;
  bool exists = resource_data->TryGetData(key, &data);
  if (!exists) {
    data.set_primary_key(key);
    data.set_last_visit_time(base::Time::Now().ToInternalValue());
    size_t new_resources_size = new_resources.size();
    std::set<GURL> resources_seen;
    for (size_t i = 0; i < new_resources_size; ++i) {
      const URLRequestSummary& summary = new_resources[i];
      if (resources_seen.find(summary.resource_url) != resources_seen.end())
        continue;

      ResourceData* resource_to_add = data.add_resources();
      resource_to_add->set_resource_url(summary.resource_url.spec());
      resource_to_add->set_resource_type(
          static_cast<ResourceData::ResourceType>(summary.resource_type));
      resource_to_add->set_number_of_hits(1);
      resource_to_add->set_average_position(i + 1);
      resource_to_add->set_priority(
          static_cast<ResourceData::Priority>(summary.priority));
      resource_to_add->set_before_first_contentful_paint(
          summary.before_first_contentful_paint);
      resource_to_add->set_has_validators(summary.has_validators);
      resource_to_add->set_always_revalidate(summary.always_revalidate);

      resources_seen.insert(summary.resource_url);
    }
  } else {
    data.set_last_visit_time(base::Time::Now().ToInternalValue());

    // Build indices over the data.
    std::map<GURL, int> new_index, old_index;
    int new_resources_size = static_cast<int>(new_resources.size());
    for (int i = 0; i < new_resources_size; ++i) {
      const URLRequestSummary& summary = new_resources[i];
      // Take the first occurence of every url.
      if (new_index.find(summary.resource_url) == new_index.end())
        new_index[summary.resource_url] = i;
    }
    int old_resources_size = static_cast<int>(data.resources_size());
    for (int i = 0; i < old_resources_size; ++i) {
      bool is_new =
          old_index
              .insert(std::make_pair(GURL(data.resources(i).resource_url()), i))
              .second;
      DCHECK(is_new);
    }

    // Go through the old urls and update their hit/miss counts.
    for (int i = 0; i < old_resources_size; ++i) {
      ResourceData* old_resource = data.mutable_resources(i);
      GURL resource_url(old_resource->resource_url());
      if (new_index.find(resource_url) == new_index.end()) {
        old_resource->set_number_of_misses(old_resource->number_of_misses() +
                                           1);
        old_resource->set_consecutive_misses(
            old_resource->consecutive_misses() + 1);
      } else {
        const URLRequestSummary& new_summary =
            new_resources[new_index[resource_url]];

        // Update the resource type since it could have changed.
        if (new_summary.resource_type != content::RESOURCE_TYPE_LAST_TYPE) {
          old_resource->set_resource_type(
              static_cast<ResourceData::ResourceType>(
                  new_summary.resource_type));
        }

        old_resource->set_priority(
            static_cast<ResourceData::Priority>(new_summary.priority));
        old_resource->set_before_first_contentful_paint(
            new_summary.before_first_contentful_paint);

        int position = new_index[resource_url] + 1;
        int total =
            old_resource->number_of_hits() + old_resource->number_of_misses();
        old_resource->set_average_position(
            ((old_resource->average_position() * total) + position) /
            (total + 1));
        old_resource->set_number_of_hits(old_resource->number_of_hits() + 1);
        old_resource->set_consecutive_misses(0);
      }
    }

    // Add the new ones that we have not seen before.
    for (int i = 0; i < new_resources_size; ++i) {
      const URLRequestSummary& summary = new_resources[i];
      if (old_index.find(summary.resource_url) != old_index.end())
        continue;

      // Only need to add new stuff.
      ResourceData* resource_to_add = data.add_resources();
      resource_to_add->set_resource_url(summary.resource_url.spec());
      resource_to_add->set_resource_type(
          static_cast<ResourceData::ResourceType>(summary.resource_type));
      resource_to_add->set_number_of_hits(1);
      resource_to_add->set_average_position(i + 1);
      resource_to_add->set_priority(
          static_cast<ResourceData::Priority>(summary.priority));
      resource_to_add->set_before_first_contentful_paint(
          summary.before_first_contentful_paint);
      resource_to_add->set_has_validators(new_resources[i].has_validators);
      resource_to_add->set_always_revalidate(
          new_resources[i].always_revalidate);

      // To ensure we dont add the same url twice.
      old_index[summary.resource_url] = 0;
    }
  }

  // Trim and sort the resources after the update.
  ResourcePrefetchPredictorTables::TrimResources(
      &data, config_.max_consecutive_misses);
  ResourcePrefetchPredictorTables::SortResources(&data);
  if (data.resources_size() >
      static_cast<int>(config_.max_resources_per_entry)) {
    data.mutable_resources()->DeleteSubrange(
        config_.max_resources_per_entry,
        data.resources_size() - config_.max_resources_per_entry);
  }

  if (data.resources_size() == 0)
    resource_data->DeleteData({key});
  else
    resource_data->UpdateData(key, data);
}

void ResourcePrefetchPredictor::LearnRedirect(const std::string& key,
                                              const std::string& final_redirect,
                                              RedirectDataMap* redirect_data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // If the primary key is too long reject it.
  if (key.length() > ResourcePrefetchPredictorTables::kMaxStringLength)
    return;

  RedirectData data;
  bool exists = redirect_data->TryGetData(key, &data);
  if (!exists) {
    data.set_primary_key(key);
    data.set_last_visit_time(base::Time::Now().ToInternalValue());
    RedirectStat* redirect_to_add = data.add_redirect_endpoints();
    redirect_to_add->set_url(final_redirect);
    redirect_to_add->set_number_of_hits(1);
  } else {
    data.set_last_visit_time(base::Time::Now().ToInternalValue());

    bool need_to_add = true;
    for (RedirectStat& redirect : *(data.mutable_redirect_endpoints())) {
      if (redirect.url() == final_redirect) {
        need_to_add = false;
        redirect.set_number_of_hits(redirect.number_of_hits() + 1);
        redirect.set_consecutive_misses(0);
      } else {
        redirect.set_number_of_misses(redirect.number_of_misses() + 1);
        redirect.set_consecutive_misses(redirect.consecutive_misses() + 1);
      }
    }

    if (need_to_add) {
      RedirectStat* redirect_to_add = data.add_redirect_endpoints();
      redirect_to_add->set_url(final_redirect);
      redirect_to_add->set_number_of_hits(1);
    }
  }

  // Trim the redirects after the update.
  ResourcePrefetchPredictorTables::TrimRedirects(
      &data, config_.max_redirect_consecutive_misses);

  if (data.redirect_endpoints_size() == 0)
    redirect_data->DeleteData({key});
  else
    redirect_data->UpdateData(key, data);
}

void ResourcePrefetchPredictor::LearnOrigins(
    const std::string& host,
    const std::map<GURL, OriginRequestSummary>& summaries) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (host.size() > ResourcePrefetchPredictorTables::kMaxStringLength)
    return;

  OriginData data;
  bool exists = origin_data_->TryGetData(host, &data);
  if (!exists) {
    data.set_host(host);
    data.set_last_visit_time(base::Time::Now().ToInternalValue());
    size_t origins_size = summaries.size();
    auto ordered_origins =
        std::vector<const OriginRequestSummary*>(origins_size);
    for (const auto& kv : summaries) {
      size_t index = kv.second.first_occurrence;
      DCHECK_LT(index, origins_size);
      ordered_origins[index] = &kv.second;
    }

    for (const OriginRequestSummary* summary : ordered_origins) {
      auto* origin_to_add = data.add_origins();
      InitializeOriginStatFromOriginRequestSummary(origin_to_add, *summary);
    }
  } else {
    data.set_last_visit_time(base::Time::Now().ToInternalValue());

    std::map<GURL, int> old_index;
    int old_size = static_cast<int>(data.origins_size());
    for (int i = 0; i < old_size; ++i) {
      bool is_new =
          old_index.insert({GURL(data.origins(i).origin()), i}).second;
      DCHECK(is_new);
    }

    // Update the old origins.
    for (int i = 0; i < old_size; ++i) {
      auto* old_origin = data.mutable_origins(i);
      GURL origin(old_origin->origin());
      auto it = summaries.find(origin);
      if (it == summaries.end()) {
        // miss
        old_origin->set_number_of_misses(old_origin->number_of_misses() + 1);
        old_origin->set_consecutive_misses(old_origin->consecutive_misses() +
                                           1);
      } else {
        // hit: update.
        const auto& new_origin = it->second;
        old_origin->set_always_access_network(new_origin.always_access_network);
        old_origin->set_accessed_network(new_origin.accessed_network);

        int position = new_origin.first_occurrence + 1;
        int total =
            old_origin->number_of_hits() + old_origin->number_of_misses();
        old_origin->set_average_position(
            ((old_origin->average_position() * total) + position) /
            (total + 1));
        old_origin->set_number_of_hits(old_origin->number_of_hits() + 1);
        old_origin->set_consecutive_misses(0);
      }
    }

    // Add new origins.
    for (const auto& kv : summaries) {
      if (old_index.find(kv.first) != old_index.end())
        continue;

      auto* origin_to_add = data.add_origins();
      InitializeOriginStatFromOriginRequestSummary(origin_to_add, kv.second);
    }
  }

  // Trim and Sort.
  ResourcePrefetchPredictorTables::TrimOrigins(&data,
                                               config_.max_consecutive_misses);
  ResourcePrefetchPredictorTables::SortOrigins(&data);
  if (data.origins_size() > static_cast<int>(config_.max_resources_per_entry)) {
    data.mutable_origins()->DeleteSubrange(
        config_.max_origins_per_entry,
        data.origins_size() - config_.max_origins_per_entry);
  }

  // Update the database.
  if (data.origins_size() == 0)
    origin_data_->DeleteData({host});
  else
    origin_data_->UpdateData(host, data);
}

void ResourcePrefetchPredictor::ReportDatabaseReadiness(
    const history::TopHostsList& top_hosts) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (top_hosts.size() == 0)
    return;

  size_t count_in_cache = 0;
  size_t total_visits = 0;
  for (const std::pair<std::string, int>& top_host : top_hosts) {
    const std::string& host = top_host.first;
    total_visits += top_host.second;

    // Hostnames in TopHostsLists are stripped of their 'www.' prefix. We
    // assume that www.foo.com entry from |host_resource_data_| is also suitable
    // for foo.com.
    if (PopulatePrefetcherRequest(host, *host_resource_data_, nullptr) ||
        (!base::StartsWith(host, "www.", base::CompareCase::SENSITIVE) &&
         PopulatePrefetcherRequest("www." + host, *host_resource_data_,
                                   nullptr))) {
      ++count_in_cache;
    }
  }

  // Filter users that don't have the rich browsing history.
  if (total_visits > kReportReadinessThreshold) {
    UMA_HISTOGRAM_PERCENTAGE("ResourcePrefetchPredictor.DatabaseReadiness",
                             100 * count_in_cache / top_hosts.size());
  }
}

void ResourcePrefetchPredictor::OnURLsDeleted(
    history::HistoryService* history_service,
    bool all_history,
    bool expired,
    const history::URLRows& deleted_rows,
    const std::set<GURL>& favicon_urls) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(initialization_state_ == INITIALIZED);

  if (all_history) {
    DeleteAllUrls();
    UMA_HISTOGRAM_ENUMERATION("ResourcePrefetchPredictor.ReportingEvent",
                              REPORTING_EVENT_ALL_HISTORY_CLEARED,
                              REPORTING_EVENT_COUNT);
  } else {
    DeleteUrls(deleted_rows);
    UMA_HISTOGRAM_ENUMERATION("ResourcePrefetchPredictor.ReportingEvent",
                              REPORTING_EVENT_PARTIAL_HISTORY_CLEARED,
                              REPORTING_EVENT_COUNT);
  }
}

void ResourcePrefetchPredictor::OnHistoryServiceLoaded(
    history::HistoryService* history_service) {
  if (initialization_state_ == INITIALIZING) {
    OnHistoryAndCacheLoaded();
  }
}

void ResourcePrefetchPredictor::ConnectToHistoryService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(INITIALIZING, initialization_state_);

  // Register for HistoryServiceLoading if it is not ready.
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile_,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  if (!history_service)
    return;
  DCHECK(!history_service_observer_.IsObserving(history_service));
  history_service_observer_.Add(history_service);
  if (history_service->BackendLoaded()) {
    // HistoryService is already loaded. Continue with Initialization.
    OnHistoryAndCacheLoaded();
  }
}

////////////////////////////////////////////////////////////////////////////////
// TestObserver.

TestObserver::~TestObserver() {
  predictor_->SetObserverForTesting(nullptr);
}

TestObserver::TestObserver(ResourcePrefetchPredictor* predictor)
    : predictor_(predictor) {
  predictor_->SetObserverForTesting(this);
}

}  // namespace predictors
