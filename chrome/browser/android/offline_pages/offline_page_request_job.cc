// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/offline_page_request_job.h"

#include <string>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/android/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/android/offline_pages/offline_page_tab_helper.h"
#include "chrome/browser/android/offline_pages/offline_page_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/offline_pages/offline_page_model.h"
#include "components/previews/previews_experiments.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/resource_type.h"
#include "net/base/network_change_notifier.h"
#include "net/http/http_request_headers.h"
#include "net/nqe/network_quality_estimator.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"

namespace offline_pages {

const char kOfflinePageHeader[] = "X-Chrome-offline";
const char kOfflinePageHeaderReasonKey[] = "reason";
const char kOfflinePageHeaderReasonValueDueToNetError[] = "error";
const char kOfflinePageHeaderReasonValueFromDownload[] = "download";
const char kOfflinePageHeaderPersistKey[] = "persist";
const char kOfflinePageHeaderIDKey[] = "id";

namespace {

enum class NetworkState {
  // No network connection.
  DISCONNECTED_NETWORK,
  // Prohibitively slow means that the NetworkQualityEstimator reported a
  // connection slow enough to warrant showing an offline page if available.
  PROHIBITIVELY_SLOW_NETWORK,
  // Network error received due to bad network, i.e. connected to a hotspot or
  // proxy that does not have a working network.
  FLAKY_NETWORK,
  // Network is in working condition.
  CONNECTED_NETWORK,
  // Force to load the offline page if it is available, though network is in
  // working condition.
  FORCE_OFFLINE_ON_CONNECTED_NETWORK
};

// This enum is used to tell all possible outcomes of handling network requests
// that might serve offline contents.
enum class RequestResult {
  OFFLINE_PAGE_SERVED,
  NO_TAB_ID,
  NO_WEB_CONTENTS,
  PAGE_NOT_FRESH,
  OFFLINE_PAGE_NOT_FOUND
};

const char kUserDataKey[] = "offline_page_key";

// Contains the info to handle offline page request.
class OfflinePageRequestInfo : public base::SupportsUserData::Data {
 public:
  OfflinePageRequestInfo() : use_default_(false) {}
  ~OfflinePageRequestInfo() override {}

  static OfflinePageRequestInfo* GetFromRequest(net::URLRequest* request) {
    return static_cast<OfflinePageRequestInfo*>(
        request->GetUserData(&kUserDataKey));
  }

  bool use_default() const { return use_default_; }
  void set_use_default(bool use_default) { use_default_ = use_default; }

 private:
  // True if the next time this request is started, the request should be
  // serviced from the default handler.
  bool use_default_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageRequestInfo);
};

class DefaultDelegate : public OfflinePageRequestJob::Delegate {
 public:
  DefaultDelegate() {}

  content::ResourceRequestInfo::WebContentsGetter
  GetWebContentsGetter(net::URLRequest* request) const override {
    const content::ResourceRequestInfo* info =
        content::ResourceRequestInfo::ForRequest(request);
    return info ? info->GetWebContentsGetterForRequest()
                : content::ResourceRequestInfo::WebContentsGetter();
  }

  OfflinePageRequestJob::Delegate::TabIdGetter GetTabIdGetter() const override {
    return base::Bind(&DefaultDelegate::GetTabId);
  }

 private:
  static bool GetTabId(content::WebContents* web_contents, int* tab_id) {
    return OfflinePageUtils::GetTabId(web_contents, tab_id);
  }

  DISALLOW_COPY_AND_ASSIGN(DefaultDelegate);
};

// Used to parse the extra request header string that defines offline page
// loading behaviors.
class OfflinePageHeader {
 public:
  enum class Reason {
    NET_ERROR,
    DOWNLOAD,
    UNKNOWN
  };

  explicit OfflinePageHeader(const std::string& header_string);
  ~OfflinePageHeader() {}

  bool successfully_parsed() const { return successfully_parsed_; }
  bool need_to_persist() const { return need_to_persist_; }
  Reason reason() const { return reason_; }
  const std::string& id() const { return id_; }

 private:
  // True if the header is present and parsed successfully.
  bool successfully_parsed_;
  // Flag to indicate if the header should be persisted across session restore.
  bool need_to_persist_;
  // Describes the reason to load offline page.
  Reason reason_;
  // The offline ID of the page to load.
  std::string id_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageHeader);
};

