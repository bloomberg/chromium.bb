// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/offline_page_request_job.h"

#include <string>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/task_runner_util.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/offline_pages/offline_page_tab_helper.h"
#include "chrome/browser/offline_pages/offline_page_utils.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/renderer_host/chrome_navigation_ui_data.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "components/offline_pages/core/request_header/offline_page_header.h"
#include "components/previews/core/previews_decider.h"
#include "components/previews/core/previews_experiments.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/resource_type.h"
#include "net/base/file_stream.h"
#include "net/base/filename_util.h"
#include "net/base/io_buffer.h"
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
  // Digest for the archive file does not match with the one saved in the
  // metadata database.
  DIGEST_MISMATCH,
  // The archive file does not exist.
  FILE_NOT_FOUND,
};

const char kUserDataKey[] = "offline_page_key";
const size_t kMaxBufferSizeForValidation = 1024;

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

class DefaultOfflinePageRequestJobDelegate
    : public OfflinePageRequestJob::Delegate {
 public:
  DefaultOfflinePageRequestJobDelegate() {}

  content::ResourceRequestInfo::WebContentsGetter
  GetWebContentsGetter(net::URLRequest* request) const override {
    const content::ResourceRequestInfo* info =
        content::ResourceRequestInfo::ForRequest(request);
    return info ? info->GetWebContentsGetterForRequest()
                : content::ResourceRequestInfo::WebContentsGetter();
  }

  OfflinePageRequestJob::Delegate::TabIdGetter GetTabIdGetter() const override {
    return base::Bind(&DefaultOfflinePageRequestJobDelegate::GetTabId);
  }

 private:
  static bool GetTabId(content::WebContents* web_contents, int* tab_id) {
    return OfflinePageUtils::GetTabId(web_contents, tab_id);
  }

  DISALLOW_COPY_AND_ASSIGN(DefaultOfflinePageRequestJobDelegate);
};

void GetFileSize(const base::FilePath& file_path, int64_t* file_size) {
  bool succeeded = base::GetFileSize(file_path, file_size);
  if (!succeeded) {
    // Use -1 to indicate that file is not found.
    *file_size = -1;
  }
}

void UpdateDigest(
    const scoped_refptr<OfflinePageRequestJob::ThreadSafeArchiveValidator>&
        validator,
    scoped_refptr<net::IOBuffer> buffer,
    size_t len) {
  validator->Update(buffer->data(), len);
}

OfflinePageRequestJob::NetworkState GetNetworkState(
    net::URLRequest* request,
    const OfflinePageHeader& offline_header,
    previews::PreviewsDecider* previews_decider) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (offline_header.reason == OfflinePageHeader::Reason::NET_ERROR)
    return OfflinePageRequestJob::NetworkState::FLAKY_NETWORK;

  if (net::NetworkChangeNotifier::IsOffline())
    return OfflinePageRequestJob::NetworkState::DISCONNECTED_NETWORK;

  // If RELOAD is present in the offline header, load the live page.
  if (offline_header.reason == OfflinePageHeader::Reason::RELOAD)
    return OfflinePageRequestJob::NetworkState::CONNECTED_NETWORK;

  // If other reason is present in the offline header, force loading the offline
  // page even when the network is connected.
  if (offline_header.reason != OfflinePageHeader::Reason::NONE) {
    return OfflinePageRequestJob::NetworkState::
        FORCE_OFFLINE_ON_CONNECTED_NETWORK;
  }

  // Checks if previews are allowed, the network is slow, and the request is
  // allowed to be shown for previews. When reloading from an offline page or
  // through other force checks, previews should not be considered; previews
  // eligiblity is only checked when |offline_header.reason| is Reason::NONE.
  if (previews_decider && previews_decider->ShouldAllowPreview(
                              *request, previews::PreviewsType::OFFLINE)) {
    return OfflinePageRequestJob::NetworkState::PROHIBITIVELY_SLOW_NETWORK;
  }

  // Otherwise, the network state is a good network.
  return OfflinePageRequestJob::NetworkState::CONNECTED_NETWORK;
}

