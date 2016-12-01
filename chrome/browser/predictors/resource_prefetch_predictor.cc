// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/resource_prefetch_predictor.h"

#include <map>
#include <set>
#include <utility>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/predictors/predictor_database.h"
#include "chrome/browser/predictors/predictor_database_factory.h"
#include "chrome/browser/predictors/resource_prefetcher_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "components/history/core/browser/history_database.h"
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

namespace predictors {

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

float ComputeRedirectConfidence(const predictors::RedirectStat& redirect) {
  return (redirect.number_of_hits() + 0.0) /
         (redirect.number_of_hits() + redirect.number_of_misses());
}

// Used to fetch the visit count for a URL from the History database.
class GetUrlVisitCountTask : public history::HistoryDBTask {
 public:
  using URLRequestSummary = ResourcePrefetchPredictor::URLRequestSummary;
  using PageRequestSummary = ResourcePrefetchPredictor::PageRequestSummary;
  typedef base::Callback<void(size_t,  // URL visit count.
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
    : visit_count_(0), summary_(std::move(summary)), callback_(callback) {
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
  callback_.Run(visit_count_, *summary_);
}

GetUrlVisitCountTask::~GetUrlVisitCountTask() {}

}  // namespace

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
  // Restricts content::RESOURCE_TYPE_{PREFETCH,SUB_RESOURCE,XHR} to a small set
  // of mime types, because these resource types don't communicate how the
  // resources will be used.
  if (resource_type == content::RESOURCE_TYPE_PREFETCH ||
      resource_type == content::RESOURCE_TYPE_SUB_RESOURCE ||
      resource_type == content::RESOURCE_TYPE_XHR) {
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

// static
bool ResourcePrefetchPredictor::GetRedirectEndpoint(
    const std::string& first_redirect,
    const RedirectDataMap& redirect_data_map,
    std::string* final_redirect) {
  DCHECK(final_redirect);

  RedirectDataMap::const_iterator it = redirect_data_map.find(first_redirect);
  if (it == redirect_data_map.end())
    return false;

  const RedirectData& redirect_data = it->second;
  auto best_redirect = std::max_element(
      redirect_data.redirect_endpoints().begin(),
      redirect_data.redirect_endpoints().end(),
      [](const RedirectStat& x, const RedirectStat& y) {
        return ComputeRedirectConfidence(x) < ComputeRedirectConfidence(y);
      });

  const float kMinRedirectConfidenceToTriggerPrefetch = 0.7f;
  const int kMinRedirectHitsToTriggerPrefetch = 2;

  if (best_redirect == redirect_data.redirect_endpoints().end() ||
      ComputeRedirectConfidence(*best_redirect) <
          kMinRedirectConfidenceToTriggerPrefetch ||
      best_redirect->number_of_hits() < kMinRedirectHitsToTriggerPrefetch)
    return false;

  *final_redirect = best_redirect->url();
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// ResourcePrefetchPredictor nested types.

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

ResourcePrefetchPredictor::PageRequestSummary::PageRequestSummary(
    const GURL& i_main_frame_url)
    : main_frame_url(i_main_frame_url), initial_url(i_main_frame_url) {}

ResourcePrefetchPredictor::PageRequestSummary::PageRequestSummary(
    const PageRequestSummary& other) = default;

ResourcePrefetchPredictor::PageRequestSummary::~PageRequestSummary() {}

////////////////////////////////////////////////////////////////////////////////
// ResourcePrefetchPredictor.

ResourcePrefetchPredictor::ResourcePrefetchPredictor(
    const ResourcePrefetchPredictorConfig& config,
    Profile* profile)
    : profile_(profile),
      observer_(nullptr),
      config_(config),
      initialization_state_(NOT_INITIALIZED),
      tables_(PredictorDatabaseFactory::GetForProfile(profile)
                  ->resource_prefetch_tables()),
      history_service_observer_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Some form of learning has to be enabled.
  DCHECK(config_.IsLearningEnabled());
}

ResourcePrefetchPredictor::~ResourcePrefetchPredictor() {}

void ResourcePrefetchPredictor::StartInitialization() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT0("browser", "ResourcePrefetchPredictor::StartInitialization");

  if (initialization_state_ != NOT_INITIALIZED)
    return;
  initialization_state_ = INITIALIZING;

  // Create local caches using the database as loaded.
  auto url_data_map = base::MakeUnique<PrefetchDataMap>();
  auto host_data_map = base::MakeUnique<PrefetchDataMap>();
  auto url_redirect_data_map = base::MakeUnique<RedirectDataMap>();
  auto host_redirect_data_map = base::MakeUnique<RedirectDataMap>();

  // Get raw pointers to pass to the first task. Ownership of the unique_ptrs
  // will be passed to the reply task.
  auto url_data_map_ptr = url_data_map.get();
  auto host_data_map_ptr = host_data_map.get();
  auto url_redirect_data_map_ptr = url_redirect_data_map.get();
  auto host_redirect_data_map_ptr = host_redirect_data_map.get();

  BrowserThread::PostTaskAndReply(
      BrowserThread::DB, FROM_HERE,
      base::Bind(&ResourcePrefetchPredictorTables::GetAllData, tables_,
                 url_data_map_ptr, host_data_map_ptr, url_redirect_data_map_ptr,
                 host_redirect_data_map_ptr),
      base::Bind(&ResourcePrefetchPredictor::CreateCaches, AsWeakPtr(),
                 base::Passed(&url_data_map), base::Passed(&host_data_map),
                 base::Passed(&url_redirect_data_map),
                 base::Passed(&host_redirect_data_map)));
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

void ResourcePrefetchPredictor::StartPrefetching(const GURL& url,
                                                 PrefetchOrigin origin) {
  TRACE_EVENT1("browser", "ResourcePrefetchPredictor::StartPrefetching", "url",
               url.spec());
  if (!prefetch_manager_.get())  // Prefetching not enabled.
    return;
  if (!config_.IsPrefetchingEnabledForOrigin(profile_, origin))
    return;

  std::vector<GURL> subresource_urls;
  if (!GetPrefetchData(url, &subresource_urls))
    return;

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&ResourcePrefetcherManager::MaybeAddPrefetch,
                 prefetch_manager_, url, subresource_urls));
}

void ResourcePrefetchPredictor::StopPrefetching(const GURL& url) {
  TRACE_EVENT1("browser", "ResourcePrefetchPredictor::StopPrefetching", "url",
               url.spec());
  if (!prefetch_manager_.get())  // Not enabled.
    return;

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&ResourcePrefetcherManager::MaybeRemovePrefetch,
                 prefetch_manager_, url));
}

