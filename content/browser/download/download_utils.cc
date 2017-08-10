// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_utils.h"

#include "base/format_macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "content/browser/download/download_interrupt_reasons_impl.h"
#include "content/browser/download/download_stats.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_url_parameters.h"
#include "content/public/common/resource_request.h"
#include "net/base/elements_upload_data_stream.h"
#include "net/base/load_flags.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/url_request_context.h"

namespace content {

namespace {

void AppendExtraHeaders(net::HttpRequestHeaders* headers,
                        DownloadUrlParameters* params) {
  for (const auto& header : params->request_headers())
    headers->SetHeaderIfMissing(header.first, header.second);
}

int GetLoadFlags(DownloadUrlParameters* params, bool has_upload_data) {
  int load_flags = 0;
  if (params->prefer_cache()) {
    // If there is upload data attached, only retrieve from cache because there
    // is no current mechanism to prompt the user for their consent for a
    // re-post. For GETs, try to retrieve data from the cache and skip
    // validating the entry if present.
    if (has_upload_data)
      load_flags |= net::LOAD_ONLY_FROM_CACHE | net::LOAD_SKIP_CACHE_VALIDATION;
    else
      load_flags |= net::LOAD_SKIP_CACHE_VALIDATION;
  } else {
    load_flags |= net::LOAD_DISABLE_CACHE;
  }
  return load_flags;
}

std::unique_ptr<net::HttpRequestHeaders> GetAdditionalRequestHeaders(
    DownloadUrlParameters* params) {
  auto headers = base::MakeUnique<net::HttpRequestHeaders>();
  if (params->offset() == 0 &&
      params->length() == DownloadSaveInfo::kLengthFullContent) {
    AppendExtraHeaders(headers.get(), params);
    return headers;
  }

  bool has_last_modified = !params->last_modified().empty();
  bool has_etag = !params->etag().empty();

  // Strong validator(i.e. etag or last modified) is required in range requests
  // for download resumption and parallel download.
  DCHECK(has_etag || has_last_modified);
  if (!has_etag && !has_last_modified) {
    DVLOG(1) << "Creating partial request without strong validators.";
    AppendExtraHeaders(headers.get(), params);
    return headers;
  }

  // Add "Range" header.
  std::string range_header =
      (params->length() == DownloadSaveInfo::kLengthFullContent)
          ? base::StringPrintf("bytes=%" PRId64 "-", params->offset())
          : base::StringPrintf("bytes=%" PRId64 "-%" PRId64, params->offset(),
                               params->offset() + params->length() - 1);
  headers->SetHeader(net::HttpRequestHeaders::kRange, range_header);

  // Add "If-Range" headers.
  if (params->use_if_range()) {
    // In accordance with RFC 7233 Section 3.2, use If-Range to specify that
    // the server return the entire entity if the validator doesn't match.
    // Last-Modified can be used in the absence of ETag as a validator if the
    // response headers satisfied the HttpUtil::HasStrongValidators()
    // predicate.
    //
    // This function assumes that HasStrongValidators() was true and that the
    // ETag and Last-Modified header values supplied are valid.
    headers->SetHeader(net::HttpRequestHeaders::kIfRange,
                       has_etag ? params->etag() : params->last_modified());
    AppendExtraHeaders(headers.get(), params);
    return headers;
  }

  // Add "If-Match"/"If-Unmodified-Since" headers.
  if (has_etag)
    headers->SetHeader(net::HttpRequestHeaders::kIfMatch, params->etag());

  // According to RFC 7232 section 3.4, "If-Unmodified-Since" is mainly for
  // old servers that didn't implement "If-Match" and must be ignored when
  // "If-Match" presents.
  if (has_last_modified) {
    headers->SetHeader(net::HttpRequestHeaders::kIfUnmodifiedSince,
                       params->last_modified());
  }

  AppendExtraHeaders(headers.get(), params);
  return headers;
}

}  // namespace

DownloadInterruptReason HandleRequestCompletionStatus(
    net::Error error_code, bool has_strong_validators,
    net::CertStatus cert_status, DownloadInterruptReason abort_reason) {
  // ERR_CONTENT_LENGTH_MISMATCH can be caused by 1 of the following reasons:
  // 1. Server or proxy closes the connection too early.
  // 2. The content-length header is wrong.
  // If the download has strong validators, we can interrupt the download
  // and let it resume automatically. Otherwise, resuming the download will
  // cause it to restart and the download may never complete if the error was
  // caused by reason 2. As a result, downloads without strong validators are
  // treated as completed here.
  // TODO(qinmin): check the metrics from downloads with strong validators,
  // and decide whether we should interrupt downloads without strong validators
  // rather than complete them.
  if (error_code == net::ERR_CONTENT_LENGTH_MISMATCH &&
      !has_strong_validators) {
    error_code = net::OK;
    RecordDownloadCount(COMPLETED_WITH_CONTENT_LENGTH_MISMATCH_COUNT);
  }

  if (error_code == net::ERR_ABORTED) {
    // ERR_ABORTED == something outside of the network
    // stack cancelled the request.  There aren't that many things that
    // could do this to a download request (whose lifetime is separated from
    // the tab from which it came).  We map this to USER_CANCELLED as the
    // case we know about (system suspend because of laptop close) corresponds
    // to a user action.
    // TODO(asanka): A lid close or other power event should result in an
    // interruption that doesn't discard the partial state, unlike
     // USER_CANCELLED. (https://crbug.com/166179)
    if (net::IsCertStatusError(cert_status))
      return DOWNLOAD_INTERRUPT_REASON_SERVER_CERT_PROBLEM;
    else
      return DOWNLOAD_INTERRUPT_REASON_USER_CANCELED;
  } else if (abort_reason != DOWNLOAD_INTERRUPT_REASON_NONE) {
    // If a more specific interrupt reason was specified before the request
    // was explicitly cancelled, then use it.
    return abort_reason;
  }

  return ConvertNetErrorToInterruptReason(
      error_code, DOWNLOAD_INTERRUPT_FROM_NETWORK);
}

std::unique_ptr<ResourceRequest> CreateResourceRequest(
    DownloadUrlParameters* params) {
  DCHECK(params->offset() >= 0);

  std::unique_ptr<ResourceRequest> request(new ResourceRequest);
  request->method = params->method();
  request->url = params->url();
  request->request_initiator = params->initiator();
  request->do_not_prompt_for_login = params->do_not_prompt_for_login();
  request->site_for_cookies = params->url();
  request->referrer = params->referrer().url;
  request->referrer_policy = params->referrer().policy;
  request->download_to_file = true;

  bool has_upload_data = false;
  if (!params->post_body().empty()) {
    request->request_body = ResourceRequestBody::CreateFromBytes(
        params->post_body().data(), params->post_body().size());
    has_upload_data = true;
  }

  if (params->post_id() >= 0) {
    // The POST in this case does not have an actual body, and only works
    // when retrieving data from cache. This is done because we don't want
    // to do a re-POST without user consent, and currently don't have a good
    // plan on how to display the UI for that.
    DCHECK(params->prefer_cache());
    DCHECK_EQ("POST", params->method());
    request->request_body = new ResourceRequestBody();
    request->request_body->set_identifier(params->post_id());
    has_upload_data = true;
  }

  request->load_flags = GetLoadFlags(params, has_upload_data);

  // Add additional request headers.
  std::unique_ptr<net::HttpRequestHeaders> headers =
      GetAdditionalRequestHeaders(params);
  if (!headers->IsEmpty())
    request->headers = headers->ToString();

  return request;
}

std::unique_ptr<net::URLRequest>
CreateURLRequestOnIOThread(DownloadUrlParameters* params) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(params->offset() >= 0);