OfflinePageHeader::OfflinePageHeader(const std::string& header_string)
    : successfully_parsed_(false),
      need_to_persist_(false),
      reason_(Reason::UNKNOWN) {
  // If the offline header is not present, treat it as not parsed successfully.
  if (header_string.empty())
    return;

  base::StringTokenizer tokenizer(header_string, ", ");
  while (tokenizer.GetNext()) {
    std::string pair = tokenizer.token();
    std::size_t pos = pair.find('=');
    if (pos == std::string::npos)
      return;
    std::string key = base::ToLowerASCII(pair.substr(0, pos));
    std::string value = base::ToLowerASCII(pair.substr(pos + 1));
    if (key == kOfflinePageHeaderPersistKey) {
      need_to_persist_ = (value == "1");
    } else if (key == kOfflinePageHeaderReasonKey) {
      if (value == kOfflinePageHeaderReasonValueDueToNetError)
        reason_ = Reason::NET_ERROR;
      else if (value == kOfflinePageHeaderReasonValueFromDownload)
        reason_ = Reason::DOWNLOAD;
      else
        reason_ = Reason::UNKNOWN;
    } else if (key == kOfflinePageHeaderIDKey) {
      id_ = value;
    }
  }

  successfully_parsed_ = true;
}

bool IsNetworkProhibitivelySlow(net::URLRequest* request) {
  // NetworkQualityEstimator only works when it is enabled.
  if (!previews::IsOfflinePreviewsEnabled())
    return false;

  if (!request->context())
    return false;

  net::NetworkQualityEstimator* network_quality_estimator =
      request->context()->network_quality_estimator();
  if (!network_quality_estimator)
    return false;

  net::EffectiveConnectionType effective_connection_type =
      network_quality_estimator->GetEffectiveConnectionType();
  return effective_connection_type >= net::EFFECTIVE_CONNECTION_TYPE_OFFLINE &&
         effective_connection_type <= net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G;
}

NetworkState GetNetworkState(net::URLRequest* request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  std::string offline_header_string;
  request->extra_request_headers().GetHeader(
      kOfflinePageHeader, &offline_header_string);
  OfflinePageHeader offline_header(offline_header_string);
  if (offline_header.reason() == OfflinePageHeader::Reason::NET_ERROR)
    return NetworkState::FLAKY_NETWORK;

  if (net::NetworkChangeNotifier::IsOffline())
    return NetworkState::DISCONNECTED_NETWORK;

  if (IsNetworkProhibitivelySlow(request))
    return NetworkState::PROHIBITIVELY_SLOW_NETWORK;

  // If custom offline header is present, the offline page should be forced to
  // load even when the network is connected.
  return offline_header.successfully_parsed()
             ? NetworkState::FORCE_OFFLINE_ON_CONNECTED_NETWORK
             : NetworkState::CONNECTED_NETWORK;
}

OfflinePageRequestJob::AggregatedRequestResult
RequestResultToAggregatedRequestResult(
    RequestResult request_result, NetworkState network_state) {
  if (request_result == RequestResult::NO_TAB_ID)
    return OfflinePageRequestJob::AggregatedRequestResult::NO_TAB_ID;

  if (request_result == RequestResult::NO_WEB_CONTENTS)
    return OfflinePageRequestJob::AggregatedRequestResult::NO_WEB_CONTENTS;

  if (request_result == RequestResult::PAGE_NOT_FRESH) {
    DCHECK_EQ(NetworkState::PROHIBITIVELY_SLOW_NETWORK, network_state);
    return OfflinePageRequestJob::AggregatedRequestResult::
        PAGE_NOT_FRESH_ON_PROHIBITIVELY_SLOW_NETWORK;
  }

  if (request_result == RequestResult::OFFLINE_PAGE_NOT_FOUND) {
    switch (network_state) {
      case NetworkState::DISCONNECTED_NETWORK:
        return OfflinePageRequestJob::AggregatedRequestResult::
            PAGE_NOT_FOUND_ON_DISCONNECTED_NETWORK;
      case NetworkState::PROHIBITIVELY_SLOW_NETWORK:
        return OfflinePageRequestJob::AggregatedRequestResult::
            PAGE_NOT_FOUND_ON_PROHIBITIVELY_SLOW_NETWORK;
      case NetworkState::FLAKY_NETWORK:
        return OfflinePageRequestJob::AggregatedRequestResult::
            PAGE_NOT_FOUND_ON_FLAKY_NETWORK;
      case NetworkState::FORCE_OFFLINE_ON_CONNECTED_NETWORK:
        return OfflinePageRequestJob::AggregatedRequestResult::
            PAGE_NOT_FOUND_ON_CONNECTED_NETWORK;
      default:
        NOTREACHED();
    }
  }

  DCHECK_EQ(RequestResult::OFFLINE_PAGE_SERVED, request_result);
  DCHECK_NE(NetworkState::CONNECTED_NETWORK, network_state);
  switch (network_state) {
    case NetworkState::DISCONNECTED_NETWORK:
      return OfflinePageRequestJob::AggregatedRequestResult::
          SHOW_OFFLINE_ON_DISCONNECTED_NETWORK;
    case NetworkState::PROHIBITIVELY_SLOW_NETWORK:
      return OfflinePageRequestJob::AggregatedRequestResult::
          SHOW_OFFLINE_ON_PROHIBITIVELY_SLOW_NETWORK;
    case NetworkState::FLAKY_NETWORK:
      return OfflinePageRequestJob::AggregatedRequestResult::
          SHOW_OFFLINE_ON_FLAKY_NETWORK;
    case NetworkState::FORCE_OFFLINE_ON_CONNECTED_NETWORK:
      return OfflinePageRequestJob::AggregatedRequestResult::
          SHOW_OFFLINE_ON_CONNECTED_NETWORK;
    default:
      NOTREACHED();
   }

   return OfflinePageRequestJob::AggregatedRequestResult::
       AGGREGATED_REQUEST_RESULT_MAX;
}