void ResourcePrefetchPredictor::SetObserverForTesting(TestObserver* observer) {
  observer_ = observer;
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

  const GURL& main_frame_url = request.navigation_id.main_frame_url;
  StartPrefetching(main_frame_url, PrefetchOrigin::NAVIGATION);

  // Cleanup older navigations.
  CleanupAbandonedNavigations(request.navigation_id);

  // New empty navigation entry.
  inflight_navigations_.insert(
      std::make_pair(request.navigation_id,
                     base::MakeUnique<PageRequestSummary>(main_frame_url)));
}

void ResourcePrefetchPredictor::OnMainFrameResponse(
    const URLRequestSummary& response) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (initialization_state_ != INITIALIZED)
    return;

  StopPrefetching(response.navigation_id.main_frame_url);
}

void ResourcePrefetchPredictor::OnMainFrameRedirect(
    const URLRequestSummary& response) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const GURL& main_frame_url = response.navigation_id.main_frame_url;
  std::unique_ptr<PageRequestSummary> summary;
  NavigationMap::iterator nav_it =
      inflight_navigations_.find(response.navigation_id);
  if (nav_it != inflight_navigations_.end()) {
    summary.reset(nav_it->second.release());
    inflight_navigations_.erase(nav_it);
  }

  // The redirect url may be empty if the URL was invalid.
  if (response.redirect_url.is_empty())
    return;

  // If we lost the information about the first hop for some reason.
  if (!summary) {
    summary = base::MakeUnique<PageRequestSummary>(main_frame_url);
  }

  // A redirect will not lead to another OnMainFrameRequest call, so record the
  // redirect url as a new navigation id and save the initial url.
  NavigationID navigation_id(response.navigation_id);
  navigation_id.main_frame_url = response.redirect_url;
  summary->main_frame_url = response.redirect_url;
  inflight_navigations_.insert(
      std::make_pair(navigation_id, std::move(summary)));
}