OfflinePageRequestJob::AggregatedRequestResult
RequestResultToAggregatedRequestResult(
    RequestResult request_result,
    OfflinePageRequestJob::NetworkState network_state) {
  if (request_result == RequestResult::NO_TAB_ID)
    return OfflinePageRequestJob::AggregatedRequestResult::NO_TAB_ID;

  if (request_result == RequestResult::NO_WEB_CONTENTS)
    return OfflinePageRequestJob::AggregatedRequestResult::NO_WEB_CONTENTS;

  if (request_result == RequestResult::FILE_NOT_FOUND)
    return OfflinePageRequestJob::AggregatedRequestResult::FILE_NOT_FOUND;

  if (request_result == RequestResult::PAGE_NOT_FRESH) {
    DCHECK_EQ(OfflinePageRequestJob::NetworkState::PROHIBITIVELY_SLOW_NETWORK,
              network_state);
    return OfflinePageRequestJob::AggregatedRequestResult::
        PAGE_NOT_FRESH_ON_PROHIBITIVELY_SLOW_NETWORK;
  }

  if (request_result == RequestResult::OFFLINE_PAGE_NOT_FOUND) {
    switch (network_state) {
      case OfflinePageRequestJob::NetworkState::DISCONNECTED_NETWORK:
        return OfflinePageRequestJob::AggregatedRequestResult::
            PAGE_NOT_FOUND_ON_DISCONNECTED_NETWORK;
      case OfflinePageRequestJob::NetworkState::PROHIBITIVELY_SLOW_NETWORK:
        return OfflinePageRequestJob::AggregatedRequestResult::
            PAGE_NOT_FOUND_ON_PROHIBITIVELY_SLOW_NETWORK;
      case OfflinePageRequestJob::NetworkState::FLAKY_NETWORK:
        return OfflinePageRequestJob::AggregatedRequestResult::
            PAGE_NOT_FOUND_ON_FLAKY_NETWORK;
      case OfflinePageRequestJob::NetworkState::
          FORCE_OFFLINE_ON_CONNECTED_NETWORK:
        return OfflinePageRequestJob::AggregatedRequestResult::
            PAGE_NOT_FOUND_ON_CONNECTED_NETWORK;
      case OfflinePageRequestJob::NetworkState::CONNECTED_NETWORK:
        break;
    }
    NOTREACHED();
  }

  if (request_result == RequestResult::REDIRECTED) {
    switch (network_state) {
      case OfflinePageRequestJob::NetworkState::DISCONNECTED_NETWORK:
        return OfflinePageRequestJob::AggregatedRequestResult::
            REDIRECTED_ON_DISCONNECTED_NETWORK;
      case OfflinePageRequestJob::NetworkState::PROHIBITIVELY_SLOW_NETWORK:
        return OfflinePageRequestJob::AggregatedRequestResult::
            REDIRECTED_ON_PROHIBITIVELY_SLOW_NETWORK;
      case OfflinePageRequestJob::NetworkState::FLAKY_NETWORK:
        return OfflinePageRequestJob::AggregatedRequestResult::
            REDIRECTED_ON_FLAKY_NETWORK;
      case OfflinePageRequestJob::NetworkState::
          FORCE_OFFLINE_ON_CONNECTED_NETWORK:
        return OfflinePageRequestJob::AggregatedRequestResult::
            REDIRECTED_ON_CONNECTED_NETWORK;
      case OfflinePageRequestJob::NetworkState::CONNECTED_NETWORK:
        break;
    }
    NOTREACHED();
  }

  if (request_result == RequestResult::DIGEST_MISMATCH) {
    switch (network_state) {
      case OfflinePageRequestJob::NetworkState::DISCONNECTED_NETWORK:
        return OfflinePageRequestJob::AggregatedRequestResult::
            DIGEST_MISMATCH_ON_DISCONNECTED_NETWORK;
      case OfflinePageRequestJob::NetworkState::PROHIBITIVELY_SLOW_NETWORK:
        return OfflinePageRequestJob::AggregatedRequestResult::
            DIGEST_MISMATCH_ON_PROHIBITIVELY_SLOW_NETWORK;
      case OfflinePageRequestJob::NetworkState::FLAKY_NETWORK:
        return OfflinePageRequestJob::AggregatedRequestResult::
            DIGEST_MISMATCH_ON_FLAKY_NETWORK;
      case OfflinePageRequestJob::NetworkState::
          FORCE_OFFLINE_ON_CONNECTED_NETWORK:
      case OfflinePageRequestJob::NetworkState::CONNECTED_NETWORK:
        return OfflinePageRequestJob::AggregatedRequestResult::
            DIGEST_MISMATCH_ON_CONNECTED_NETWORK;
    }
    NOTREACHED();
  }

  DCHECK_EQ(RequestResult::OFFLINE_PAGE_SERVED, request_result);
  DCHECK_NE(OfflinePageRequestJob::NetworkState::CONNECTED_NETWORK,
            network_state);
  switch (network_state) {
    case OfflinePageRequestJob::NetworkState::DISCONNECTED_NETWORK:
      return OfflinePageRequestJob::AggregatedRequestResult::
          SHOW_OFFLINE_ON_DISCONNECTED_NETWORK;
    case OfflinePageRequestJob::NetworkState::PROHIBITIVELY_SLOW_NETWORK:
      return OfflinePageRequestJob::AggregatedRequestResult::
          SHOW_OFFLINE_ON_PROHIBITIVELY_SLOW_NETWORK;
    case OfflinePageRequestJob::NetworkState::FLAKY_NETWORK:
      return OfflinePageRequestJob::AggregatedRequestResult::
          SHOW_OFFLINE_ON_FLAKY_NETWORK;
    case OfflinePageRequestJob::NetworkState::
        FORCE_OFFLINE_ON_CONNECTED_NETWORK:
      return OfflinePageRequestJob::AggregatedRequestResult::
          SHOW_OFFLINE_ON_CONNECTED_NETWORK;
    case OfflinePageRequestJob::NetworkState::CONNECTED_NETWORK:
      break;
  }
  NOTREACHED();

  return OfflinePageRequestJob::AggregatedRequestResult::
      AGGREGATED_REQUEST_RESULT_MAX;
}