void ReportRequestResult(
    RequestResult request_result, NetworkState network_state) {
  OfflinePageRequestJob::ReportAggregatedRequestResult(
      RequestResultToAggregatedRequestResult(request_result, network_state));
}

OfflinePageModel* GetOfflinePageModel(
    content::ResourceRequestInfo::WebContentsGetter web_contents_getter) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::WebContents* web_contents = web_contents_getter.Run();
  return web_contents ?
      OfflinePageModelFactory::GetForBrowserContext(
          web_contents->GetBrowserContext()) :
      nullptr;
}

void NotifyOfflineFilePathOnIO(base::WeakPtr<OfflinePageRequestJob> job,
                               const base::FilePath& offline_file_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (!job)
    return;
  job->OnOfflineFilePathAvailable(offline_file_path);
}

// Notifies OfflinePageRequestJob about the offline file path. Note that the
// file path may be empty if not found or on error.
void NotifyOfflineFilePathOnUI(base::WeakPtr<OfflinePageRequestJob> job,
                               const base::FilePath& offline_file_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Delegates to IO thread since OfflinePageRequestJob should only be accessed
  // from IO thread.
  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&NotifyOfflineFilePathOnIO, job, offline_file_path));
}

// Finds the offline file path based on the select page result and network
// state and marks it as accessed.
RequestResult AccessOfflineFile(
    NetworkState network_state,
    base::WeakPtr<OfflinePageRequestJob> job,
    content::ResourceRequestInfo::WebContentsGetter web_contents_getter,
    const OfflinePageItem* offline_page,
    base::FilePath* offline_file_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!offline_page)
    return RequestResult::OFFLINE_PAGE_NOT_FOUND;

  // |web_contents_getter| is passed from IO thread. We need to check if
  // web contents is still valid.
  content::WebContents* web_contents = web_contents_getter.Run();
  if (!web_contents)
    return RequestResult::NO_WEB_CONTENTS;

  // If the page is being loaded on a slow network, only use the offline page
  // if it was created within the past day.
  // TODO(romax): Make the constant be policy driven.
  if (network_state == NetworkState::PROHIBITIVELY_SLOW_NETWORK &&
      base::Time::Now() - offline_page->creation_time >
          base::TimeDelta::FromDays(1)) {
    return RequestResult::PAGE_NOT_FRESH;
  }

  // Since offline page will be loaded, it should be marked as accessed.
  OfflinePageModel* offline_page_model =
      OfflinePageModelFactory::GetForBrowserContext(
          web_contents->GetBrowserContext());
  // |offline_page_model| cannot be null because OfflinePageRequestInterceptor
  // will not be created under incognito mode.
  DCHECK(offline_page_model);
  offline_page_model->MarkPageAccessed(offline_page->offline_id);

  // Save an cached copy of OfflinePageItem such that Tab code can get
  // the loaded offline page immediately.
  OfflinePageTabHelper* tab_helper =
      OfflinePageTabHelper::FromWebContents(web_contents);
  DCHECK(tab_helper);
  tab_helper->SetOfflinePage(
      *offline_page, network_state == NetworkState::PROHIBITIVELY_SLOW_NETWORK);

  *offline_file_path = offline_page->file_path;
  return RequestResult::OFFLINE_PAGE_SERVED;
}

