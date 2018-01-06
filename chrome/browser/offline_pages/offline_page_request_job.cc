// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/offline_page_request_job.h"

#include <string>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/offline_pages/offline_page_tab_helper.h"
#include "chrome/browser/offline_pages/offline_page_utils.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/renderer_host/chrome_navigation_ui_data.h"
#include "components/offline_pages/core/archive_validator.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "components/offline_pages/core/request_header/offline_page_header.h"
#include "components/previews/core/previews_decider.h"
#include "components/previews/core/previews_experiments.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/resource_type.h"
#include "net/base/network_change_notifier.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_redirect_job.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace offline_pages {

namespace {

enum class NetworkState {
  // No network connection.
  DISCONNECTED_NETWORK,
  // Prohibitively slow means that the NetworkQualityEstimator reported a
  // connection slow enough to warrant showing an offline page if available.
  // This requires offline previews to be enabled and the URL of the request to
  // be allowed by previews.
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
  // Offline page was shown for current URL.
  OFFLINE_PAGE_SERVED,
  // Redirected from original URL to final URL in preparation to show the
  // offline page under final URL. OFFLINE_PAGE_SERVED is most likely to be
  // reported next if no other error is encountered.
  REDIRECTED,
  // Tab was gone.
  NO_TAB_ID,
  // Web contents was gone.
  NO_WEB_CONTENTS,
  // The offline page found was not fresh enough, i.e. not created in the past
  // day. This only applies in prohibitively slow network.
  PAGE_NOT_FRESH,
  // Offline page was not found, by searching with either final URL or original
  // URL.
  OFFLINE_PAGE_NOT_FOUND,
  // Digest for the underlying file does not match with the one saved in the
  // metadata database.
  DIGEST_MISMATCH,
};

const char kUserDataKey[] = "offline_page_key";

void ValidatePage(
    const GURL& url,
    const OfflinePageHeader& offline_header,
    NetworkState network_state,
    base::WeakPtr<OfflinePageRequestJob> job,
    content::ResourceRequestInfo::WebContentsGetter web_contents_getter,
    const std::vector<OfflinePageItem>& offline_pages,
    size_t offline_page_index);

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

NetworkState GetNetworkState(net::URLRequest* request,
                             const OfflinePageHeader& offline_header,
                             previews::PreviewsDecider* previews_decider) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (offline_header.reason == OfflinePageHeader::Reason::NET_ERROR)
    return NetworkState::FLAKY_NETWORK;

  if (net::NetworkChangeNotifier::IsOffline())
    return NetworkState::DISCONNECTED_NETWORK;

  // If RELOAD is present in the offline header, load the live page.
  if (offline_header.reason == OfflinePageHeader::Reason::RELOAD)
    return NetworkState::CONNECTED_NETWORK;

  // If other reason is present in the offline header, force loading the offline
  // page even when the network is connected.
  if (offline_header.reason != OfflinePageHeader::Reason::NONE) {
    return NetworkState::FORCE_OFFLINE_ON_CONNECTED_NETWORK;
  }

  // Checks if previews are allowed, the network is slow, and the request is
  // allowed to be shown for previews. When reloading from an offline page or
  // through other force checks, previews should not be considered; previews
  // eligiblity is only checked when |offline_header.reason| is Reason::NONE.
  if (previews_decider && previews_decider->ShouldAllowPreview(
                              *request, previews::PreviewsType::OFFLINE)) {
    return NetworkState::PROHIBITIVELY_SLOW_NETWORK;
  }

