// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/resource_prefetch_predictor.h"

#include <map>
#include <set>
#include <utility>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/predictors/predictor_database.h"
#include "chrome/browser/predictors/predictor_database_factory.h"
#include "chrome/browser/predictors/resource_prefetcher_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "components/history/core/browser/history_database.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/history_service.h"
#include "components/mime_util/mime_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/web_contents.h"
#include "net/base/mime_util.h"
#include "net/base/network_change_notifier.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context_getter.h"

using content::BrowserThread;

namespace {

// Sorted by decreasing likelihood according to HTTP archive.
const char* kFontMimeTypes[] = {"font/woff2",
                                "application/x-font-woff",
                                "application/font-woff",
                                "application/font-woff2",
                                "font/x-woff",
                                "application/x-font-ttf",
                                "font/woff",
                                "font/ttf",
                                "application/x-font-otf",
                                "x-font/woff",
                                "application/font-sfnt",
                                "application/font-ttf"};

// For reporting events of interest that are not tied to any navigation.
enum ReportingEvent {
  REPORTING_EVENT_ALL_HISTORY_CLEARED = 0,
  REPORTING_EVENT_PARTIAL_HISTORY_CLEARED = 1,
  REPORTING_EVENT_COUNT = 2
};

}  // namespace

namespace predictors {

////////////////////////////////////////////////////////////////////////////////
// History lookup task.

// Used to fetch the visit count for a URL from the History database.
class GetUrlVisitCountTask : public history::HistoryDBTask {
 public:
  typedef ResourcePrefetchPredictor::URLRequestSummary URLRequestSummary;
  typedef base::Callback<void(
      size_t,   // Visit count.
      const NavigationID&,
      const std::vector<URLRequestSummary>&)> VisitInfoCallback;

  GetUrlVisitCountTask(
      const NavigationID& navigation_id,
      std::vector<URLRequestSummary>* requests,
      VisitInfoCallback callback)
      : visit_count_(0),
        navigation_id_(navigation_id),
        requests_(requests),
        callback_(callback) {
    DCHECK(requests_.get());
  }

  bool RunOnDBThread(history::HistoryBackend* backend,
                     history::HistoryDatabase* db) override {
    history::URLRow url_row;
    if (db->GetRowForURL(navigation_id_.main_frame_url, &url_row))
      visit_count_ = url_row.visit_count();
    return true;
  }

  void DoneRunOnMainThread() override {
    callback_.Run(visit_count_, navigation_id_, *requests_);
  }

 private:
  ~GetUrlVisitCountTask() override {}