void ReportOpenResult(int result) {
  base::UmaHistogramSparse("OfflinePages.RequestJob.OpenFileErrorCode",
                           -result);
}

void ReportSeekResult(int result) {
  if (result < 0) {
    base::UmaHistogramSparse("OfflinePages.RequestJob.SeekFileErrorCode",
                             static_cast<int>(-result));
  }
}

void ReportReadResult(int result) {
  if (result < 0) {
    base::UmaHistogramSparse("OfflinePages.RequestJob.ReadFileErrorCode",
                             -result);
  }
}

void ReportRequestResult(RequestResult request_result,
                         OfflinePageRequestJob::NetworkState network_state) {
  OfflinePageRequestJob::ReportAggregatedRequestResult(
      RequestResultToAggregatedRequestResult(request_result, network_state));
}

void ReportOfflinePageSize(OfflinePageRequestJob::NetworkState network_state,
                           const OfflinePageItem& offline_page) {
  if (offline_page.client_id.name_space.empty())
    return;

  // The two histograms report values between 1KiB and 100MiB.
  switch (network_state) {
    case OfflinePageRequestJob::NetworkState::
        DISCONNECTED_NETWORK:  // Fall-through
    case OfflinePageRequestJob::NetworkState::
        PROHIBITIVELY_SLOW_NETWORK:  // Fall-through
    case OfflinePageRequestJob::NetworkState::FLAKY_NETWORK:
      base::UmaHistogramCounts100000("OfflinePages.PageSizeOnAccess.Offline." +
                                         offline_page.client_id.name_space,
                                     offline_page.file_size / 1024);
      return;
    case OfflinePageRequestJob::NetworkState::
        FORCE_OFFLINE_ON_CONNECTED_NETWORK:
      base::UmaHistogramCounts100000("OfflinePages.PageSizeOnAccess.Online." +
                                         offline_page.client_id.name_space,
                                     offline_page.file_size / 1024);
      return;
    case OfflinePageRequestJob::NetworkState::CONNECTED_NETWORK:
      break;
  }
  NOTREACHED();
}

void ReportAccessEntryPoint(
    const std::string& name_space,
    OfflinePageRequestJob::AccessEntryPoint entry_point) {
  base::UmaHistogramEnumeration("OfflinePages.AccessEntryPoint." + name_space,
                                entry_point,
                                OfflinePageRequestJob::AccessEntryPoint::COUNT);
}

void ReportExistenceOfRangeHeader(bool has_range_header) {
  base::UmaHistogramBoolean("OfflinePages.RequestJob.RangeHeader",
                            has_range_header);
}

void ReportIntentDataChangedAfterValidation(bool changed) {
  UMA_HISTOGRAM_BOOLEAN(
      "OfflinePages.RequestJob.IntentDataChangedAfterValidation", changed);
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

void NotifyAvailableOfflinePagesOnIO(
    base::WeakPtr<OfflinePageRequestJob> job,
    const std::vector<OfflinePageRequestJob::Candidate>& candidates) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (job)
    job->OnOfflinePagesAvailable(candidates);
}

// Notifies OfflinePageRequestJob about all the matched offline pages.
void NotifyAvailableOfflinePagesOnUI(
    base::WeakPtr<OfflinePageRequestJob> job,
    const std::vector<OfflinePageRequestJob::Candidate>& candidates) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Delegates to IO thread since OfflinePageRequestJob should only be accessed
  // from IO thread.
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::BindOnce(&NotifyAvailableOfflinePagesOnIO, job, candidates));
}

// Failed to find an offline page.
void FailedToFindOfflinePage(RequestResult request_error_result,
                             OfflinePageRequestJob::NetworkState network_state,
                             base::WeakPtr<OfflinePageRequestJob> job) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_NE(RequestResult::OFFLINE_PAGE_SERVED, request_error_result);

  ReportRequestResult(request_error_result, network_state);
  NotifyAvailableOfflinePagesOnUI(
      job, std::vector<OfflinePageRequestJob::Candidate>());
}