  // Otherwise, the network state is a good network.
  return NetworkState::CONNECTED_NETWORK;
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
      case NetworkState::CONNECTED_NETWORK:
        break;
    }
    NOTREACHED();
  }

  if (request_result == RequestResult::REDIRECTED) {
    switch (network_state) {
      case NetworkState::DISCONNECTED_NETWORK:
        return OfflinePageRequestJob::AggregatedRequestResult::
            REDIRECTED_ON_DISCONNECTED_NETWORK;
      case NetworkState::PROHIBITIVELY_SLOW_NETWORK:
        return OfflinePageRequestJob::AggregatedRequestResult::
            REDIRECTED_ON_PROHIBITIVELY_SLOW_NETWORK;
      case NetworkState::FLAKY_NETWORK:
        return OfflinePageRequestJob::AggregatedRequestResult::
            REDIRECTED_ON_FLAKY_NETWORK;
      case NetworkState::FORCE_OFFLINE_ON_CONNECTED_NETWORK:
        return OfflinePageRequestJob::AggregatedRequestResult::
            REDIRECTED_ON_CONNECTED_NETWORK;
      case NetworkState::CONNECTED_NETWORK:
        break;
    }
    NOTREACHED();
  }

  if (request_result == RequestResult::DIGEST_MISMATCH) {
    switch (network_state) {
      case NetworkState::DISCONNECTED_NETWORK:
        return OfflinePageRequestJob::AggregatedRequestResult::
            DIGEST_MISMATCH_ON_DISCONNECTED_NETWORK;
      case NetworkState::PROHIBITIVELY_SLOW_NETWORK:
        return OfflinePageRequestJob::AggregatedRequestResult::
            DIGEST_MISMATCH_ON_PROHIBITIVELY_SLOW_NETWORK;
      case NetworkState::FLAKY_NETWORK:
        return OfflinePageRequestJob::AggregatedRequestResult::
            DIGEST_MISMATCH_ON_FLAKY_NETWORK;
      case NetworkState::FORCE_OFFLINE_ON_CONNECTED_NETWORK:
      case NetworkState::CONNECTED_NETWORK:
        return OfflinePageRequestJob::AggregatedRequestResult::
            DIGEST_MISMATCH_ON_CONNECTED_NETWORK;
    }
    NOTREACHED();
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
    case NetworkState::CONNECTED_NETWORK:
      break;
  }
  NOTREACHED();

  return OfflinePageRequestJob::AggregatedRequestResult::
      AGGREGATED_REQUEST_RESULT_MAX;
}

void ReportRequestResult(RequestResult request_result,
                         NetworkState network_state) {
  OfflinePageRequestJob::ReportAggregatedRequestResult(
      RequestResultToAggregatedRequestResult(request_result, network_state));
}

void ReportOfflinePageSize(NetworkState network_state,
                           const OfflinePageItem& offline_page) {
  if (offline_page.client_id.name_space.empty())
    return;

  // The two histograms report values between 1KiB and 100MiB.
  switch (network_state) {
    case NetworkState::DISCONNECTED_NETWORK:        // Fall-through
    case NetworkState::PROHIBITIVELY_SLOW_NETWORK:  // Fall-through
    case NetworkState::FLAKY_NETWORK:
      base::UmaHistogramCounts100000("OfflinePages.PageSizeOnAccess.Offline." +
                                         offline_page.client_id.name_space,
                                     offline_page.file_size / 1024);
      return;
    case NetworkState::FORCE_OFFLINE_ON_CONNECTED_NETWORK:
      base::UmaHistogramCounts100000("OfflinePages.PageSizeOnAccess.Online." +
                                         offline_page.client_id.name_space,
                                     offline_page.file_size / 1024);
      return;
    case NetworkState::CONNECTED_NETWORK:
      break;
  }
  NOTREACHED();
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

void ValidateFileOnBackgroundThread(
    const base::FilePath& file_path,
    int64_t expected_file_size,
    const std::string& expected_digest,
    const base::Callback<void(bool)>& callback) {
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BACKGROUND},
      base::Bind(&ArchiveValidator::ValidateFile, file_path, expected_file_size,
                 expected_digest),
      callback);
}

void NotifyOfflineFilePathOnIO(base::WeakPtr<OfflinePageRequestJob> job,
                               const std::string& name_space,
                               const base::FilePath& offline_file_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (!job)
    return;
  job->OnOfflineFilePathAvailable(name_space, offline_file_path);
}

void NotifyOfflineRedirectOnIO(base::WeakPtr<OfflinePageRequestJob> job,
                               const GURL& redirected_url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (!job)
    return;
  job->OnOfflineRedirectAvailabe(redirected_url);
}