  int visit_count_;
  NavigationID navigation_id_;
  std::unique_ptr<std::vector<URLRequestSummary>> requests_;
  VisitInfoCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(GetUrlVisitCountTask);
};

////////////////////////////////////////////////////////////////////////////////
// ResourcePrefetchPredictor static functions.

// static
bool ResourcePrefetchPredictor::ShouldRecordRequest(
    net::URLRequest* request,
    content::ResourceType resource_type) {
  const content::ResourceRequestInfo* request_info =
      content::ResourceRequestInfo::ForRequest(request);
  if (!request_info)
    return false;

  if (!request_info->IsMainFrame())
    return false;

  return resource_type == content::RESOURCE_TYPE_MAIN_FRAME &&
      IsHandledMainPage(request);
}

// static
bool ResourcePrefetchPredictor::ShouldRecordResponse(
    net::URLRequest* response) {
  const content::ResourceRequestInfo* request_info =
      content::ResourceRequestInfo::ForRequest(response);
  if (!request_info)
    return false;

  if (!request_info->IsMainFrame())
    return false;

  content::ResourceType resource_type = request_info->GetResourceType();
  return resource_type == content::RESOURCE_TYPE_MAIN_FRAME
             ? IsHandledMainPage(response)
             : IsHandledSubresource(response, resource_type);
}

// static
bool ResourcePrefetchPredictor::ShouldRecordRedirect(
    net::URLRequest* response) {
  const content::ResourceRequestInfo* request_info =
      content::ResourceRequestInfo::ForRequest(response);
  if (!request_info)
    return false;

  if (!request_info->IsMainFrame())
    return false;

  return request_info->GetResourceType() == content::RESOURCE_TYPE_MAIN_FRAME &&
      IsHandledMainPage(response);
}

// static
bool ResourcePrefetchPredictor::IsHandledMainPage(net::URLRequest* request) {
  return request->url().SchemeIsHTTPOrHTTPS();
}

// static
bool ResourcePrefetchPredictor::IsHandledSubresource(
    net::URLRequest* response,
    content::ResourceType resource_type) {
  if (!response->first_party_for_cookies().SchemeIsHTTPOrHTTPS() ||
      !response->url().SchemeIsHTTPOrHTTPS())
    return false;

  std::string mime_type;
  response->GetMimeType(&mime_type);
  if (!IsHandledResourceType(resource_type, mime_type))
    return false;

  if (response->method() != "GET")
    return false;

  if (response->original_url().spec().length() >
      ResourcePrefetchPredictorTables::kMaxStringLength) {
    return false;
  }

  if (!response->response_info().headers.get() || IsNoStore(response))
    return false;

  return true;
}

// static
bool ResourcePrefetchPredictor::IsHandledResourceType(
    content::ResourceType resource_type,
    const std::string& mime_type) {
  content::ResourceType actual_resource_type =
      GetResourceType(resource_type, mime_type);
  return actual_resource_type == content::RESOURCE_TYPE_STYLESHEET ||
         actual_resource_type == content::RESOURCE_TYPE_SCRIPT ||
         actual_resource_type == content::RESOURCE_TYPE_IMAGE ||
         actual_resource_type == content::RESOURCE_TYPE_FONT_RESOURCE;
}

// static
content::ResourceType ResourcePrefetchPredictor::GetResourceType(
    content::ResourceType resource_type,
    const std::string& mime_type) {
  // Restricts content::RESOURCE_TYPE_{PREFETCH,SUB_RESOURCE} to a small set of
  // mime types, because these resource types don't communicate how the
  // resources will be used.
  if (resource_type == content::RESOURCE_TYPE_PREFETCH ||
      resource_type == content::RESOURCE_TYPE_SUB_RESOURCE) {
    return GetResourceTypeFromMimeType(mime_type,
                                       content::RESOURCE_TYPE_LAST_TYPE);
  }
  return resource_type;
}

// static
bool ResourcePrefetchPredictor::IsNoStore(const net::URLRequest* response) {
  if (response->was_cached())
    return false;

  const net::HttpResponseInfo& response_info = response->response_info();
  if (!response_info.headers.get())
    return false;
  return response_info.headers->HasHeaderValue("cache-control", "no-store");
}

// static
content::ResourceType ResourcePrefetchPredictor::GetResourceTypeFromMimeType(
    const std::string& mime_type,
    content::ResourceType fallback) {
  if (mime_type.empty()) {
    return fallback;
  } else if (mime_util::IsSupportedImageMimeType(mime_type)) {
    return content::RESOURCE_TYPE_IMAGE;
  } else if (mime_util::IsSupportedJavascriptMimeType(mime_type)) {
    return content::RESOURCE_TYPE_SCRIPT;
  } else if (net::MatchesMimeType("text/css", mime_type)) {
    return content::RESOURCE_TYPE_STYLESHEET;
  } else {
    bool found =
        std::any_of(std::begin(kFontMimeTypes), std::end(kFontMimeTypes),
                    [&mime_type](const std::string& mime) {
                      return net::MatchesMimeType(mime, mime_type);
                    });
    if (found)
      return content::RESOURCE_TYPE_FONT_RESOURCE;
  }
  return fallback;
}

////////////////////////////////////////////////////////////////////////////////
// ResourcePrefetchPredictor structs.

ResourcePrefetchPredictor::URLRequestSummary::URLRequestSummary()
    : resource_type(content::RESOURCE_TYPE_LAST_TYPE),
      priority(net::IDLE),
      was_cached(false),
      has_validators(false),
      always_revalidate(false) {}

ResourcePrefetchPredictor::URLRequestSummary::URLRequestSummary(
    const URLRequestSummary& other)
    : navigation_id(other.navigation_id),
      resource_url(other.resource_url),
      resource_type(other.resource_type),
      priority(other.priority),
      mime_type(other.mime_type),
      was_cached(other.was_cached),
      redirect_url(other.redirect_url),
      has_validators(other.has_validators),
      always_revalidate(other.always_revalidate) {}

ResourcePrefetchPredictor::URLRequestSummary::~URLRequestSummary() {
}

// static
bool ResourcePrefetchPredictor::URLRequestSummary::SummarizeResponse(
    const net::URLRequest& request,
    URLRequestSummary* summary) {
  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(&request);
  if (!info)
    return false;

  int render_process_id, render_frame_id;
  if (!info->GetAssociatedRenderFrame(&render_process_id, &render_frame_id))
    return false;

  summary->navigation_id = NavigationID(render_process_id, render_frame_id,
                                        request.first_party_for_cookies());
  summary->navigation_id.creation_time = request.creation_time();
  summary->resource_url = request.original_url();
  content::ResourceType resource_type_from_request = info->GetResourceType();
  summary->priority = request.priority();
  request.GetMimeType(&summary->mime_type);
  summary->was_cached = request.was_cached();
  summary->resource_type =
      GetResourceType(resource_type_from_request, summary->mime_type);

  scoped_refptr<net::HttpResponseHeaders> headers =
      request.response_info().headers;
  if (headers.get()) {
    summary->has_validators = headers->HasValidators();
    // RFC 2616, section 14.9.
    summary->always_revalidate =
        headers->HasHeaderValue("cache-control", "no-cache") ||
        headers->HasHeaderValue("pragma", "no-cache") ||
        headers->HasHeaderValue("vary", "*");
  }
  return true;
}

ResourcePrefetchPredictor::Result::Result(
    PrefetchKeyType i_key_type,
    ResourcePrefetcher::RequestVector* i_requests)
    : key_type(i_key_type),
      requests(i_requests) {
}

ResourcePrefetchPredictor::Result::~Result() {
}

////////////////////////////////////////////////////////////////////////////////
// ResourcePrefetchPredictor.

ResourcePrefetchPredictor::ResourcePrefetchPredictor(
    const ResourcePrefetchPredictorConfig& config,
    Profile* profile)
    : profile_(profile),
      config_(config),
      initialization_state_(NOT_INITIALIZED),
      tables_(PredictorDatabaseFactory::GetForProfile(profile)
                  ->resource_prefetch_tables()),
      history_service_observer_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Some form of learning has to be enabled.
  DCHECK(config_.IsLearningEnabled());
  if (config_.IsURLPrefetchingEnabled(profile_))
    DCHECK(config_.IsURLLearningEnabled());
  if (config_.IsHostPrefetchingEnabled(profile_))
    DCHECK(config_.IsHostLearningEnabled());
}