// Handles the result of finding an offline page.
void SucceededToFindOfflinePage(
    NetworkState network_state,
    base::WeakPtr<OfflinePageRequestJob> job,
    content::ResourceRequestInfo::WebContentsGetter web_contents_getter,
    const OfflinePageItem* offline_page) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::FilePath offline_file_path;
  RequestResult request_result = AccessOfflineFile(
      network_state, job, web_contents_getter, offline_page,
      &offline_file_path);

  ReportRequestResult(request_result, network_state);

  // NotifyOfflineFilePathOnUI should always be called regardless the failure
  // result and empty file path such that OfflinePageRequestJob will be notified
  // on failure.
  NotifyOfflineFilePathOnUI(job, offline_file_path);
}

void FailedToFindOfflinePage(base::WeakPtr<OfflinePageRequestJob> job) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Proceed with empty file path in order to notify the OfflinePageRequestJob
  // about the failure.
  base::FilePath empty_file_path;
  NotifyOfflineFilePathOnUI(job, empty_file_path);
}

// Tries to find the offline page to serve for |online_url|.
void SelectPageForOnlineURL(
    const GURL& online_url,
    NetworkState network_state,
    content::ResourceRequestInfo::WebContentsGetter web_contents_getter,
    OfflinePageRequestJob::Delegate::TabIdGetter tab_id_getter,
    base::WeakPtr<OfflinePageRequestJob> job) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::WebContents* web_contents = web_contents_getter.Run();
  if (!web_contents){
    ReportRequestResult(RequestResult::NO_WEB_CONTENTS, network_state);
    FailedToFindOfflinePage(job);
    return;
  }
  int tab_id;
  if (!tab_id_getter.Run(web_contents, &tab_id)) {
    ReportRequestResult(RequestResult::NO_TAB_ID, network_state);
    FailedToFindOfflinePage(job);
    return;
  }

  OfflinePageUtils::SelectPageForOnlineURL(
      web_contents->GetBrowserContext(),
      online_url,
      tab_id,
      base::Bind(&SucceededToFindOfflinePage,
                 network_state,
                 job,
                 web_contents_getter));
}

void FindPageWithOfflineIDDone(
    const GURL& online_url,
    NetworkState network_state,
    content::ResourceRequestInfo::WebContentsGetter web_contents_getter,
    OfflinePageRequestJob::Delegate::TabIdGetter tab_id_getter,
    base::WeakPtr<OfflinePageRequestJob> job,
    const OfflinePageItem* offline_page) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // If the found offline page does not has same URL as the request URL, fall
  // back to find the offline page based on the URL.
  if (!offline_page || offline_page->url != online_url) {
    SelectPageForOnlineURL(
        online_url, network_state, web_contents_getter, tab_id_getter, job);
    return;
  }

  SucceededToFindOfflinePage(
      network_state, job, web_contents_getter, offline_page);
}

// Tries to find an offline page associated with |offline_id|.
void FindPageWithOfflineID(
    const GURL& online_url,
    int64_t offline_id,
    NetworkState network_state,
    content::ResourceRequestInfo::WebContentsGetter web_contents_getter,
    OfflinePageRequestJob::Delegate::TabIdGetter tab_id_getter,
    base::WeakPtr<OfflinePageRequestJob> job) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  OfflinePageModel* offline_page_model =
      GetOfflinePageModel(web_contents_getter);
  if (!offline_page_model) {
    FailedToFindOfflinePage(job);
    return;
  }

  offline_page_model->GetPageByOfflineId(
      offline_id,
      base::Bind(&FindPageWithOfflineIDDone,
                 online_url,
                 network_state,
                 web_contents_getter,
                 tab_id_getter,
                 job));
}