// Notifies OfflinePageRequestJob about the offline file path. Note that the
// file path may be empty if not found or on error.
void NotifyOfflineFilePathOnUI(base::WeakPtr<OfflinePageRequestJob> job,
                               const std::string& name_space,
                               const base::FilePath& offline_file_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Delegates to IO thread since OfflinePageRequestJob should only be accessed
  // from IO thread.
  content::BrowserThread::PostTask(content::BrowserThread::IO, FROM_HERE,
                                   base::Bind(&NotifyOfflineFilePathOnIO, job,
                                              name_space, offline_file_path));
}

// Notifies OfflinePageRequestJob about the redirected URL. Note that
// redirected_url should not be empty.
void NotifyOfflineRedirectOnUI(base::WeakPtr<OfflinePageRequestJob> job,
                               const GURL& redirected_url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!redirected_url.is_empty());

  // Delegates to IO thread since OfflinePageRequestJob should only be accessed
  // from IO thread.
  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&NotifyOfflineRedirectOnIO, job, redirected_url));
}

// Failed to find a trusted offline page.
void FailedToFindTrustedOfflinePage(RequestResult request_error_result,
                                    NetworkState network_state,
                                    base::WeakPtr<OfflinePageRequestJob> job) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_NE(RequestResult::OFFLINE_PAGE_SERVED, request_error_result);

  ReportRequestResult(request_error_result, network_state);

  // Proceed with empty file path in order to notify the OfflinePageRequestJob
  // about the failure.
  base::FilePath empty_file_path;
  NotifyOfflineFilePathOnUI(job, std::string(), empty_file_path);
}

// Succeeded to find a trusted offline page which can be served.
void SucceededToFindTrustedOfflinePage(
    const GURL& url,
    const OfflinePageHeader& offline_header,
    NetworkState network_state,
    base::WeakPtr<OfflinePageRequestJob> job,
    content::ResourceRequestInfo::WebContentsGetter web_contents_getter,
    const OfflinePageItem& offline_page) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // |web_contents_getter| is passed from IO thread. We need to check if
  // web contents is still valid.
  content::WebContents* web_contents = web_contents_getter.Run();
  if (!web_contents) {
    FailedToFindTrustedOfflinePage(RequestResult::NO_WEB_CONTENTS,
                                   network_state, job);
    return;
  }

  // If the match is for original URL, trigger the redirect.
  // Note: If the offline page has same orginal URL and final URL, don't trigger
  // the redirect. Some websites might route the redirect finally back to itself
  // after intermediate redirects for authentication. Previously this case was
  // not handled and some pages might be saved with same URLs. Though we fixed
  // the problem, we still need to support those pages already saved with this
  if (url == offline_page.original_url && url != offline_page.url) {
    ReportRequestResult(RequestResult::REDIRECTED, network_state);
    NotifyOfflineRedirectOnUI(job, offline_page.url);
    return;
  }

  ReportRequestResult(RequestResult::OFFLINE_PAGE_SERVED, network_state);
  ReportOfflinePageSize(network_state, offline_page);

  // Since offline page will be loaded, it should be marked as accessed.
  OfflinePageModel* offline_page_model =
      OfflinePageModelFactory::GetForBrowserContext(
          web_contents->GetBrowserContext());
  // |offline_page_model| cannot be null because OfflinePageRequestInterceptor
  // will not be created under incognito mode.
  DCHECK(offline_page_model);
  offline_page_model->MarkPageAccessed(offline_page.offline_id);

  // Save an cached copy of OfflinePageItem such that Tab code can get
  // the loaded offline page immediately.
  OfflinePageTabHelper* tab_helper =
      OfflinePageTabHelper::FromWebContents(web_contents);
  DCHECK(tab_helper);
  tab_helper->SetOfflinePage(
      offline_page, offline_header, true /*is_trusted*/,
      network_state == NetworkState::PROHIBITIVELY_SLOW_NETWORK);

  // NotifyOfflineFilePathOnUI should always be called regardless the failure
  // result and empty file path such that OfflinePageRequestJob will be notified
  // on failure.
  NotifyOfflineFilePathOnUI(job, offline_page.client_id.name_space,
                            offline_page.file_path);
}