void ResourcePrefetchPredictor::OnSubresourceResponse(
    const URLRequestSummary& response) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  NavigationMap::const_iterator nav_it =
        inflight_navigations_.find(response.navigation_id);
  if (nav_it == inflight_navigations_.end()) {
    return;
  }

  nav_it->second->subresource_requests.push_back(response);
}

void ResourcePrefetchPredictor::OnNavigationComplete(
    const NavigationID& nav_id_without_timing_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  NavigationMap::iterator nav_it =
      inflight_navigations_.find(nav_id_without_timing_info);
  if (nav_it == inflight_navigations_.end())
    return;

  // Remove the navigation from the inflight navigations.
  std::unique_ptr<PageRequestSummary> summary = std::move(nav_it->second);
  inflight_navigations_.erase(nav_it);

  // Kick off history lookup to determine if we should record the URL.
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile_,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  DCHECK(history_service);
  history_service->ScheduleDBTask(
      std::unique_ptr<history::HistoryDBTask>(new GetUrlVisitCountTask(
          std::move(summary),
          base::Bind(&ResourcePrefetchPredictor::OnVisitCountLookup,
                     AsWeakPtr()))),
      &history_lookup_consumer_);
}

bool ResourcePrefetchPredictor::GetPrefetchData(const GURL& main_frame_url,
                                                std::vector<GURL>* urls) {
  DCHECK(urls);
  DCHECK(urls->empty());

  // Fetch URLs based on a redirect endpoint for URL/host first.
  std::string redirect_endpoint;
  if (GetRedirectEndpoint(main_frame_url.spec(), *url_redirect_table_cache_,
                          &redirect_endpoint) &&
      PopulatePrefetcherRequest(redirect_endpoint, *url_table_cache_, urls)) {
    return true;
  }

  if (GetRedirectEndpoint(main_frame_url.host(), *host_redirect_table_cache_,
                          &redirect_endpoint) &&
      PopulatePrefetcherRequest(redirect_endpoint, *host_table_cache_, urls)) {
    return true;
  }

  // Fallback to fetching URLs based on the incoming URL/host.
  if (PopulatePrefetcherRequest(main_frame_url.spec(), *url_table_cache_,
                                urls)) {
    return true;
  }

  return PopulatePrefetcherRequest(main_frame_url.host(), *host_table_cache_,
                                   urls);
}

bool ResourcePrefetchPredictor::PopulatePrefetcherRequest(
    const std::string& main_frame_key,
    const PrefetchDataMap& data_map,
    std::vector<GURL>* urls) {
  DCHECK(urls);
  PrefetchDataMap::const_iterator it = data_map.find(main_frame_key);
  if (it == data_map.end())
    return false;

  size_t initial_size = urls->size();
  for (const ResourceData& resource : it->second.resources()) {
    float confidence =
        static_cast<float>(resource.number_of_hits()) /
        (resource.number_of_hits() + resource.number_of_misses());
    if (confidence < config_.min_resource_confidence_to_trigger_prefetch ||
        resource.number_of_hits() <
            config_.min_resource_hits_to_trigger_prefetch)
      continue;

    urls->push_back(GURL(resource.resource_url()));
  }

  return urls->size() > initial_size;
}