ResourcePrefetchPredictor::~ResourcePrefetchPredictor() {
}

void ResourcePrefetchPredictor::RecordURLRequest(
    const URLRequestSummary& request) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (initialization_state_ != INITIALIZED)
    return;

  CHECK_EQ(request.resource_type, content::RESOURCE_TYPE_MAIN_FRAME);
  OnMainFrameRequest(request);
}

void ResourcePrefetchPredictor::RecordURLResponse(
    const URLRequestSummary& response) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (initialization_state_ != INITIALIZED)
    return;

  if (response.resource_type == content::RESOURCE_TYPE_MAIN_FRAME)
    OnMainFrameResponse(response);
  else
    OnSubresourceResponse(response);
}

void ResourcePrefetchPredictor::RecordURLRedirect(
    const URLRequestSummary& response) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (initialization_state_ != INITIALIZED)
    return;

  CHECK_EQ(response.resource_type, content::RESOURCE_TYPE_MAIN_FRAME);
  OnMainFrameRedirect(response);
}

void ResourcePrefetchPredictor::RecordMainFrameLoadComplete(
    const NavigationID& navigation_id) {
  switch (initialization_state_) {
    case NOT_INITIALIZED:
      StartInitialization();
      break;
    case INITIALIZING:
      break;
    case INITIALIZED:
      // WebContents can return an empty URL if the navigation entry
      // corresponding to the navigation has not been created yet.
      if (!navigation_id.main_frame_url.is_empty())
        OnNavigationComplete(navigation_id);
      break;
    default:
      NOTREACHED() << "Unexpected initialization_state_: "
                   << initialization_state_;
  }
}