void ValidatePageDone(
    const GURL& url,
    const OfflinePageHeader& offline_header,
    NetworkState network_state,
    base::WeakPtr<OfflinePageRequestJob> job,
    content::ResourceRequestInfo::WebContentsGetter web_contents_getter,
    const std::vector<OfflinePageItem>& offline_pages,
    size_t offline_page_index,
    bool successfully_validated) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_LT(offline_page_index, offline_pages.size());

  if (successfully_validated) {
    SucceededToFindTrustedOfflinePage(url, offline_header, network_state, job,
                                      web_contents_getter,
                                      offline_pages[offline_page_index]);
    return;
  }

  // Bail out if no more page to validate.
  offline_page_index++;
  if (offline_page_index >= offline_pages.size()) {
    FailedToFindTrustedOfflinePage(RequestResult::DIGEST_MISMATCH,
                                   network_state, job);
    return;
  }

  // Move to validating next page.
  ValidatePage(url, offline_header, network_state, job, web_contents_getter,
               offline_pages, offline_page_index);
}

// Validate an offline page at |offline_page_index|.
void ValidatePage(
    const GURL& url,
    const OfflinePageHeader& offline_header,
    NetworkState network_state,
    base::WeakPtr<OfflinePageRequestJob> job,
    content::ResourceRequestInfo::WebContentsGetter web_contents_getter,
    const std::vector<OfflinePageItem>& offline_pages,
    size_t offline_page_index) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_LT(offline_page_index, offline_pages.size());

  const OfflinePageItem& offline_page = offline_pages[offline_page_index];

  // If the page is being loaded on a slow network, only use the offline page
  // if it was created within the past day.
  if (network_state == NetworkState::PROHIBITIVELY_SLOW_NETWORK &&
      base::Time::Now() - offline_page.creation_time >
          previews::params::OfflinePreviewFreshnessDuration()) {
    FailedToFindTrustedOfflinePage(RequestResult::PAGE_NOT_FRESH, network_state,
                                   job);
    return;
  }

  OfflinePageModel* offline_page_model =
      GetOfflinePageModel(web_contents_getter);
  if (!offline_page_model) {
    FailedToFindTrustedOfflinePage(RequestResult::NO_WEB_CONTENTS,
                                   network_state, job);
    return;
  }

  bool can_skip_file_validation = false;
  bool is_page_trusted = false;

  // If the file is in internal directory, the page can be trusted without
  // validation.
  if (offline_page_model->IsArchiveInInternalDir(offline_page.file_path)) {
    can_skip_file_validation = true;
    is_page_trusted = true;
  } else if (offline_page.digest.empty()) {
    // Otherwise, the file is in public directory. If the digest is not found in
    // the metadata database, it means that the digest is erased when the file
    // modification is detected. In this case, the page cannot be trusted.
    can_skip_file_validation = true;
    is_page_trusted = false;
  }

  if (can_skip_file_validation) {
    ValidatePageDone(url, offline_header, network_state, job,
                     web_contents_getter, offline_pages, offline_page_index,
                     is_page_trusted);
    return;
  }

  // Validate if the file has been modified.
  ValidateFileOnBackgroundThread(
      offline_page.file_path, offline_page.file_size, offline_page.digest,
      base::Bind(&ValidatePageDone, url, offline_header, network_state, job,
                 web_contents_getter, offline_pages, offline_page_index));
}

// Handles the result of finding matched offline pages.
void SelectPagesForURLDone(
    const GURL& url,
    const OfflinePageHeader& offline_header,
    NetworkState network_state,
    base::WeakPtr<OfflinePageRequestJob> job,
    content::ResourceRequestInfo::WebContentsGetter web_contents_getter,
    const std::vector<OfflinePageItem>& offline_pages) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Bail out if no page is found.
  if (offline_pages.empty()) {
    FailedToFindTrustedOfflinePage(RequestResult::OFFLINE_PAGE_NOT_FOUND,
                                   network_state, job);
    return;
  }

  // Start to validate the first page.
  ValidatePage(url, offline_header, network_state, job, web_contents_getter,
               offline_pages, 0);
}