void ResourcePrefetchPredictor::CreateCaches(
    std::unique_ptr<PrefetchDataMap> url_data_map,
    std::unique_ptr<PrefetchDataMap> host_data_map,
    std::unique_ptr<RedirectDataMap> url_redirect_data_map,
    std::unique_ptr<RedirectDataMap> host_redirect_data_map) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  DCHECK_EQ(INITIALIZING, initialization_state_);
  DCHECK(!url_table_cache_);
  DCHECK(!host_table_cache_);
  DCHECK(!url_redirect_table_cache_);
  DCHECK(!host_redirect_table_cache_);
  DCHECK(inflight_navigations_.empty());

  url_table_cache_ = std::move(url_data_map);
  host_table_cache_ = std::move(host_data_map);
  url_redirect_table_cache_ = std::move(url_redirect_data_map);
  host_redirect_table_cache_ = std::move(host_redirect_data_map);

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
  if (config_.IsPrefetchingEnabledForSomeOrigin(profile_)) {
    prefetch_manager_ = new ResourcePrefetcherManager(
        this, config_, profile_->GetRequestContext());
  }
  initialization_state_ = INITIALIZED;

  if (observer_)
    observer_->OnPredictorInitialized();
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
}

void ResourcePrefetchPredictor::DeleteAllUrls() {
  inflight_navigations_.clear();
  url_table_cache_->clear();
  host_table_cache_->clear();
  url_redirect_table_cache_->clear();
  host_redirect_table_cache_->clear();

  BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
      base::Bind(&ResourcePrefetchPredictorTables::DeleteAllData, tables_));
}

void ResourcePrefetchPredictor::DeleteUrls(const history::URLRows& urls) {
  // Check all the urls in the database and pick out the ones that are present
  // in the cache.
  std::vector<std::string> urls_to_delete, hosts_to_delete;
  std::vector<std::string> url_redirects_to_delete, host_redirects_to_delete;

  for (const auto& it : urls) {
    const std::string& url_spec = it.url().spec();
    if (url_table_cache_->find(url_spec) != url_table_cache_->end()) {
      urls_to_delete.push_back(url_spec);
      url_table_cache_->erase(url_spec);
    }

    if (url_redirect_table_cache_->find(url_spec) !=
        url_redirect_table_cache_->end()) {
      url_redirects_to_delete.push_back(url_spec);
      url_redirect_table_cache_->erase(url_spec);
    }

    const std::string& host = it.url().host();
    if (host_table_cache_->find(host) != host_table_cache_->end()) {
      hosts_to_delete.push_back(host);
      host_table_cache_->erase(host);
    }

    if (host_redirect_table_cache_->find(host) !=
        host_redirect_table_cache_->end()) {
      host_redirects_to_delete.push_back(host);
      host_redirect_table_cache_->erase(host);
    }
  }

  if (!urls_to_delete.empty() || !hosts_to_delete.empty()) {
    BrowserThread::PostTask(
        BrowserThread::DB, FROM_HERE,
        base::Bind(&ResourcePrefetchPredictorTables::DeleteResourceData,
                   tables_, urls_to_delete, hosts_to_delete));
  }

  if (!url_redirects_to_delete.empty() || !host_redirects_to_delete.empty()) {
    BrowserThread::PostTask(
        BrowserThread::DB, FROM_HERE,
        base::Bind(&ResourcePrefetchPredictorTables::DeleteRedirectData,
                   tables_, url_redirects_to_delete, host_redirects_to_delete));
  }
}

