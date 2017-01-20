// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/attachments/attachment_downloader_impl.h"

#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/single_thread_task_runner.h"
#include "base/sys_byteorder.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/sync/engine/attachments/attachment_util.h"
#include "components/sync/engine_impl/attachments/attachment_uploader_impl.h"
#include "components/sync/protocol/sync.pb.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"

namespace syncer {

struct AttachmentDownloaderImpl::DownloadState {
 public:
  DownloadState(const AttachmentId& attachment_id,
                const AttachmentUrl& attachment_url);

  AttachmentId attachment_id;
  AttachmentUrl attachment_url;
  // |access_token| needed to invalidate if downloading attachment fails with
  // HTTP_UNAUTHORIZED.
  std::string access_token;
  std::unique_ptr<net::URLFetcher> url_fetcher;
  std::vector<DownloadCallback> user_callbacks;
  base::TimeTicks start_time;
};

AttachmentDownloaderImpl::DownloadState::DownloadState(
    const AttachmentId& attachment_id,
    const AttachmentUrl& attachment_url)
    : attachment_id(attachment_id), attachment_url(attachment_url) {}

AttachmentDownloaderImpl::AttachmentDownloaderImpl(
    const GURL& sync_service_url,
    const scoped_refptr<net::URLRequestContextGetter>&
        url_request_context_getter,
    const std::string& account_id,
    const OAuth2TokenService::ScopeSet& scopes,
    const scoped_refptr<OAuth2TokenServiceRequest::TokenServiceProvider>&
        token_service_provider,
    const std::string& store_birthday,
    ModelType model_type)
    : OAuth2TokenService::Consumer("attachment-downloader-impl"),
      sync_service_url_(sync_service_url),
      url_request_context_getter_(url_request_context_getter),
      account_id_(account_id),
      oauth2_scopes_(scopes),
      token_service_provider_(token_service_provider),
      raw_store_birthday_(store_birthday),
      model_type_(model_type) {
  DCHECK(url_request_context_getter_.get());
  DCHECK(!account_id.empty());
  DCHECK(!scopes.empty());
  DCHECK(token_service_provider_.get());
  DCHECK(!raw_store_birthday_.empty());
}

AttachmentDownloaderImpl::~AttachmentDownloaderImpl() {}

void AttachmentDownloaderImpl::DownloadAttachment(
    const AttachmentId& attachment_id,
    const DownloadCallback& callback) {
  DCHECK(CalledOnValidThread());

  AttachmentUrl url = AttachmentUploaderImpl::GetURLForAttachmentId(
                          sync_service_url_, attachment_id)
                          .spec();

  StateMap::iterator iter = state_map_.find(url);
  DownloadState* download_state =
      iter != state_map_.end() ? iter->second.get() : nullptr;
  if (!download_state) {
    // There is no request started for this attachment id. Let's create
    // DownloadState and request access token for it.
    std::unique_ptr<DownloadState> new_download_state(
        new DownloadState(attachment_id, url));
    download_state = new_download_state.get();
    state_map_[url] = std::move(new_download_state);
    RequestAccessToken(download_state);
  }
  DCHECK(download_state->attachment_id == attachment_id);
  download_state->user_callbacks.push_back(callback);
}

void AttachmentDownloaderImpl::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const std::string& access_token,
    const base::Time& expiration_time) {
  DCHECK(CalledOnValidThread());
  DCHECK(request == access_token_request_.get());
  access_token_request_.reset();
  StateList::const_iterator iter;
  // Start downloads for all download requests waiting for access token.
  for (iter = requests_waiting_for_access_token_.begin();
       iter != requests_waiting_for_access_token_.end(); ++iter) {
    DownloadState* download_state = *iter;
    download_state->access_token = access_token;
    download_state->url_fetcher =
        CreateFetcher(download_state->attachment_url, access_token);
    download_state->start_time = base::TimeTicks::Now();
    download_state->url_fetcher->Start();
  }
  requests_waiting_for_access_token_.clear();
}

void AttachmentDownloaderImpl::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  DCHECK(CalledOnValidThread());
  DCHECK(request == access_token_request_.get());
  access_token_request_.reset();
  StateList::const_iterator iter;
  // Without access token all downloads fail.
  for (iter = requests_waiting_for_access_token_.begin();
       iter != requests_waiting_for_access_token_.end(); ++iter) {
    DownloadState* download_state = *iter;
    scoped_refptr<base::RefCountedString> null_attachment_data;
    ReportResult(*download_state, DOWNLOAD_TRANSIENT_ERROR,
                 null_attachment_data);
    // Don't delete using the URL directly to avoid an access after free error
    // due to std::unordered_map's implementation. See crbug.com/603275.
    auto erase_iter = state_map_.find(download_state->attachment_url);
    DCHECK(erase_iter != state_map_.end());
    state_map_.erase(erase_iter);
  }
  requests_waiting_for_access_token_.clear();
}