// Handles the result of finding an offline page with the requested offline ID.
void GetPageByOfflineIdDone(
    const GURL& url,
    const OfflinePageHeader& offline_header,
    NetworkState network_state,
    content::ResourceRequestInfo::WebContentsGetter web_contents_getter,
    base::WeakPtr<OfflinePageRequestJob> job,
    const OfflinePageItem* offline_page) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // If the found offline page does not match the request URL, fail.
  if (!offline_page || offline_page->url != url) {
    FailedToFindTrustedOfflinePage(RequestResult::OFFLINE_PAGE_NOT_FOUND,
                                   network_state, job);
    return;
  }

  // Continue with the validation step.
  std::vector<OfflinePageItem> offline_pages;
  offline_pages.push_back(*offline_page);
  SelectPagesForURLDone(url, offline_header, network_state, job,
                        web_contents_getter, offline_pages);
}

// Tries to find the offline page to serve for |url|.
void GetPageToServeURL(
    const GURL& url,
    const OfflinePageHeader& offline_header,
    NetworkState network_state,
    content::ResourceRequestInfo::WebContentsGetter web_contents_getter,
    OfflinePageRequestJob::Delegate::TabIdGetter tab_id_getter,
    base::WeakPtr<OfflinePageRequestJob> job) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::WebContents* web_contents = web_contents_getter.Run();
  if (!web_contents) {
    FailedToFindTrustedOfflinePage(RequestResult::NO_WEB_CONTENTS,
                                   network_state, job);
    return;
  }
  int tab_id;
  if (!tab_id_getter.Run(web_contents, &tab_id)) {
    FailedToFindTrustedOfflinePage(RequestResult::NO_TAB_ID, network_state,
                                   job);
    return;
  }

  // If an int64 offline ID is present in the offline header, try to load that
  // particular version.
  if (!offline_header.id.empty()) {
    int64_t offline_id;
    if (base::StringToInt64(offline_header.id, &offline_id)) {
      OfflinePageModel* offline_page_model =
          OfflinePageModelFactory::GetForBrowserContext(
              web_contents->GetBrowserContext());
      DCHECK(offline_page_model);
      offline_page_model->GetPageByOfflineId(
          offline_id, base::Bind(&GetPageByOfflineIdDone, url, offline_header,
                                 network_state, web_contents_getter, job));
      return;
    }
  }

  OfflinePageUtils::SelectPagesForURL(
      web_contents->GetBrowserContext(), url, URLSearchMode::SEARCH_BY_ALL_URLS,
      tab_id,
      base::Bind(&SelectPagesForURLDone, url, offline_header, network_state,
                 job, web_contents_getter));
}

void ReportAccessEntryPoint(
    const std::string& name_space,
    OfflinePageRequestJob::AccessEntryPoint entry_point) {
  base::UmaHistogramEnumeration("OfflinePages.AccessEntryPoint." + name_space,
                                entry_point,
                                OfflinePageRequestJob::AccessEntryPoint::COUNT);
}

}  // namespace

// static
void OfflinePageRequestJob::ReportAggregatedRequestResult(
    AggregatedRequestResult result) {
  UMA_HISTOGRAM_ENUMERATION("OfflinePages.AggregatedRequestResult2",
      static_cast<int>(result),
      static_cast<int>(AggregatedRequestResult::AGGREGATED_REQUEST_RESULT_MAX));
}

// static
OfflinePageRequestJob* OfflinePageRequestJob::Create(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    previews::PreviewsDecider* previews_decider) {
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
    request->SetUserData(&kUserDataKey,
                         base::MakeUnique<OfflinePageRequestInfo>());
  }

  return new OfflinePageRequestJob(request, network_delegate, previews_decider);
}