// Handles the result of finding matched offline pages.
void SelectPagesForURLDone(
    const GURL& url,
    const OfflinePageHeader& offline_header,
    OfflinePageRequestJob::NetworkState network_state,
    base::WeakPtr<OfflinePageRequestJob> job,
    content::ResourceRequestInfo::WebContentsGetter web_contents_getter,
    const std::vector<OfflinePageItem>& offline_pages) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Bail out if no page is found.
  if (offline_pages.empty()) {
    FailedToFindOfflinePage(RequestResult::OFFLINE_PAGE_NOT_FOUND,
                            network_state, job);
    return;
  }

  // Bail out if web_contents is gone.
  OfflinePageModel* offline_page_model =
      GetOfflinePageModel(web_contents_getter);
  if (!offline_page_model) {
    FailedToFindOfflinePage(RequestResult::NO_WEB_CONTENTS, network_state, job);
    return;
  }

  std::vector<OfflinePageRequestJob::Candidate> candidates;
  for (const auto& offline_page : offline_pages) {
    OfflinePageRequestJob::Candidate candidate;
    candidate.offline_page = offline_page;
    candidate.archive_is_in_internal_dir =
        offline_page_model->IsArchiveInInternalDir(offline_page.file_path);
    candidates.push_back(candidate);
  }

  NotifyAvailableOfflinePagesOnUI(job, candidates);
}

// Handles the result of finding an offline page with the requested offline ID.
void GetPageByOfflineIdDone(
    const GURL& url,
    const OfflinePageHeader& offline_header,
    OfflinePageRequestJob::NetworkState network_state,
    content::ResourceRequestInfo::WebContentsGetter web_contents_getter,
    base::WeakPtr<OfflinePageRequestJob> job,
    const OfflinePageItem* offline_page) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // If the found offline page does not match the request URL, fail.
  if (!offline_page || offline_page->url != url) {
    FailedToFindOfflinePage(RequestResult::OFFLINE_PAGE_NOT_FOUND,
                            network_state, job);
    return;
  }

  std::vector<OfflinePageItem> offline_pages;
  offline_pages.push_back(*offline_page);
  SelectPagesForURLDone(url, offline_header, network_state, job,
                        web_contents_getter, offline_pages);
}

// Tries to find all the offline pages to serve for |url|.
void GetPagesToServeURL(
    const GURL& url,
    const OfflinePageHeader& offline_header,
    OfflinePageRequestJob::NetworkState network_state,
    content::ResourceRequestInfo::WebContentsGetter web_contents_getter,
    OfflinePageRequestJob::Delegate::TabIdGetter tab_id_getter,
    base::WeakPtr<OfflinePageRequestJob> job) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::WebContents* web_contents = web_contents_getter.Run();
  if (!web_contents) {
    FailedToFindOfflinePage(RequestResult::NO_WEB_CONTENTS, network_state, job);
    return;
  }
  int tab_id;
  if (!tab_id_getter.Run(web_contents, &tab_id)) {
    FailedToFindOfflinePage(RequestResult::NO_TAB_ID, network_state, job);
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

// Do all the things needed to be done on UI thread after a trusted offline
// page has been visited.
void VisitTrustedOfflinePageOnUI(
    const OfflinePageHeader& offline_header,
    OfflinePageRequestJob::NetworkState network_state,
    content::ResourceRequestInfo::WebContentsGetter web_contents_getter,
    const OfflinePageItem& offline_page,
    bool archive_is_in_internal_dir) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // |web_contents_getter| is passed from IO thread. We need to check if
  // web contents is still valid.
  content::WebContents* web_contents = web_contents_getter.Run();
  if (!web_contents)
    return;

  OfflinePageModel* offline_page_model =
      OfflinePageModelFactory::GetForBrowserContext(
          web_contents->GetBrowserContext());
  if (!offline_page_model)
    return;

  // Mark the offline page as being accessed.
  offline_page_model->MarkPageAccessed(offline_page.offline_id);

  // Save an cached copy of OfflinePageItem such that Tab code can get
  // the loaded offline page immediately.
  OfflinePageTabHelper* tab_helper =
      OfflinePageTabHelper::FromWebContents(web_contents);
  DCHECK(tab_helper);
  tab_helper->SetOfflinePage(
      offline_page, offline_header,
      archive_is_in_internal_dir
          ? OfflinePageTrustedState::TRUSTED_AS_IN_INTERNAL_DIR
          : OfflinePageTrustedState::TRUSTED_AS_UNMODIFIED_AND_IN_PUBLIC_DIR,
      network_state ==
          OfflinePageRequestJob::NetworkState::PROHIBITIVELY_SLOW_NETWORK);
}