void ResourcePrefetchPredictor::FinishedPrefetchForNavigation(
    const NavigationID& navigation_id,
    PrefetchKeyType key_type,
    ResourcePrefetcher::RequestVector* requests) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::unique_ptr<Result> result(new Result(key_type, requests));
  // Add the results to the results map.
  if (!results_map_.insert(std::make_pair(navigation_id, std::move(result)))
           .second)
    DLOG(FATAL) << "Returning results for existing navigation.";
}

void ResourcePrefetchPredictor::Shutdown() {
  if (prefetch_manager_.get()) {
    prefetch_manager_->ShutdownOnUIThread();
    prefetch_manager_ = NULL;
  }
  history_service_observer_.RemoveAll();
}

void ResourcePrefetchPredictor::OnMainFrameRequest(
    const URLRequestSummary& request) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(INITIALIZED, initialization_state_);

  StartPrefetching(request.navigation_id);

  // Cleanup older navigations.
  CleanupAbandonedNavigations(request.navigation_id);

  // New empty navigation entry.
  inflight_navigations_.insert(std::make_pair(
      request.navigation_id,
      make_linked_ptr(new std::vector<URLRequestSummary>())));
}

void ResourcePrefetchPredictor::OnMainFrameResponse(
    const URLRequestSummary& response) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (initialization_state_ != INITIALIZED)
    return;

  StopPrefetching(response.navigation_id);
}

void ResourcePrefetchPredictor::OnMainFrameRedirect(
    const URLRequestSummary& response) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(shishir): There are significant gains to be had here if we can use the
  // start URL in a redirect chain as the key to start prefetching. We can save
  // of redirect times considerably assuming that the redirect chains do not
  // change.

  // Stop any inflight prefetching. Remove the older navigation.
  StopPrefetching(response.navigation_id);
  inflight_navigations_.erase(response.navigation_id);

  // A redirect will not lead to another OnMainFrameRequest call, so record the
  // redirect url as a new navigation.

  // The redirect url may be empty if the URL was invalid.
  if (response.redirect_url.is_empty())
    return;

  NavigationID navigation_id(response.navigation_id);
  navigation_id.main_frame_url = response.redirect_url;
  inflight_navigations_.insert(std::make_pair(
      navigation_id,
      make_linked_ptr(new std::vector<URLRequestSummary>())));
}

void ResourcePrefetchPredictor::OnSubresourceResponse(
    const URLRequestSummary& response) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  NavigationMap::const_iterator nav_it =
        inflight_navigations_.find(response.navigation_id);
  if (nav_it == inflight_navigations_.end()) {
    return;
  }

  nav_it->second->push_back(response);
}

void ResourcePrefetchPredictor::OnNavigationComplete(
    const NavigationID& nav_id_without_timing_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  NavigationMap::iterator nav_it =
      inflight_navigations_.find(nav_id_without_timing_info);
  if (nav_it == inflight_navigations_.end())
    return;

  const NavigationID navigation_id(nav_it->first);

  // Remove the navigation from the inflight navigations.
  std::vector<URLRequestSummary>* requests = (nav_it->second).release();
  inflight_navigations_.erase(nav_it);

  // Kick off history lookup to determine if we should record the URL.
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile_,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  DCHECK(history_service);
  history_service->ScheduleDBTask(
      std::unique_ptr<history::HistoryDBTask>(new GetUrlVisitCountTask(
          navigation_id, requests,
          base::Bind(&ResourcePrefetchPredictor::OnVisitCountLookup,
                     AsWeakPtr()))),
      &history_lookup_consumer_);
}