// Tries to find the offline page to serve for |online_url|.
void SelectPage(
    const GURL& online_url,
    const std::string& offline_header_string,
    NetworkState network_state,
    content::ResourceRequestInfo::WebContentsGetter web_contents_getter,
    OfflinePageRequestJob::Delegate::TabIdGetter tab_id_getter,
    base::WeakPtr<OfflinePageRequestJob> job) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  OfflinePageHeader offline_header(offline_header_string);

  // If an offline ID is present in the offline header, try to load that
  // particular version.
  if (!offline_header.id().empty()) {
    // if the id string cannot be converted to int64 id, fall through to
    // select page via online URL.
    int64_t offline_id;
    if (base::StringToInt64(offline_header.id(), &offline_id)) {
      FindPageWithOfflineID(online_url, offline_id, network_state,
                            web_contents_getter, tab_id_getter, job);
      return;
    }
  }

  SelectPageForOnlineURL(online_url, network_state, web_contents_getter,
                         tab_id_getter, job);
}

}  // namespace

// static
void OfflinePageRequestJob::ReportAggregatedRequestResult(
    AggregatedRequestResult result) {
  UMA_HISTOGRAM_ENUMERATION("OfflinePages.AggregatedRequestResult",
      static_cast<int>(result),
      static_cast<int>(AggregatedRequestResult::AGGREGATED_REQUEST_RESULT_MAX));
}

// static
OfflinePageRequestJob* OfflinePageRequestJob::Create(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) {
  const content::ResourceRequestInfo* resource_request_info =
      content::ResourceRequestInfo::ForRequest(request);
  if (!resource_request_info)
    return nullptr;

  // Ignore the requests not for the main resource.
  if (resource_request_info->GetResourceType() !=
      content::RESOURCE_TYPE_MAIN_FRAME) {
    return nullptr;
  }

  // Ignore non-http/https requests.
  if (!request->url().SchemeIsHTTPOrHTTPS())
    return nullptr;

  // Ignore requests other than GET.
  if (request->method() != "GET")
    return nullptr;

  OfflinePageRequestInfo* info =
      OfflinePageRequestInfo::GetFromRequest(request);
  if (info) {
    // Fall back to default which is set when an offline page cannot be served,
    // either page not found or online version desired.
    if (info->use_default())
      return nullptr;
  } else {
    request->SetUserData(&kUserDataKey, new OfflinePageRequestInfo());
  }

  return new OfflinePageRequestJob(request, network_delegate);
}

OfflinePageRequestJob::OfflinePageRequestJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate)
    : net::URLRequestFileJob(
          request,
          network_delegate,
          base::FilePath(),
          content::BrowserThread::GetBlockingPool()->
              GetTaskRunnerWithShutdownBehavior(
                  base::SequencedWorkerPool::SKIP_ON_SHUTDOWN)),
      delegate_(new DefaultDelegate()),
      weak_ptr_factory_(this) {
}

OfflinePageRequestJob::~OfflinePageRequestJob() {
}

void OfflinePageRequestJob::Start() {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&OfflinePageRequestJob::StartAsync,
                            weak_ptr_factory_.GetWeakPtr()));
}

void OfflinePageRequestJob::StartAsync() {
  NetworkState network_state = GetNetworkState(request());
  if (network_state == NetworkState::CONNECTED_NETWORK) {
    FallbackToDefault();
    return;
  }

  std::string offline_header_string;
  request()->extra_request_headers().GetHeader(kOfflinePageHeader,
                                               &offline_header_string);
  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&SelectPage,
                 request()->url(),
                 offline_header_string,
                 network_state,
                 delegate_->GetWebContentsGetter(request()),
                 delegate_->GetTabIdGetter(),
                 weak_ptr_factory_.GetWeakPtr()));
}

void OfflinePageRequestJob::Kill() {
  net::URLRequestJob::Kill();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void OfflinePageRequestJob::FallbackToDefault() {
  OfflinePageRequestInfo* info =
      OfflinePageRequestInfo::GetFromRequest(request());
  DCHECK(info);
  info->set_use_default(true);

  URLRequestJob::NotifyRestartRequired();
}

void OfflinePageRequestJob::OnOfflineFilePathAvailable(
    const base::FilePath& offline_file_path) {
  // If offline file path is empty, it means that offline page cannot be found
  // and we want to restart the job to fall back to the default handling.
  if (offline_file_path.empty()) {
    FallbackToDefault();
    return;
  }

  // Sets the file path and lets URLRequestFileJob start to read from the file.
  file_path_ = offline_file_path;
  URLRequestFileJob::Start();
}

void OfflinePageRequestJob::SetDelegateForTesting(
    std::unique_ptr<Delegate> delegate) {
  delegate_ = std::move(delegate);
}

}  // namespace offline_pages