void ResourcePrefetchPredictor::RemoveOldestEntryInPrefetchDataMap(
    PrefetchKeyType key_type,
    PrefetchDataMap* data_map) {
  if (data_map->empty())
    return;

  uint64_t oldest_time = UINT64_MAX;
  std::string key_to_delete;
  for (const auto& kv : *data_map) {
    const PrefetchData& data = kv.second;
    if (key_to_delete.empty() || data.last_visit_time() < oldest_time) {
      key_to_delete = data.primary_key();
      oldest_time = data.last_visit_time();
    }
  }

  data_map->erase(key_to_delete);
  BrowserThread::PostTask(
      BrowserThread::DB, FROM_HERE,
      base::Bind(
          &ResourcePrefetchPredictorTables::DeleteSingleResourceDataPoint,
          tables_, key_to_delete, key_type));
}

void ResourcePrefetchPredictor::RemoveOldestEntryInRedirectDataMap(
    PrefetchKeyType key_type,
    RedirectDataMap* data_map) {
  if (data_map->empty())
    return;

  uint64_t oldest_time = UINT64_MAX;
  std::string key_to_delete;
  for (const auto& kv : *data_map) {
    const RedirectData& data = kv.second;
    if (key_to_delete.empty() || data.last_visit_time() < oldest_time) {
      key_to_delete = data.primary_key();
      oldest_time = data.last_visit_time();
    }
  }

  data_map->erase(key_to_delete);
  BrowserThread::PostTask(
      BrowserThread::DB, FROM_HERE,
      base::Bind(
          &ResourcePrefetchPredictorTables::DeleteSingleRedirectDataPoint,
          tables_, key_to_delete, key_type));
}

void ResourcePrefetchPredictor::OnVisitCountLookup(
    size_t url_visit_count,
    const PageRequestSummary& summary) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  UMA_HISTOGRAM_COUNTS("ResourcePrefetchPredictor.HistoryVisitCountForUrl",
                       url_visit_count);

  // TODO(alexilin): make only one request to DB thread.

  // URL level data - merge only if we already saved the data, or it
  // meets the cutoff requirement.
  const std::string url_spec = summary.main_frame_url.spec();
  bool already_tracking = url_table_cache_->find(url_spec) !=
      url_table_cache_->end();
  bool should_track_url =
      already_tracking || (url_visit_count >= config_.min_url_visit_count);

  if (should_track_url) {
    LearnNavigation(url_spec, PREFETCH_KEY_TYPE_URL,
                    summary.subresource_requests, config_.max_urls_to_track,
                    url_table_cache_.get(), summary.initial_url.spec(),
                    url_redirect_table_cache_.get());
  }

  // Host level data - no cutoff, always learn the navigation if enabled.
  const std::string host = summary.main_frame_url.host();
  LearnNavigation(host, PREFETCH_KEY_TYPE_HOST, summary.subresource_requests,
                  config_.max_hosts_to_track, host_table_cache_.get(),
                  summary.initial_url.host(), host_redirect_table_cache_.get());

  if (observer_)
    observer_->OnNavigationLearned(url_visit_count, summary);
}