bool ResourcePrefetchPredictor::GetPrefetchData(
    const NavigationID& navigation_id,
    ResourcePrefetcher::RequestVector* prefetch_requests,
    PrefetchKeyType* key_type) {
  DCHECK(prefetch_requests);
  DCHECK(key_type);

  *key_type = PREFETCH_KEY_TYPE_URL;
  const GURL& main_frame_url = navigation_id.main_frame_url;

  bool use_url_data = config_.IsPrefetchingEnabled(profile_) ?
      config_.IsURLPrefetchingEnabled(profile_) :
      config_.IsURLLearningEnabled();
  if (use_url_data) {
    PrefetchDataMap::const_iterator iterator =
        url_table_cache_->find(main_frame_url.spec());
    if (iterator != url_table_cache_->end())
      PopulatePrefetcherRequest(iterator->second, prefetch_requests);
  }
  if (!prefetch_requests->empty())
    return true;

  bool use_host_data = config_.IsPrefetchingEnabled(profile_) ?
      config_.IsHostPrefetchingEnabled(profile_) :
      config_.IsHostLearningEnabled();
  if (use_host_data) {
    PrefetchDataMap::const_iterator iterator =
        host_table_cache_->find(main_frame_url.host());
    if (iterator != host_table_cache_->end()) {
      *key_type = PREFETCH_KEY_TYPE_HOST;
      PopulatePrefetcherRequest(iterator->second, prefetch_requests);
    }
  }

  return !prefetch_requests->empty();
}

void ResourcePrefetchPredictor::PopulatePrefetcherRequest(
    const PrefetchData& data,
    ResourcePrefetcher::RequestVector* requests) {
  for (const ResourceRow& row : data.resources) {
    float confidence = static_cast<float>(row.number_of_hits) /
                       (row.number_of_hits + row.number_of_misses);
    if (confidence < config_.min_resource_confidence_to_trigger_prefetch ||
        row.number_of_hits < config_.min_resource_hits_to_trigger_prefetch) {
      continue;
    }

    ResourcePrefetcher::Request* req =
        new ResourcePrefetcher::Request(row.resource_url);
    requests->push_back(req);
  }
}

void ResourcePrefetchPredictor::StartPrefetching(
    const NavigationID& navigation_id) {
  if (!prefetch_manager_.get())  // Prefetching not enabled.
    return;

  // Prefer URL based data first.
  std::unique_ptr<ResourcePrefetcher::RequestVector> requests(
      new ResourcePrefetcher::RequestVector);
  PrefetchKeyType key_type;
  if (!GetPrefetchData(navigation_id, requests.get(), &key_type)) {
    // No prefetching data at host or URL level.
    return;
  }

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      base::Bind(&ResourcePrefetcherManager::MaybeAddPrefetch,
                 prefetch_manager_,
                 navigation_id,
                 key_type,
                 base::Passed(&requests)));
}

void ResourcePrefetchPredictor::StopPrefetching(
    const NavigationID& navigation_id) {
  if (!prefetch_manager_.get())  // Not enabled.
    return;

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&ResourcePrefetcherManager::MaybeRemovePrefetch,
                 prefetch_manager_,
                 navigation_id));
}

void ResourcePrefetchPredictor::StartInitialization() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  DCHECK_EQ(NOT_INITIALIZED, initialization_state_);
  initialization_state_ = INITIALIZING;

  // Create local caches using the database as loaded.
  std::unique_ptr<PrefetchDataMap> url_data_map(new PrefetchDataMap());
  std::unique_ptr<PrefetchDataMap> host_data_map(new PrefetchDataMap());
  PrefetchDataMap* url_data_ptr = url_data_map.get();
  PrefetchDataMap* host_data_ptr = host_data_map.get();

  BrowserThread::PostTaskAndReply(
      BrowserThread::DB, FROM_HERE,
      base::Bind(&ResourcePrefetchPredictorTables::GetAllData,
                 tables_, url_data_ptr, host_data_ptr),
      base::Bind(&ResourcePrefetchPredictor::CreateCaches, AsWeakPtr(),
                 base::Passed(&url_data_map), base::Passed(&host_data_map)));
}