void ClearOfflinePageData(
    content::ResourceRequestInfo::WebContentsGetter web_contents_getter) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // |web_contents_getter| is passed from IO thread. We need to check if
  // web contents is still valid.
  content::WebContents* web_contents = web_contents_getter.Run();
  if (!web_contents)
    return;

  // Save an cached copy of OfflinePageItem such that Tab code can get
  // the loaded offline page immediately.
  OfflinePageTabHelper* tab_helper =
      OfflinePageTabHelper::FromWebContents(web_contents);
  DCHECK(tab_helper);
  tab_helper->ClearOfflinePage();
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
                         std::make_unique<OfflinePageRequestInfo>());
  }

  return new OfflinePageRequestJob(request, network_delegate, previews_decider);
}

OfflinePageRequestJob::OfflinePageRequestJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    previews::PreviewsDecider* previews_decider)
    : net::URLRequestJob(request, network_delegate),
      delegate_(new DefaultOfflinePageRequestJobDelegate()),
      previews_decider_(previews_decider),
      candidate_index_(0),
      has_range_header_(false),
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
  offline_header_ = OfflinePageHeader(offline_header_value);

  network_state_ =
      GetNetworkState(request(), offline_header_, previews_decider_);
  if (network_state_ == NetworkState::CONNECTED_NETWORK) {
    FallbackToDefault();
    return;
  }

  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::BindOnce(&GetPagesToServeURL, request()->url(), offline_header_,
                     network_state_, delegate_->GetWebContentsGetter(request()),
                     delegate_->GetTabIdGetter(),
                     weak_ptr_factory_.GetWeakPtr()));
}