void ResourcePrefetchPredictor::LearnNavigation(
    const std::string& key,
    PrefetchKeyType key_type,
    const std::vector<URLRequestSummary>& new_resources,
    size_t max_data_map_size,
    PrefetchDataMap* data_map,
    const std::string& key_before_redirects,
    RedirectDataMap* redirect_map) {
  TRACE_EVENT1("browser", "ResourcePrefetchPredictor::LearnNavigation", "key",
               key);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // If the primary key is too long reject it.
  if (key.length() > ResourcePrefetchPredictorTables::kMaxStringLength)
    return;

  PrefetchDataMap::iterator cache_entry = data_map->find(key);
  if (cache_entry == data_map->end()) {
    // If the table is full, delete an entry.
    if (data_map->size() >= max_data_map_size)
      RemoveOldestEntryInPrefetchDataMap(key_type, data_map);

    cache_entry = data_map->insert(std::make_pair(key, PrefetchData())).first;
    PrefetchData& data = cache_entry->second;
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
      resource_to_add->set_has_validators(summary.has_validators);
      resource_to_add->set_always_revalidate(summary.always_revalidate);

      resources_seen.insert(summary.resource_url);
    }
  } else {
    PrefetchData& data = cache_entry->second;
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
      resource_to_add->set_has_validators(new_resources[i].has_validators);
      resource_to_add->set_always_revalidate(
          new_resources[i].always_revalidate);

      // To ensure we dont add the same url twice.
      old_index[summary.resource_url] = 0;
    }
  }

  PrefetchData& data = cache_entry->second;
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

  // If the row has no resources, remove it from the cache and delete the
  // entry in the database. Else update the database.
  if (data.resources_size() == 0) {
    data_map->erase(key);
    BrowserThread::PostTask(
        BrowserThread::DB, FROM_HERE,
        base::Bind(
            &ResourcePrefetchPredictorTables::DeleteSingleResourceDataPoint,
            tables_, key, key_type));
  } else {
    PrefetchData empty_data;
    RedirectData empty_redirect_data;
    bool is_host = key_type == PREFETCH_KEY_TYPE_HOST;
    const PrefetchData& host_data = is_host ? data : empty_data;
    const PrefetchData& url_data = is_host ? empty_data : data;
    BrowserThread::PostTask(
        BrowserThread::DB, FROM_HERE,
        base::Bind(&ResourcePrefetchPredictorTables::UpdateData, tables_,
                   url_data, host_data, empty_redirect_data,
                   empty_redirect_data));
  }

  if (key != key_before_redirects) {
    LearnRedirect(key_before_redirects, key_type, key, max_data_map_size,
                  redirect_map);
  }
}

void ResourcePrefetchPredictor::LearnRedirect(const std::string& key,
                                              PrefetchKeyType key_type,
                                              const std::string& final_redirect,
                                              size_t max_redirect_map_size,
                                              RedirectDataMap* redirect_map) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // If the primary key is too long reject it.
  if (key.length() > ResourcePrefetchPredictorTables::kMaxStringLength)
    return;

  RedirectDataMap::iterator cache_entry = redirect_map->find(key);
  if (cache_entry == redirect_map->end()) {
    if (redirect_map->size() >= max_redirect_map_size)
      RemoveOldestEntryInRedirectDataMap(key_type, redirect_map);

    cache_entry =
        redirect_map->insert(std::make_pair(key, RedirectData())).first;
    RedirectData& data = cache_entry->second;
    data.set_primary_key(key);
    data.set_last_visit_time(base::Time::Now().ToInternalValue());
    RedirectStat* redirect_to_add = data.add_redirect_endpoints();
    redirect_to_add->set_url(final_redirect);
    redirect_to_add->set_number_of_hits(1);
  } else {
    RedirectData& data = cache_entry->second;
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

  RedirectData& data = cache_entry->second;
  // Trim the redirects after the update.
  ResourcePrefetchPredictorTables::TrimRedirects(
      &data, config_.max_consecutive_misses);

  if (data.redirect_endpoints_size() == 0) {
    redirect_map->erase(cache_entry);
    BrowserThread::PostTask(
        BrowserThread::DB, FROM_HERE,
        base::Bind(
            &ResourcePrefetchPredictorTables::DeleteSingleRedirectDataPoint,
            tables_, key, key_type));
  } else {
    RedirectData empty_redirect_data;
    PrefetchData empty_data;
    bool is_host = key_type == PREFETCH_KEY_TYPE_HOST;
    const RedirectData& host_redirect_data =
        is_host ? data : empty_redirect_data;
    const RedirectData& url_redirect_data =
        is_host ? empty_redirect_data : data;
    BrowserThread::PostTask(
        BrowserThread::DB, FROM_HERE,
        base::Bind(&ResourcePrefetchPredictorTables::UpdateData, tables_,
                   empty_data, empty_data, url_redirect_data,
                   host_redirect_data));
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