void ResourcePrefetchPredictor::CreateCaches(
    std::unique_ptr<PrefetchDataMap> url_data_map,
    std::unique_ptr<PrefetchDataMap> host_data_map) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  DCHECK_EQ(INITIALIZING, initialization_state_);
  DCHECK(!url_table_cache_);
  DCHECK(!host_table_cache_);
  DCHECK(inflight_navigations_.empty());

  url_table_cache_.reset(url_data_map.release());
  host_table_cache_.reset(host_data_map.release());

  UMA_HISTOGRAM_COUNTS("ResourcePrefetchPredictor.UrlTableMainFrameUrlCount",
                       url_table_cache_->size());
  UMA_HISTOGRAM_COUNTS("ResourcePrefetchPredictor.HostTableHostCount",
                       host_table_cache_->size());

  ConnectToHistoryService();
}

void ResourcePrefetchPredictor::OnHistoryAndCacheLoaded() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(INITIALIZING, initialization_state_);

  // Initialize the prefetch manager only if prefetching is enabled.
  if (config_.IsPrefetchingEnabled(profile_)) {
    prefetch_manager_ = new ResourcePrefetcherManager(
        this, config_, profile_->GetRequestContext());
  }
  initialization_state_ = INITIALIZED;
}

void ResourcePrefetchPredictor::CleanupAbandonedNavigations(
    const NavigationID& navigation_id) {
  static const base::TimeDelta max_navigation_age =
      base::TimeDelta::FromSeconds(config_.max_navigation_lifetime_seconds);

  base::TimeTicks time_now = base::TimeTicks::Now();
  for (NavigationMap::iterator it = inflight_navigations_.begin();
       it != inflight_navigations_.end();) {
    if (it->first.IsSameRenderer(navigation_id) ||
        (time_now - it->first.creation_time > max_navigation_age)) {
      inflight_navigations_.erase(it++);
    } else {
      ++it;
    }
  }
  for (ResultsMap::const_iterator it = results_map_.begin();
       it != results_map_.end();) {
    if (it->first.IsSameRenderer(navigation_id) ||
        (time_now - it->first.creation_time > max_navigation_age)) {
      results_map_.erase(it++);
    } else {
      ++it;
    }
  }
}

void ResourcePrefetchPredictor::DeleteAllUrls() {
  inflight_navigations_.clear();
  url_table_cache_->clear();
  host_table_cache_->clear();

  BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
      base::Bind(&ResourcePrefetchPredictorTables::DeleteAllData, tables_));
}

void ResourcePrefetchPredictor::DeleteUrls(const history::URLRows& urls) {
  // Check all the urls in the database and pick out the ones that are present
  // in the cache.
  std::vector<std::string> urls_to_delete, hosts_to_delete;

  for (const auto& it : urls) {
    const std::string& url_spec = it.url().spec();
    if (url_table_cache_->find(url_spec) != url_table_cache_->end()) {
      urls_to_delete.push_back(url_spec);
      url_table_cache_->erase(url_spec);
    }

    const std::string& host = it.url().host();
    if (host_table_cache_->find(host) != host_table_cache_->end()) {
      hosts_to_delete.push_back(host);
      host_table_cache_->erase(host);
    }
  }

  if (!urls_to_delete.empty() || !hosts_to_delete.empty()) {
    BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
        base::Bind(&ResourcePrefetchPredictorTables::DeleteData,
                   tables_,
                   urls_to_delete,
                   hosts_to_delete));
  }
}

void ResourcePrefetchPredictor::RemoveOldestEntryInPrefetchDataMap(
    PrefetchKeyType key_type,
    PrefetchDataMap* data_map) {
  if (data_map->empty())
    return;

  base::Time oldest_time;
  std::string key_to_delete;
  for (PrefetchDataMap::iterator it = data_map->begin();
       it != data_map->end(); ++it) {
    if (key_to_delete.empty() || it->second.last_visit < oldest_time) {
      key_to_delete = it->first;
      oldest_time = it->second.last_visit;
    }
  }

  data_map->erase(key_to_delete);
    BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
        base::Bind(&ResourcePrefetchPredictorTables::DeleteSingleDataPoint,
                   tables_,
                   key_to_delete,
                   key_type));
}

