// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_utils.h"

#include "base/format_macros.h"
#include "base/memory/ptr_util.h"
#include "base/process/process_handle.h"
#include "base/strings/stringprintf.h"
#include "base/task_scheduler/post_task.h"
#include "components/download/downloader/in_progress/download_entry.h"
#include "components/download/downloader/in_progress/in_progress_cache.h"
#include "components/download/public/common/download_create_info.h"
#include "components/download/public/common/download_interrupt_reasons_utils.h"
#include "components/download/public/common/download_save_info.h"
#include "components/download/public/common/download_url_parameters.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/loader/upload_data_stream_builder.h"
#include "content/browser/resource_context_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "net/base/elements_upload_data_stream.h"
#include "net/base/load_flags.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/url_request_context.h"
#include "services/network/public/cpp/resource_request.h"

namespace content {

namespace {

void AppendExtraHeaders(net::HttpRequestHeaders* headers,
                        download::DownloadUrlParameters* params) {
  for (const auto& header : params->request_headers())
    headers->SetHeaderIfMissing(header.first, header.second);
}

int GetLoadFlags(download::DownloadUrlParameters* params,
                 bool has_upload_data) {
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
    download::DownloadUrlParameters* params) {
  auto headers = std::make_unique<net::HttpRequestHeaders>();
  if (params->offset() == 0 &&
      params->length() == download::DownloadSaveInfo::kLengthFullContent) {
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
      (params->length() == download::DownloadSaveInfo::kLengthFullContent)
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

storage::BlobStorageContext* BlobStorageContextGetter(
    ResourceContext* resource_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ChromeBlobStorageContext* blob_context =
      GetChromeBlobStorageContextForResourceContext(resource_context);
  return blob_context->context();
}

std::unique_ptr<network::ResourceRequest> CreateResourceRequest(
    download::DownloadUrlParameters* params) {
  DCHECK(params->offset() >= 0);

  std::unique_ptr<network::ResourceRequest> request(
      new network::ResourceRequest);
  request->method = params->method();
  request->url = params->url();
  request->request_initiator = params->initiator();
  request->do_not_prompt_for_login = params->do_not_prompt_for_login();
  request->site_for_cookies = params->url();
  request->referrer = params->referrer();
  request->referrer_policy = params->referrer_policy();
  request->allow_download = true;
  request->is_main_frame = true;

  if (params->render_process_host_id() >= 0)
    request->render_frame_id = params->render_frame_host_routing_id();

  bool has_upload_data = false;
  if (params->post_body()) {
    request->request_body = params->post_body();
    has_upload_data = true;
  }

  if (params->post_id() >= 0) {
    // The POST in this case does not have an actual body, and only works
    // when retrieving data from cache. This is done because we don't want
    // to do a re-POST without user consent, and currently don't have a good
    // plan on how to display the UI for that.
    DCHECK(params->prefer_cache());
    DCHECK_EQ("POST", params->method());
    request->request_body = new network::ResourceRequestBody();
    request->request_body->set_identifier(params->post_id());
    has_upload_data = true;
  }

  request->load_flags = GetLoadFlags(params, has_upload_data);

  // Add additional request headers.
  std::unique_ptr<net::HttpRequestHeaders> headers =
      GetAdditionalRequestHeaders(params);
  request->headers.Swap(headers.get());

  return request;
}

std::unique_ptr<net::URLRequest> CreateURLRequestOnIOThread(
    download::DownloadUrlParameters* params) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(params->offset() >= 0);

  // ResourceDispatcherHost{Base} is-not-a URLRequest::Delegate, and
  // download::DownloadUrlParameters can-not include
  // resource_dispatcher_host_impl.h, so we must down cast. RDHI is the only
  // subclass of RDH as of 2012 May 4.
  std::unique_ptr<net::URLRequest> request(
      params->url_request_context_getter()
          ->GetURLRequestContext()
          ->CreateRequest(params->url(), net::DEFAULT_PRIORITY, nullptr,
                          params->GetNetworkTrafficAnnotation()));
  request->set_method(params->method());

  if (params->post_body()) {
    storage::BlobStorageContext* blob_context =
        params->get_blob_storage_context_getter().Run();
    scoped_refptr<base::SingleThreadTaskRunner> task_runner =
        base::CreateSingleThreadTaskRunnerWithTraits(
            {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
    std::unique_ptr<net::UploadDataStream> upload_data_stream =
        UploadDataStreamBuilder::Build(params->post_body().get(), blob_context,
                                       nullptr /*FileSystemContext*/,
                                       task_runner.get());
    request->set_upload(std::move(upload_data_stream));
  }

  if (params->post_id() >= 0) {
    // The POST in this case does not have an actual body, and only works
    // when retrieving data from cache. This is done because we don't want
    // to do a re-POST without user consent, and currently don't have a good
    // plan on how to display the UI for that.
    DCHECK(params->prefer_cache());
    DCHECK_EQ("POST", params->method());
    std::vector<std::unique_ptr<net::UploadElementReader>> element_readers;
    request->set_upload(std::make_unique<net::ElementsUploadDataStream>(
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

base::Optional<download::DownloadEntry> GetInProgressEntry(
    const std::string& guid,
    BrowserContext* browser_context) {
  base::Optional<download::DownloadEntry> entry;
  if (!browser_context || guid.empty())
    return entry;

  auto* manager_delegate = browser_context->GetDownloadManagerDelegate();
  if (manager_delegate) {
    download::InProgressCache* in_progress_cache =
        manager_delegate->GetInProgressCache();
    if (in_progress_cache)
      entry = in_progress_cache->RetrieveEntry(guid);
  }
  return entry;
}

}  // namespace content