void OfflinePageRequestJob::Kill() {
  stream_.reset();
  net::URLRequestJob::Kill();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

int OfflinePageRequestJob::ReadRawData(net::IOBuffer* dest, int dest_size) {
  DCHECK_NE(dest_size, 0);

  return stream_->Read(
      dest, dest_size,
      base::Bind(&OfflinePageRequestJob::DidReadForServing,
                 weak_ptr_factory_.GetWeakPtr(), base::WrapRefCounted(dest)));
}

void OfflinePageRequestJob::GetResponseInfo(net::HttpResponseInfo* info) {
  if (!fake_headers_for_redirect_) {
    net::URLRequestJob::GetResponseInfo(info);
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

bool OfflinePageRequestJob::GetMimeType(std::string* mime_type) const {
  if (candidates_.size() > 0 && candidate_index_ < candidates_.size()) {
    *mime_type = "multipart/related";
    return true;
  }
  return false;
}

void OfflinePageRequestJob::SetExtraRequestHeaders(
    const net::HttpRequestHeaders& headers) {
  if (headers.HasHeader(net::HttpRequestHeaders::kRange))
    has_range_header_ = true;
}

void OfflinePageRequestJob::FallbackToDefault() {
  OfflinePageRequestInfo* info =
      OfflinePageRequestInfo::GetFromRequest(request());
  DCHECK(info);
  info->set_use_default(true);

  net::URLRequestJob::NotifyRestartRequired();
}

void OfflinePageRequestJob::OnOfflinePagesAvailable(
    const std::vector<Candidate>& candidates) {
  // If no offline page is found, restart the job to fall back to the default
  // handling.
  if (candidates.empty()) {
    FallbackToDefault();
    return;
  }

  file_task_runner_ = base::CreateTaskRunnerWithTraits(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

  // Start the file validation from the 1st offline page.
  candidates_ = candidates;
  candidate_index_ = 0;
  ValidateFile();
}

void OfflinePageRequestJob::OnTrustedOfflinePageFound() {
  // If the match is for original URL, trigger the redirect.
  // Note: If the offline page has same orginal URL and final URL, don't trigger
  // the redirect. Some websites might route the redirect finally back to itself
  // after intermediate redirects for authentication. Previously this case was
  // not handled and some pages might be saved with same URLs. Though we fixed
  // the problem, we still need to support those pages already saved with this
  if (request()->url() == GetCurrentOfflinePage().original_url &&
      request()->url() != GetCurrentOfflinePage().url) {
    ReportRequestResult(RequestResult::REDIRECTED, network_state_);
    Redirect(GetCurrentOfflinePage().url);
    return;
  }

  // If the page is being loaded on a slow network, only use the offline page
  // if it was created within the past day.
  if (network_state_ == NetworkState::PROHIBITIVELY_SLOW_NETWORK &&
      base::Time::Now() - GetCurrentOfflinePage().creation_time >
          previews::params::OfflinePreviewFreshnessDuration()) {
    ReportRequestResult(RequestResult::PAGE_NOT_FRESH, network_state_);
    FallbackToDefault();
    return;
  }

  // No need to open the file if it has already been opened for the validation.
  if (stream_) {
    DidOpenForServing(net::OK);
    return;
  }

  // If a file:// or content:// intent is being processed, open the file:// or
  // content:// denoted in the intent instead. Otherwise, open the archive file
  // associated with the offline page.
  base::FilePath file_path;
  if (IsProcessingFileUrlIntent()) {
    bool valid = net::FileURLToFilePath(offline_header_.intent_url, &file_path);
    DCHECK(valid);
#if defined(OS_ANDROID)
  } else if (IsProcessingContentUrlIntent()) {
    file_path = base::FilePath(offline_header_.intent_url.spec());
    DCHECK(file_path.IsContentUri());
#endif  // defined(OS_ANDROID)
  } else {
    file_path = GetCurrentOfflinePage().file_path;
  }
  OpenFile(file_path, base::Bind(&OfflinePageRequestJob::DidOpenForServing,
                                 weak_ptr_factory_.GetWeakPtr()));
}

void OfflinePageRequestJob::VisitTrustedOfflinePage() {
  ReportRequestResult(RequestResult::OFFLINE_PAGE_SERVED, network_state_);
  ReportAccessEntryPoint(GetCurrentOfflinePage().client_id.name_space,
                         GetAccessEntryPoint());
  ReportOfflinePageSize(network_state_, GetCurrentOfflinePage());
  ReportExistenceOfRangeHeader(has_range_header_);

  SetOfflinePageNavigationUIData(true /*is_offline_page*/);

  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::BindOnce(&VisitTrustedOfflinePageOnUI, offline_header_,
                     network_state_, delegate_->GetWebContentsGetter(request()),
                     GetCurrentOfflinePage(),
                     candidates_[candidate_index_].archive_is_in_internal_dir));
}

void OfflinePageRequestJob::SetOfflinePageNavigationUIData(
    bool is_offline_page) {
  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request());
  ChromeNavigationUIData* navigation_data =
      static_cast<ChromeNavigationUIData*>(info->GetNavigationUIData());
  if (navigation_data) {
    std::unique_ptr<OfflinePageNavigationUIData> offline_page_data =
        std::make_unique<OfflinePageNavigationUIData>(is_offline_page);
    navigation_data->SetOfflinePageNavigationUIData(
        std::move(offline_page_data));
  }
}

void OfflinePageRequestJob::Redirect(const GURL& redirected_url) {
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

  net::URLRequestJob::NotifyHeadersComplete();
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

  switch (offline_header_.reason) {
    case OfflinePageHeader::Reason::DOWNLOAD:
      return AccessEntryPoint::DOWNLOADS;
    case OfflinePageHeader::Reason::NOTIFICATION:
      return AccessEntryPoint::NOTIFICATION;
    case OfflinePageHeader::Reason::FILE_URL_INTENT:
      return AccessEntryPoint::FILE_URL_INTENT;
    case OfflinePageHeader::Reason::CONTENT_URL_INTENT:
      return AccessEntryPoint::CONTENT_URL_INTENT;
    default:
      break;
  }

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

const OfflinePageItem& OfflinePageRequestJob::GetCurrentOfflinePage() const {
  return candidates_[candidate_index_].offline_page;
}

bool OfflinePageRequestJob::IsProcessingFileUrlIntent() const {
  return offline_header_.reason == OfflinePageHeader::Reason::FILE_URL_INTENT;
}

bool OfflinePageRequestJob::IsProcessingContentUrlIntent() const {
  return offline_header_.reason ==
         OfflinePageHeader::Reason::CONTENT_URL_INTENT;
}

bool OfflinePageRequestJob::IsProcessingFileOrContentUrlIntent() const {
  return IsProcessingFileUrlIntent() || IsProcessingContentUrlIntent();
}

void OfflinePageRequestJob::OpenFile(const base::FilePath& file_path,
                                     const net::CompletionCallback& callback) {
  if (!stream_)
    stream_ = std::make_unique<net::FileStream>(file_task_runner_);

  int flags =
      base::File::FLAG_OPEN | base::File::FLAG_READ | base::File::FLAG_ASYNC;
#if defined(OS_ANDROID)
  if (!file_path.IsContentUri())
    flags |= base::File::FLAG_EXCLUSIVE_READ;
#else
  flags |= base::File::FLAG_EXCLUSIVE_READ;
#endif  // defined(OS_ANDROID)
  int result = stream_->Open(file_path, flags, callback);
  if (result != net::ERR_IO_PENDING)
    callback.Run(result);
}

void OfflinePageRequestJob::UpdateDigestOnBackground(
    scoped_refptr<net::IOBuffer> buffer,
    size_t len,
    base::OnceCallback<void(void)> digest_updated_callback) {
  DCHECK_GT(len, 0u);

  if (!archive_validator_)
    archive_validator_ = new ThreadSafeArchiveValidator();

  // Delegate to background task runner to update the hash since it is time
  // consuming. Once it is done, |digest_updated_callback| will be called to
  // continue the reading.
  file_task_runner_->PostTaskAndReply(
      FROM_HERE, base::BindOnce(&UpdateDigest, archive_validator_, buffer, len),
      std::move(digest_updated_callback));
}

void OfflinePageRequestJob::FinalizeDigestOnBackground(
    base::OnceCallback<void(const std::string&)> digest_finalized_callback) {
  DCHECK(archive_validator_.get());

  // Delegate to background task runner to finalize the hash to get the digest
  // since it is time consuming. Once it is done, |digest_finalized_callback|
  // will be called with the digest.
  base::PostTaskAndReplyWithResult(
      file_task_runner_.get(), FROM_HERE,
      base::BindOnce(&ThreadSafeArchiveValidator::Finish, archive_validator_),
      std::move(digest_finalized_callback));
}

void OfflinePageRequestJob::ValidateFile() {
  // If the archive file is in internal directory, the offline page can be
  // deemed as trusted without going through valication.
  if (candidates_[candidate_index_].archive_is_in_internal_dir) {
    OnTrustedOfflinePageFound();
    return;
  }

  // Otherwise, the archive file is in public directory. If the digest is empty,
  // the validation can fail immediately.
  if (GetCurrentOfflinePage().digest.empty()) {
    OnFileValidationDone(FileValidationResult::FILE_VALIDATION_FAILED);
    return;
  }

  // If a file:// or content:// URL intent is being viewed, skip the validation.
  // The digest for the file:// or content:// denoted in the intent was computed
  // and used to find the offline page. However, we will not validate and read
  // from the archive archive file assoicated with the offline page since it may
  // not exist or even got modified. We will read from the file:// or content://
  // denoted in the intent  and compute the digest of the read data to make sure
  // it does not get changed.
  if (IsProcessingFileOrContentUrlIntent()) {
    OnFileValidationDone(FileValidationResult::FILE_VALIDATION_SUCCEEDED);
    return;
  }

  GetFileSizeForValidation();
}

void OfflinePageRequestJob::GetFileSizeForValidation() {
  int64_t* file_size = new int64_t(0);
  file_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&GetFileSize, GetCurrentOfflinePage().file_path,
                     base::Unretained(file_size)),
      base::BindOnce(&OfflinePageRequestJob::DidGetFileSizeForValidation,
                     weak_ptr_factory_.GetWeakPtr(), base::Owned(file_size)));
}

void OfflinePageRequestJob::DidGetFileSizeForValidation(
    const int64_t* actual_file_size) {
  if (*actual_file_size == -1) {
    OnFileValidationDone(FileValidationResult::FILE_NOT_FOUND);
    return;
  }

  if (*actual_file_size != GetCurrentOfflinePage().file_size) {
    OnFileValidationDone(FileValidationResult::FILE_VALIDATION_FAILED);
    return;
  }

  // Open file to compute the digest.
  OpenFile(GetCurrentOfflinePage().file_path,
           base::Bind(&OfflinePageRequestJob::DidOpenForValidation,
                      weak_ptr_factory_.GetWeakPtr()));
}

void OfflinePageRequestJob::DidOpenForValidation(int result) {
  ReportOpenResult(result);

  if (result != net::OK) {
    OnFileValidationDone(FileValidationResult::FILE_VALIDATION_FAILED);
    return;
  }

  if (!buffer_)
    buffer_ = new net::IOBuffer(kMaxBufferSizeForValidation);

  ReadForValidation();
}

void OfflinePageRequestJob::ReadForValidation() {
  int result =
      stream_->Read(buffer_.get(), kMaxBufferSizeForValidation,
                    base::Bind(&OfflinePageRequestJob::DidReadForValidation,
                               weak_ptr_factory_.GetWeakPtr()));
  if (result != net::ERR_IO_PENDING)
    DidReadForValidation(result);
}

void OfflinePageRequestJob::DidReadForValidation(int result) {
  ReportReadResult(result);

  if (result < 0) {
    OnFileValidationDone(FileValidationResult::FILE_VALIDATION_FAILED);
    return;
  }

  if (result > 0) {
    UpdateDigestOnBackground(
        buffer_, result,
        base::BindOnce(&OfflinePageRequestJob::ReadForValidation,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  // When |result| is 0 (net::OK), it indicates EOF. We need to finalize the
  // hash to get the actual digest.
  FinalizeDigestOnBackground(base::BindOnce(
      &OfflinePageRequestJob::DidComputeActualDigestForValidation,
      weak_ptr_factory_.GetWeakPtr()));
}

void OfflinePageRequestJob::DidComputeActualDigestForValidation(
    const std::string& actual_digest) {
  DCHECK(!GetCurrentOfflinePage().digest.empty());
  bool is_trusted = actual_digest == GetCurrentOfflinePage().digest;
  OnFileValidationDone(is_trusted
                           ? FileValidationResult::FILE_VALIDATION_SUCCEEDED
                           : FileValidationResult::FILE_VALIDATION_FAILED);
}

void OfflinePageRequestJob::OnFileValidationDone(FileValidationResult result) {
  if (result == FileValidationResult::FILE_VALIDATION_SUCCEEDED) {
    OnTrustedOfflinePageFound();
    return;
  }

  // The stream is no longer needed.
  stream_.reset();

  // Move to next archive file if there is more.
  candidate_index_++;
  if (candidate_index_ < candidates_.size()) {
    ValidateFile();
    return;
  }

  // Otherwise, no trusted offline page can be found so we fall back to the
  // default handling.
  ReportRequestResult(result == FileValidationResult::FILE_NOT_FOUND
                          ? RequestResult::FILE_NOT_FOUND
                          : RequestResult::DIGEST_MISMATCH,
                      network_state_);
  FallbackToDefault();
}

void OfflinePageRequestJob::DidOpenForServing(int result) {
  ReportOpenResult(result);

  // Handle the file opening failure.
  if (result != net::OK) {
    ReportRequestResult(RequestResult::FILE_NOT_FOUND, network_state_);

    // If the file:// or content:// intent is being processed, don't fall
    // back to the default handling. Instead, we should fail the request.
    if (IsProcessingFileOrContentUrlIntent()) {
      NotifyStartError(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                             net::ERR_FAILED));
    } else {
      FallbackToDefault();
    }
    return;
  }

  // Now we're going to read the archive file to serve the offline page. Do
  // all the necessary reporting and bookkeeping for using this offline page.
  VisitTrustedOfflinePage();

  // Note that we always seek to the beginning of the file because the file may
  // have already been read for validation purpose.
  int seek_result =
      stream_->Seek(0, base::Bind(&OfflinePageRequestJob::DidSeekForServing,
                                  weak_ptr_factory_.GetWeakPtr()));
  if (seek_result != net::ERR_IO_PENDING)
    DidSeekForServing(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE);
}

void OfflinePageRequestJob::DidSeekForServing(int64_t result) {
  DCHECK(result <= 0);

  ReportSeekResult(result);

  if (result < 0) {
    NotifyStartError(net::URLRequestStatus(
        net::URLRequestStatus::FAILED, net::ERR_REQUEST_RANGE_NOT_SATISFIABLE));
    return;
  }

  set_expected_content_size(GetCurrentOfflinePage().file_size);
  NotifyHeadersComplete();
}

void OfflinePageRequestJob::DidReadForServing(scoped_refptr<net::IOBuffer> buf,
                                              int result) {
  ReportReadResult(result);

  if (result < 0 || !IsProcessingFileOrContentUrlIntent()) {
    buf = nullptr;
    NotifyReadRawDataComplete(result);
    return;
  }

  // At this point, we have result >= 0 && IsProcessingFileOrContentUrlIntent()
  // which means the read succeeds for processing the file:// or content:// URL
  // intent. We need to compute the digest to ensure that the file:// or
  // content:// we read is not modified since the time we received the intent,
  // validated the data provided by file:// or content:// URL, and decided to
  // turn it into the corresponding http/https URL and let OfflinePageRequestJob
  // handle it.
  if (result > 0) {
    UpdateDigestOnBackground(
        buf, result,
        base::BindOnce(&OfflinePageRequestJob::NotifyReadRawDataComplete,
                       weak_ptr_factory_.GetWeakPtr(), result));

  } else {
    // When |result| is 0 (net::OK), it indicates EOF. We need to finalize the
    // hash to get the actual digest.
    FinalizeDigestOnBackground(
        base::BindOnce(&OfflinePageRequestJob::DidComputeActualDigestForServing,
                       weak_ptr_factory_.GetWeakPtr(), result));
  }
}

void OfflinePageRequestJob::NotifyReadRawDataComplete(int result) {
  ReadRawDataComplete(result);
}

void OfflinePageRequestJob::DidComputeActualDigestForServing(
    int result,
    const std::string& actual_digest) {
  // If the actual digest does not match, fail the request job.
  bool mismatch = actual_digest != GetCurrentOfflinePage().digest;
  ReportIntentDataChangedAfterValidation(mismatch);
  if (mismatch) {
    SetOfflinePageNavigationUIData(false /*is_offline_page*/);
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(&ClearOfflinePageData,
                   delegate_->GetWebContentsGetter(request())));
    result = net::ERR_FAILED;
  }

  ReadRawDataComplete(result);
}

}  // namespace offline_pages