void ResourcePrefetchPredictor::OnVisitCountLookup(
    size_t visit_count,
    const NavigationID& navigation_id,
    const std::vector<URLRequestSummary>& requests) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  UMA_HISTOGRAM_COUNTS("ResourcePrefetchPredictor.HistoryVisitCountForUrl",
                       visit_count);

  // URL level data - merge only if we are already saving the data, or we it
  // meets the cutoff requirement.
  const std::string url_spec = navigation_id.main_frame_url.spec();
  bool already_tracking = url_table_cache_->find(url_spec) !=
      url_table_cache_->end();
  bool should_track_url = already_tracking ||
      (visit_count >= config_.min_url_visit_count);

  if (should_track_url && config_.IsURLLearningEnabled()) {
    LearnNavigation(url_spec, PREFETCH_KEY_TYPE_URL, requests,
                    config_.max_urls_to_track, url_table_cache_.get());
  }

  // Host level data - no cutoff, always learn the navigation if enabled.
  if (config_.IsHostLearningEnabled()) {
    LearnNavigation(navigation_id.main_frame_url.host(),
                    PREFETCH_KEY_TYPE_HOST,
                    requests,
                    config_.max_hosts_to_track,
                    host_table_cache_.get());
  }

  // Remove the navigation from the results map.
  results_map_.erase(navigation_id);
}