  // ResourceDispatcherHost{Base} is-not-a URLRequest::Delegate, and
  // DownloadUrlParameters can-not include resource_dispatcher_host_impl.h, so
  // we must down cast. RDHI is the only subclass of RDH as of 2012 May 4.
  std::unique_ptr<net::URLRequest> request(
      params->url_request_context_getter()
          ->GetURLRequestContext()
          ->CreateRequest(params->url(), net::DEFAULT_PRIORITY, nullptr,
                          params->GetNetworkTrafficAnnotation()));
  request->set_method(params->method());

  if (!params->post_body().empty()) {
    const std::string& body = params->post_body();
    std::unique_ptr<net::UploadElementReader> reader(
        net::UploadOwnedBytesElementReader::CreateWithString(body));
    request->set_upload(
        net::ElementsUploadDataStream::CreateWithReader(std::move(reader), 0));
  }

  if (params->post_id() >= 0) {
    // The POST in this case does not have an actual body, and only works
    // when retrieving data from cache. This is done because we don't want
    // to do a re-POST without user consent, and currently don't have a good
    // plan on how to display the UI for that.
    DCHECK(params->prefer_cache());
    DCHECK_EQ("POST", params->method());
    std::vector<std::unique_ptr<net::UploadElementReader>> element_readers;
    request->set_upload(base::MakeUnique<net::ElementsUploadDataStream>(
        std::move(element_readers), params->post_id()));
  }

  request->SetLoadFlags(GetLoadFlags(params, request->get_upload()));

  // Add additional request headers.
  std::unique_ptr<net::HttpRequestHeaders> headers =
      GetAdditionalRequestHeaders(params);
  if (!headers->IsEmpty())
    request->SetExtraRequestHeaders(*headers);

  // Downloads are treated as top level navigations. Hence the first-party
  // origin for cookies is always based on the target URL and is updated on
  // redirects.
  request->set_site_for_cookies(params->url());
  request->set_first_party_url_policy(
      net::URLRequest::UPDATE_FIRST_PARTY_URL_ON_REDIRECT);
  request->set_initiator(params->initiator());

  return request;
}


}  // namespace content