OfflinePageRequestJob::OfflinePageRequestJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    previews::PreviewsDecider* previews_decider)
    : net::URLRequestFileJob(
          request,
          network_delegate,
          base::FilePath(),
          base::CreateTaskRunnerWithTraits(
              {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
               base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      delegate_(new DefaultDelegate()),
      previews_decider_(previews_decider),
      weak_ptr_factory_(this) {}

OfflinePageRequestJob::~OfflinePageRequestJob() {
}

void OfflinePageRequestJob::Start() {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&OfflinePageRequestJob::StartAsync,
                            weak_ptr_factory_.GetWeakPtr()));
}

void OfflinePageRequestJob::StartAsync() {
  std::string offline_header_value;
  request()->extra_request_headers().GetHeader(
      kOfflinePageHeader, &offline_header_value);
  // Note that |offline_header| will be empty if parsing from the header value
  // fails.
  OfflinePageHeader offline_header(offline_header_value);

  NetworkState network_state =
      GetNetworkState(request(), offline_header, previews_decider_);
  if (network_state == NetworkState::CONNECTED_NETWORK) {
    FallbackToDefault();
    return;
  }

  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&GetPageToServeURL, request()->url(), offline_header,
                 network_state, delegate_->GetWebContentsGetter(request()),
                 delegate_->GetTabIdGetter(), weak_ptr_factory_.GetWeakPtr()));
}

void OfflinePageRequestJob::Kill() {
  net::URLRequestJob::Kill();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

bool OfflinePageRequestJob::IsRedirectResponse(GURL* location,
                                               int* http_status_code) {
  // OfflinePageRequestJob derives from URLRequestFileJob which derives from
  // URLRequestJob. We need to invoke the implementation of IsRedirectResponse
  // in URLRequestJob directly since URLRequestFileJob overrode it with the
  // logic we don't want.
  return URLRequestJob::IsRedirectResponse(location, http_status_code);
}

void OfflinePageRequestJob::GetResponseInfo(net::HttpResponseInfo* info) {
  if (!fake_headers_for_redirect_) {
    URLRequestFileJob::GetResponseInfo(info);
    return;
  }

  info->headers = fake_headers_for_redirect_;
  info->request_time = redirect_response_time_;
  info->response_time = redirect_response_time_;
}

void OfflinePageRequestJob::GetLoadTimingInfo(
    net::LoadTimingInfo* load_timing_info) const {
  // Set send_start and send_end to receive_redirect_headers_end_ to be
  // consistent with network cache behavior.
  load_timing_info->send_start = receive_redirect_headers_end_;
  load_timing_info->send_end = receive_redirect_headers_end_;
  load_timing_info->receive_headers_end = receive_redirect_headers_end_;
}

bool OfflinePageRequestJob::CopyFragmentOnRedirect(const GURL& location) const {
  return false;
}

void OfflinePageRequestJob::OnOpenComplete(int result) {
  base::UmaHistogramSparse("OfflinePages.RequestJob.OpenFileErrorCode",
                           -result);
}

void OfflinePageRequestJob::OnSeekComplete(int64_t result) {
  if (result < 0) {
    base::UmaHistogramSparse("OfflinePages.RequestJob.SeekFileErrorCode",
                             static_cast<int>(-result));
  }
}

void OfflinePageRequestJob::OnReadComplete(net::IOBuffer* buf, int result) {
  if (result < 0) {
    base::UmaHistogramSparse("OfflinePages.RequestJob.ReadFileErrorCode",
                             -result);
  }
}

void OfflinePageRequestJob::FallbackToDefault() {
  OfflinePageRequestInfo* info =
      OfflinePageRequestInfo::GetFromRequest(request());
  DCHECK(info);
  info->set_use_default(true);

  URLRequestJob::NotifyRestartRequired();
}

void OfflinePageRequestJob::OnOfflineFilePathAvailable(
    const std::string& name_space,
    const base::FilePath& offline_file_path) {
  // If offline file path is empty, it means that offline page cannot be found
  // and we want to restart the job to fall back to the default handling.
  if (offline_file_path.empty()) {
    FallbackToDefault();
    return;
  }

  ReportAccessEntryPoint(name_space, GetAccessEntryPoint());

  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request());
  ChromeNavigationUIData* navigation_data =
      static_cast<ChromeNavigationUIData*>(info->GetNavigationUIData());
  if (navigation_data) {
    std::unique_ptr<OfflinePageNavigationUIData> offline_page_data =
        std::make_unique<OfflinePageNavigationUIData>(true);
    navigation_data->SetOfflinePageNavigationUIData(
        std::move(offline_page_data));
  }

  // Sets the file path and lets URLRequestFileJob start to read from the file.
  file_path_ = offline_file_path;
  URLRequestFileJob::Start();
}