void ResourcePrefetchPredictor::LearnNavigation(
    const std::string& key,
    PrefetchKeyType key_type,
    const std::vector<URLRequestSummary>& new_resources,
    size_t max_data_map_size,
    PrefetchDataMap* data_map) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // If the primary key is too long reject it.
  if (key.length() > ResourcePrefetchPredictorTables::kMaxStringLength)
    return;

  PrefetchDataMap::iterator cache_entry = data_map->find(key);
  if (cache_entry == data_map->end()) {
    if (data_map->size() >= max_data_map_size) {
      // The table is full, delete an entry.
      RemoveOldestEntryInPrefetchDataMap(key_type, data_map);
    }

    cache_entry = data_map->insert(std::make_pair(
        key, PrefetchData(key_type, key))).first;
    cache_entry->second.last_visit = base::Time::Now();
    size_t new_resources_size = new_resources.size();
    std::set<GURL> resources_seen;
    for (size_t i = 0; i < new_resources_size; ++i) {
      if (resources_seen.find(new_resources[i].resource_url) !=
          resources_seen.end()) {
        continue;
      }
      ResourceRow row_to_add;
      row_to_add.resource_url = new_resources[i].resource_url;
      row_to_add.resource_type = new_resources[i].resource_type;
      row_to_add.number_of_hits = 1;
      row_to_add.average_position = i + 1;
      row_to_add.priority = new_resources[i].priority;
      row_to_add.has_validators = new_resources[i].has_validators;
      row_to_add.always_revalidate = new_resources[i].always_revalidate;
      cache_entry->second.resources.push_back(row_to_add);
      resources_seen.insert(new_resources[i].resource_url);
    }
  } else {
    ResourceRows& old_resources = cache_entry->second.resources;
    cache_entry->second.last_visit = base::Time::Now();

    // Build indices over the data.
    std::map<GURL, int> new_index, old_index;
    int new_resources_size = static_cast<int>(new_resources.size());
    for (int i = 0; i < new_resources_size; ++i) {
      const URLRequestSummary& summary = new_resources[i];
      // Take the first occurence of every url.
      if (new_index.find(summary.resource_url) == new_index.end())
        new_index[summary.resource_url] = i;
    }
    int old_resources_size = static_cast<int>(old_resources.size());
    for (int i = 0; i < old_resources_size; ++i) {
      const ResourceRow& row = old_resources[i];
      DCHECK(old_index.find(row.resource_url) == old_index.end());
      old_index[row.resource_url] = i;
    }

    // Go through the old urls and update their hit/miss counts.
    for (int i = 0; i < old_resources_size; ++i) {
      ResourceRow& old_row = old_resources[i];
      if (new_index.find(old_row.resource_url) == new_index.end()) {
        ++old_row.number_of_misses;
        ++old_row.consecutive_misses;
      } else {
        const URLRequestSummary& new_row =
            new_resources[new_index[old_row.resource_url]];

        // Update the resource type since it could have changed.
        if (new_row.resource_type != content::RESOURCE_TYPE_LAST_TYPE)
          old_row.resource_type = new_row.resource_type;

        old_row.priority = new_row.priority;

        int position = new_index[old_row.resource_url] + 1;
        int total = old_row.number_of_hits + old_row.number_of_misses;
        old_row.average_position =
            ((old_row.average_position * total) + position) / (total + 1);
        ++old_row.number_of_hits;
        old_row.consecutive_misses = 0;
      }
    }

    // Add the new ones that we have not seen before.
    for (int i = 0; i < new_resources_size; ++i) {
      const URLRequestSummary& summary = new_resources[i];
      if (old_index.find(summary.resource_url) != old_index.end())
        continue;

      // Only need to add new stuff.
      ResourceRow row_to_add;
      row_to_add.resource_url = summary.resource_url;
      row_to_add.resource_type = summary.resource_type;
      row_to_add.number_of_hits = 1;
      row_to_add.average_position = i + 1;
      row_to_add.priority = summary.priority;
      row_to_add.has_validators = new_resources[i].has_validators;
      row_to_add.always_revalidate = new_resources[i].always_revalidate;
      old_resources.push_back(row_to_add);

      // To ensure we dont add the same url twice.
      old_index[summary.resource_url] = 0;
    }
  }

  // Trim and sort the resources after the update.
  ResourceRows& resources = cache_entry->second.resources;
  for (ResourceRows::iterator it = resources.begin();
       it != resources.end();) {
    it->UpdateScore();
    if (it->consecutive_misses >= config_.max_consecutive_misses)
      it = resources.erase(it);
    else
      ++it;
  }
  ResourcePrefetchPredictorTables::SortResourceRows(&resources);
  if (resources.size() > config_.max_resources_per_entry)
    resources.resize(config_.max_resources_per_entry);

  // If the row has no resources, remove it from the cache and delete the
  // entry in the database. Else update the database.
  if (resources.empty()) {
    data_map->erase(key);
    BrowserThread::PostTask(
        BrowserThread::DB, FROM_HERE,
        base::Bind(&ResourcePrefetchPredictorTables::DeleteSingleDataPoint,
                   tables_,
                   key,
                   key_type));
  } else {
    bool is_host = key_type == PREFETCH_KEY_TYPE_HOST;
    PrefetchData empty_data(
        !is_host ? PREFETCH_KEY_TYPE_HOST : PREFETCH_KEY_TYPE_URL,
        std::string());
    const PrefetchData& host_data = is_host ? cache_entry->second : empty_data;
    const PrefetchData& url_data = is_host ? empty_data : cache_entry->second;
    BrowserThread::PostTask(
        BrowserThread::DB, FROM_HERE,
        base::Bind(&ResourcePrefetchPredictorTables::UpdateData,
                   tables_,
                   url_data,
                   host_data));
  }
}

void ResourcePrefetchPredictor::OnURLsDeleted(
    history::HistoryService* history_service,
    bool all_history,
    bool expired,
    const history::URLRows& deleted_rows,
    const std::set<GURL>& favicon_urls) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (INITIALIZED != initialization_state_)
    return;

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
  OnHistoryAndCacheLoaded();
  history_service_observer_.Remove(history_service);
}

void ResourcePrefetchPredictor::ConnectToHistoryService() {
  // Register for HistoryServiceLoading if it is not ready.
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile_,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  if (!history_service)
    return;
  if (history_service->BackendLoaded()) {
    // HistoryService is already loaded. Continue with Initialization.
    OnHistoryAndCacheLoaded();
    return;
  }
  DCHECK(!history_service_observer_.IsObserving(history_service));
  history_service_observer_.Add(history_service);
  return;
}

}  // namespace predictors