void AttachmentDownloaderImpl::OnURLFetchComplete(
    const net::URLFetcher* source) {
  DCHECK(CalledOnValidThread());

  // Find DownloadState by url.
  AttachmentUrl url = source->GetOriginalURL().spec();
  StateMap::iterator iter = state_map_.find(url);
  DCHECK(iter != state_map_.end());
  const DownloadState& download_state = *iter->second;
  DCHECK(source == download_state.url_fetcher.get());

  DownloadResult result = DOWNLOAD_TRANSIENT_ERROR;
  scoped_refptr<base::RefCountedString> attachment_data;
  uint32_t attachment_crc32c = 0;

  net::URLRequestStatus status = source->GetStatus();
  const int response_code = source->GetResponseCode();
  UMA_HISTOGRAM_SPARSE_SLOWLY(
      "Sync.Attachments.DownloadResponseCode",
      status.is_success() ? response_code : status.error());
  if (response_code == net::HTTP_OK) {
    std::string data_as_string;
    source->GetResponseAsString(&data_as_string);
    attachment_data = base::RefCountedString::TakeString(&data_as_string);

    UMA_HISTOGRAM_LONG_TIMES(
        "Sync.Attachments.DownloadTotalTime",
        base::TimeTicks::Now() - download_state.start_time);

    attachment_crc32c = ComputeCrc32c(attachment_data);
    uint32_t crc32c_from_headers = 0;
    if (ExtractCrc32c(source->GetResponseHeaders(), &crc32c_from_headers) &&
        attachment_crc32c != crc32c_from_headers) {
      // Fail download only if there is useful crc32c in header and it doesn't
      // match data. All other cases are fine. When crc32c is not in headers
      // locally calculated one will be stored and used for further checks.
      result = DOWNLOAD_TRANSIENT_ERROR;
    } else {
      // If the id's crc32c doesn't match that of the downloaded attachment,
      // then we're stuck and retrying is unlikely to help.
      if (attachment_crc32c != download_state.attachment_id.GetCrc32c()) {
        result = DOWNLOAD_UNSPECIFIED_ERROR;
      } else {
        result = DOWNLOAD_SUCCESS;
      }
    }
    UMA_HISTOGRAM_BOOLEAN("Sync.Attachments.DownloadChecksumResult",
                          result == DOWNLOAD_SUCCESS);
  } else if (response_code == net::HTTP_UNAUTHORIZED) {
    // Server tells us we've got a bad token so invalidate it.
    OAuth2TokenServiceRequest::InvalidateToken(token_service_provider_.get(),
                                               account_id_, oauth2_scopes_,
                                               download_state.access_token);
    // Fail the request, but indicate that it may be successful if retried.
    result = DOWNLOAD_TRANSIENT_ERROR;
  } else if (response_code == net::HTTP_FORBIDDEN) {
    // User is not allowed to use attachments.  Retrying won't help.
    result = DOWNLOAD_UNSPECIFIED_ERROR;
  } else if (response_code == net::URLFetcher::RESPONSE_CODE_INVALID) {
    result = DOWNLOAD_TRANSIENT_ERROR;
  }
  ReportResult(download_state, result, attachment_data);
  state_map_.erase(iter);
}

std::unique_ptr<net::URLFetcher> AttachmentDownloaderImpl::CreateFetcher(
    const AttachmentUrl& url,
    const std::string& access_token) {
  std::unique_ptr<net::URLFetcher> url_fetcher =
      net::URLFetcher::Create(GURL(url), net::URLFetcher::GET, this);
  AttachmentUploaderImpl::ConfigureURLFetcherCommon(
      url_fetcher.get(), access_token, raw_store_birthday_, model_type_,
      url_request_context_getter_.get());
  data_use_measurement::DataUseUserData::AttachToFetcher(
      url_fetcher.get(), data_use_measurement::DataUseUserData::SYNC);
  return url_fetcher;
}

void AttachmentDownloaderImpl::RequestAccessToken(
    DownloadState* download_state) {
  requests_waiting_for_access_token_.push_back(download_state);
  // Start access token request if there is no active one.
  if (access_token_request_ == nullptr) {
    access_token_request_ = OAuth2TokenServiceRequest::CreateAndStart(
        token_service_provider_.get(), account_id_, oauth2_scopes_, this);
  }
}

void AttachmentDownloaderImpl::ReportResult(
    const DownloadState& download_state,
    const DownloadResult& result,
    const scoped_refptr<base::RefCountedString>& attachment_data) {
  std::vector<DownloadCallback>::const_iterator iter;
  for (iter = download_state.user_callbacks.begin();
       iter != download_state.user_callbacks.end(); ++iter) {
    std::unique_ptr<Attachment> attachment;
    if (result == DOWNLOAD_SUCCESS) {
      attachment = base::MakeUnique<Attachment>(Attachment::CreateFromParts(
          download_state.attachment_id, attachment_data));
    }

    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(*iter, result, base::Passed(&attachment)));
  }
}

bool AttachmentDownloaderImpl::ExtractCrc32c(
    const net::HttpResponseHeaders* headers,
    uint32_t* crc32c) {
  DCHECK(crc32c);
  if (!headers) {
    return false;
  }

  std::string crc32c_encoded;
  std::string header_value;
  size_t iter = 0;
  // Iterate over all matching headers.
  while (headers->EnumerateHeader(&iter, "x-goog-hash", &header_value)) {
    // Because EnumerateHeader is smart about list values, header_value will
    // either be empty or a single name=value pair.
    net::HttpUtil::NameValuePairsIterator pair_iter(header_value.begin(),
                                                    header_value.end(), ',');
    if (pair_iter.GetNext()) {
      if (pair_iter.name() == "crc32c") {
        crc32c_encoded = pair_iter.value();
        DCHECK(!pair_iter.GetNext());
        break;
      }
    }
  }
  // Check if header was found
  if (crc32c_encoded.empty())
    return false;
  std::string crc32c_raw;
  if (!base::Base64Decode(crc32c_encoded, &crc32c_raw))
    return false;

  if (crc32c_raw.size() != sizeof(*crc32c))
    return false;

  *crc32c =
      base::NetToHost32(*reinterpret_cast<const uint32_t*>(crc32c_raw.c_str()));
  return true;
}

}  // namespace syncer