void OfflinePageRequestJob::OnOfflineRedirectAvailabe(
    const GURL& redirected_url) {
  receive_redirect_headers_end_ = base::TimeTicks::Now();
  redirect_response_time_ = base::Time::Now();

  std::string header_string =
      base::StringPrintf("HTTP/1.1 %i Internal Redirect\n"
                             // Clear referrer to avoid leak when going online.
                             "Referrer-Policy: no-referrer\n"
                             "Location: %s\n"
                             "Non-Authoritative-Reason: offline redirects",
                         // 302 is used to remove response bodies in order to
                         // avoid leak when going online.
                         net::URLRequestRedirectJob::REDIRECT_302_FOUND,
                         redirected_url.spec().c_str());

  fake_headers_for_redirect_ = new net::HttpResponseHeaders(
      net::HttpUtil::AssembleRawHeaders(header_string.c_str(),
                                        header_string.length()));
  DCHECK(fake_headers_for_redirect_->IsRedirect(nullptr));

  URLRequestJob::NotifyHeadersComplete();
}

// Returns true to disable the file path checking for file: scheme in
// URLRequestFileJob, that's not relevant for this class.
bool OfflinePageRequestJob::CanAccessFile(const base::FilePath& original_path,
                                          const base::FilePath& absolute_path) {
  return true;
}

void OfflinePageRequestJob::SetDelegateForTesting(
    std::unique_ptr<Delegate> delegate) {
  delegate_ = std::move(delegate);
}

OfflinePageRequestJob::AccessEntryPoint
OfflinePageRequestJob::GetAccessEntryPoint() const {
  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request());
  if (!info)
    return AccessEntryPoint::UNKNOWN;

  std::string offline_header_value;
  request()->extra_request_headers().GetHeader(kOfflinePageHeader,
                                               &offline_header_value);
  OfflinePageHeader offline_header(offline_header_value);
  if (offline_header.reason == OfflinePageHeader::Reason::DOWNLOAD)
    return AccessEntryPoint::DOWNLOADS;

  ui::PageTransition transition = info->GetPageTransition();
  if (ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_LINK)) {
    return PageTransitionGetQualifier(transition) ==
                   static_cast<int>(ui::PAGE_TRANSITION_FROM_API)
               ? AccessEntryPoint::CCT
               : AccessEntryPoint::LINK;
  } else if (ui::PageTransitionCoreTypeIs(transition,
                                          ui::PAGE_TRANSITION_TYPED) ||
             ui::PageTransitionCoreTypeIs(transition,
                                          ui::PAGE_TRANSITION_GENERATED)) {
    return AccessEntryPoint::OMNIBOX;
  } else if (ui::PageTransitionCoreTypeIs(transition,
                                          ui::PAGE_TRANSITION_AUTO_BOOKMARK)) {
    // Note that this also includes launching from bookmark which tends to be
    // less likely. For now we don't separate these two.
    return AccessEntryPoint::NTP_SUGGESTIONS_OR_BOOKMARKS;
  } else if (ui::PageTransitionCoreTypeIs(transition,
                                          ui::PAGE_TRANSITION_RELOAD)) {
    return AccessEntryPoint::RELOAD;
  }

  return AccessEntryPoint::UNKNOWN;
}

}  // namespace offline_pages
