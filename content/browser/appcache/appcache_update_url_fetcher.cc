// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_update_url_fetcher.h"

#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_request_context.h"

namespace content {

namespace {

const int kMax503Retries = 3;

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("appcache_update_job", R"(
      semantics {
        sender: "HTML5 AppCache System"
        description:
          "Web pages can include a link to a manifest file which lists "
          "resources to be cached for offline access. The AppCache system"
          "retrieves those resources in the background."
        trigger:
          "User visits a web page containing a <html manifest=manifestUrl> "
          "tag, or navigates to a document retrieved from an existing appcache "
          "and some resource should be updated."
        data: "None"
        destination: WEBSITE
      }
      policy {
        cookies_allowed: true
        cookies_store: "user"
        setting:
          "Users can control this feature via the 'Cookies' setting under "
          "'Privacy, Content settings'. If cookies are disabled for a single "
          "site, appcaches are disabled for the site only. If they are totally "
          "disabled, all appcache requests will be stopped."
        chrome_policy {
            DefaultCookiesSetting {
              policy_options {mode: MANDATORY}
              DefaultCookiesSetting: 2
            }
          }
      })");

}  // namespace

// Helper class to fetch resources. Depending on the fetch type,
// can either fetch to an in-memory string or write the response
// data out to the disk cache.
URLFetcher::URLFetcher(const GURL& url,
                       FetchType fetch_type,
                       AppCacheUpdateJob* job,
                       int buffer_size)
    : url_(url),
      job_(job),
      fetch_type_(fetch_type),
      retry_503_attempts_(0),
      buffer_(new net::IOBuffer(buffer_size)),
      request_(
          job->service_->request_context()->CreateRequest(url,
                                                          net::DEFAULT_PRIORITY,
                                                          this,
                                                          kTrafficAnnotation)),
      result_(AppCacheUpdateJob::UPDATE_OK),
      redirect_response_code_(-1),
      buffer_size_(buffer_size) {}

URLFetcher::~URLFetcher() {
  // To defend against URLRequest calling delegate methods during
  // destruction, we test for a !request_ in those methods.
  std::unique_ptr<net::URLRequest> temp = std::move(request_);
}

void URLFetcher::Start() {
  request_->set_first_party_for_cookies(job_->manifest_url_);
  request_->set_initiator(url::Origin(job_->manifest_url_));
  if (fetch_type_ == MANIFEST_FETCH && job_->doing_full_update_check_)
    request_->SetLoadFlags(request_->load_flags() | net::LOAD_BYPASS_CACHE);
  else if (existing_response_headers_.get())
    AddConditionalHeaders(existing_response_headers_.get());
  request_->Start();
}

void URLFetcher::OnReceivedRedirect(net::URLRequest* request,
                                    const net::RedirectInfo& redirect_info,
                                    bool* defer_redirect) {
  if (!request_)
    return;
  DCHECK_EQ(request_.get(), request);
  // Redirect is not allowed by the update process.
  job_->MadeProgress();
  redirect_response_code_ = request->GetResponseCode();
  request->Cancel();
  result_ = AppCacheUpdateJob::REDIRECT_ERROR;
  OnResponseCompleted(net::ERR_ABORTED);
}

void URLFetcher::OnResponseStarted(net::URLRequest* request, int net_error) {
  if (!request_)
    return;
  DCHECK_EQ(request_.get(), request);
  DCHECK_NE(net::ERR_IO_PENDING, net_error);

  int response_code = -1;
  if (net_error == net::OK) {
    response_code = request->GetResponseCode();
    job_->MadeProgress();
  }

  if ((response_code / 100) != 2) {
    if (response_code > 0)
      result_ = AppCacheUpdateJob::SERVER_ERROR;
    else
      result_ = AppCacheUpdateJob::NETWORK_ERROR;
    OnResponseCompleted(net_error);
    return;
  }

  if (url_.SchemeIsCryptographic()) {
    // Do not cache content with cert errors.
    // Also, we willfully violate the HTML5 spec at this point in order
    // to support the appcaching of cross-origin HTTPS resources.
    // We've opted for a milder constraint and allow caching unless
    // the resource has a "no-store" header. A spec change has been
    // requested on the whatwg list.
    // See http://code.google.com/p/chromium/issues/detail?id=69594
    // TODO(michaeln): Consider doing this for cross-origin HTTP too.
    const net::HttpNetworkSession::Params* session_params =
        request->context()->GetNetworkSessionParams();
    bool ignore_cert_errors =
        session_params && session_params->ignore_certificate_errors;
    if ((net::IsCertStatusError(request->ssl_info().cert_status) &&
         !ignore_cert_errors) ||
        (url_.GetOrigin() != job_->manifest_url_.GetOrigin() &&
         request->response_headers()->HasHeaderValue("cache-control",
                                                     "no-store"))) {
      DCHECK_EQ(-1, redirect_response_code_);
      request->Cancel();
      result_ = AppCacheUpdateJob::SECURITY_ERROR;
      OnResponseCompleted(net::ERR_ABORTED);
      return;
    }
  }

  // Write response info to storage for URL fetches. Wait for async write
  // completion before reading any response data.
  if (fetch_type_ == URL_FETCH || fetch_type_ == MASTER_ENTRY_FETCH) {
    response_writer_.reset(job_->CreateResponseWriter());
    scoped_refptr<HttpResponseInfoIOBuffer> io_buffer(
        new HttpResponseInfoIOBuffer(
            new net::HttpResponseInfo(request->response_info())));
    response_writer_->WriteInfo(
        io_buffer.get(),
        base::Bind(&URLFetcher::OnWriteComplete, base::Unretained(this)));
  } else {
    ReadResponseData();
  }
}

void URLFetcher::OnReadCompleted(net::URLRequest* request, int bytes_read) {
  if (!request_)
    return;
  DCHECK_NE(net::ERR_IO_PENDING, bytes_read);
  DCHECK_EQ(request_.get(), request);
  bool data_consumed = true;
  if (bytes_read > 0) {
    job_->MadeProgress();
    data_consumed = ConsumeResponseData(bytes_read);
    if (data_consumed) {
      while (true) {
        bytes_read = request->Read(buffer_.get(), buffer_size_);
        if (bytes_read <= 0)
          break;
        data_consumed = ConsumeResponseData(bytes_read);
        if (!data_consumed)
          break;
      }
    }
  }

  if (data_consumed && bytes_read != net::ERR_IO_PENDING) {
    DCHECK_EQ(AppCacheUpdateJob::UPDATE_OK, result_);
    OnResponseCompleted(bytes_read);
  }
}

void URLFetcher::AddConditionalHeaders(
    const net::HttpResponseHeaders* headers) {
  DCHECK(request_);
  DCHECK(headers);
  net::HttpRequestHeaders extra_headers;

  // Add If-Modified-Since header if response info has Last-Modified header.
  const std::string last_modified = "Last-Modified";
  std::string last_modified_value;
  headers->EnumerateHeader(nullptr, last_modified, &last_modified_value);
  if (!last_modified_value.empty()) {
    extra_headers.SetHeader(net::HttpRequestHeaders::kIfModifiedSince,
                            last_modified_value);
  }

  // Add If-None-Match header if response info has ETag header.
  const std::string etag = "ETag";
  std::string etag_value;
  headers->EnumerateHeader(nullptr, etag, &etag_value);
  if (!etag_value.empty()) {
    extra_headers.SetHeader(net::HttpRequestHeaders::kIfNoneMatch, etag_value);
  }
  if (!extra_headers.IsEmpty())
    request_->SetExtraRequestHeaders(extra_headers);
}

void URLFetcher::OnWriteComplete(int result) {
  if (result < 0) {
    request_->Cancel();
    result_ = AppCacheUpdateJob::DISKCACHE_ERROR;
    OnResponseCompleted(net::ERR_ABORTED);
    return;
  }
  ReadResponseData();
}

void URLFetcher::ReadResponseData() {
  AppCacheUpdateJob::InternalUpdateState state = job_->internal_state_;
  if (state == AppCacheUpdateJob::CACHE_FAILURE ||
      state == AppCacheUpdateJob::CANCELLED ||
      state == AppCacheUpdateJob::COMPLETED) {
    return;
  }
  int bytes_read = request_->Read(buffer_.get(), buffer_size_);
  if (bytes_read != net::ERR_IO_PENDING)
    OnReadCompleted(request_.get(), bytes_read);
}

// Returns false if response data is processed asynchronously, in which
// case ReadResponseData will be invoked when it is safe to continue
// reading more response data from the request.
bool URLFetcher::ConsumeResponseData(int bytes_read) {
  DCHECK_GT(bytes_read, 0);
  switch (fetch_type_) {
    case MANIFEST_FETCH:
    case MANIFEST_REFETCH:
      manifest_data_.append(buffer_->data(), bytes_read);
      break;
    case URL_FETCH:
    case MASTER_ENTRY_FETCH:
      DCHECK(response_writer_.get());
      response_writer_->WriteData(
          buffer_.get(), bytes_read,
          base::Bind(&URLFetcher::OnWriteComplete, base::Unretained(this)));
      return false;  // wait for async write completion to continue reading
    default:
      NOTREACHED();
  }
  return true;
}

void URLFetcher::OnResponseCompleted(int net_error) {
  if (net_error == net::OK)
    job_->MadeProgress();

  // Retry for 503s where retry-after is 0.
  if (net_error == net::OK && request_->GetResponseCode() == 503 &&
      MaybeRetryRequest()) {
    return;
  }

  switch (fetch_type_) {
    case MANIFEST_FETCH:
      job_->HandleManifestFetchCompleted(this, net_error);
      break;
    case URL_FETCH:
      job_->HandleUrlFetchCompleted(this, net_error);
      break;
    case MASTER_ENTRY_FETCH:
      job_->HandleMasterEntryFetchCompleted(this, net_error);
      break;
    case MANIFEST_REFETCH:
      job_->HandleManifestRefetchCompleted(this, net_error);
      break;
    default:
      NOTREACHED();
  }

  delete this;
}

bool URLFetcher::MaybeRetryRequest() {
  if (retry_503_attempts_ >= kMax503Retries ||
      !request_->response_headers()->HasHeaderValue("retry-after", "0")) {
    return false;
  }
  ++retry_503_attempts_;
  result_ = AppCacheUpdateJob::UPDATE_OK;
  request_ = job_->service_->request_context()->CreateRequest(
      url_, net::DEFAULT_PRIORITY, this, kTrafficAnnotation);
  Start();
  return true;
}

}  // namespace content.