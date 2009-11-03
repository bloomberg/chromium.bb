// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/url_request_automation_job.h"

#include "base/message_loop.h"
#include "base/time.h"
#include "chrome/browser/automation/automation_resource_message_filter.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host_request_info.h"
#include "chrome/test/automation/automation_messages.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"

using base::Time;
using base::TimeDelta;

// The list of filtered headers that are removed from requests sent via
// StartAsync(). These must be lower case.
static const char* kFilteredHeaderStrings[] = {
  "accept",
  "authorization",
  "cache-control",
  "connection",
  "cookie",
  "expect",
  "if-match",
  "if-modified-since",
  "if-none-match",
  "if-range",
  "if-unmodified-since",
  "max-forwards",
  "proxy-authorization",
  "te",
  "upgrade",
  "via"
};

// This class manages the interception of network requests for automation.
// It looks at the request, and creates an intercept job if it indicates
// that it should use automation channel.
// NOTE: All methods must be called on the IO thread.
class AutomationRequestInterceptor : public URLRequest::Interceptor {
 public:
  AutomationRequestInterceptor() {
    URLRequest::RegisterRequestInterceptor(this);
  }

  virtual ~AutomationRequestInterceptor() {
    URLRequest::UnregisterRequestInterceptor(this);
  }

  // URLRequest::Interceptor
  virtual URLRequestJob* MaybeIntercept(URLRequest* request);

 private:
  DISALLOW_COPY_AND_ASSIGN(AutomationRequestInterceptor);
};

URLRequestJob* AutomationRequestInterceptor::MaybeIntercept(
    URLRequest* request) {
  if (request->url().SchemeIs("http") || request->url().SchemeIs("https")) {
    ResourceDispatcherHostRequestInfo* request_info =
        ResourceDispatcherHost::InfoForRequest(request);
    if (request_info) {
      AutomationResourceMessageFilter::AutomationDetails details;
      if (AutomationResourceMessageFilter::LookupRegisteredRenderView(
              request_info->child_id(), request_info->route_id(), &details)) {
        URLRequestAutomationJob* job = new URLRequestAutomationJob(request,
            details.tab_handle, details.filter);
        return job;
      }
    }
  }

  return NULL;
}

static URLRequest::Interceptor* GetAutomationRequestInterceptor() {
  return Singleton<AutomationRequestInterceptor>::get();
}

int URLRequestAutomationJob::instance_count_ = 0;

URLRequestAutomationJob::URLRequestAutomationJob(
    URLRequest* request, int tab, AutomationResourceMessageFilter* filter)
    : URLRequestJob(request), id_(0), tab_(tab), message_filter_(filter),
      pending_buf_size_(0), redirect_status_(0) {
  DLOG(INFO) << "URLRequestAutomationJob create. Count: " << ++instance_count_;
  if (message_filter_) {
    id_ = message_filter_->NewRequestId();
    DCHECK(id_);
  } else {
    NOTREACHED();
  }
}

URLRequestAutomationJob::~URLRequestAutomationJob() {
  DLOG(INFO) << "URLRequestAutomationJob delete. Count: " << --instance_count_;
  Cleanup();
}

bool URLRequestAutomationJob::InitializeInterceptor() {
  // AutomationRequestInterceptor will register itself when it
  // is first created.
  URLRequest::Interceptor* interceptor = GetAutomationRequestInterceptor();
  return (interceptor != NULL);
}

// URLRequestJob Implementation.
void URLRequestAutomationJob::Start() {
  // Start reading asynchronously so that all error reporting and data
  // callbacks happen as they would for network requests.
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &URLRequestAutomationJob::StartAsync));
}

void URLRequestAutomationJob::Kill() {
  if (message_filter_.get()) {
    message_filter_->Send(new AutomationMsg_RequestEnd(0, tab_, id_,
        URLRequestStatus(URLRequestStatus::CANCELED, net::ERR_ABORTED)));
  }
  DisconnectFromMessageFilter();
  URLRequestJob::Kill();
}

bool URLRequestAutomationJob::ReadRawData(
    net::IOBuffer* buf, int buf_size, int* bytes_read) {
  DLOG(INFO) << "URLRequestAutomationJob: " <<
      request_->url().spec() << " - read pending: " << buf_size;
  pending_buf_ = buf;
  pending_buf_size_ = buf_size;

  message_filter_->Send(new AutomationMsg_RequestRead(0, tab_, id_,
      buf_size));
  SetStatus(URLRequestStatus(URLRequestStatus::IO_PENDING, 0));
  return false;
}

bool URLRequestAutomationJob::GetMimeType(std::string* mime_type) const {
  if (!mime_type_.empty()) {
    *mime_type = mime_type_;
  } else if (headers_) {
    headers_->GetMimeType(mime_type);
  }

  return (!mime_type->empty());
}

bool URLRequestAutomationJob::GetCharset(std::string* charset) {
  if (headers_)
    return headers_->GetCharset(charset);
  return false;
}

void URLRequestAutomationJob::GetResponseInfo(net::HttpResponseInfo* info) {
  if (headers_)
    info->headers = headers_;
  if (request_->url().SchemeIsSecure()) {
    // Make up a fake certificate for this response since we don't have
    // access to the real SSL info.
    const char* kCertIssuer = "Chrome Internal";
    const int kLifetimeDays = 100;

    info->ssl_info.cert =
        new net::X509Certificate(request_->url().GetWithEmptyPath().spec(),
                                 kCertIssuer,
                                 Time::Now(),
                                 Time::Now() +
                                     TimeDelta::FromDays(kLifetimeDays));
    info->ssl_info.cert_status = 0;
    info->ssl_info.security_bits = 0;
  }
}

int URLRequestAutomationJob::GetResponseCode() const {
  if (headers_)
    return headers_->response_code();

  static const int kDefaultResponseCode = 200;
  return kDefaultResponseCode;
}

bool URLRequestAutomationJob::IsRedirectResponse(
    GURL* location, int* http_status_code) {
  static const int kDefaultHttpRedirectResponseCode = 301;

  if (!redirect_url_.empty()) {
    DLOG_IF(ERROR, redirect_status_ == 0) << "Missing redirect status?";
    *http_status_code = redirect_status_ ? redirect_status_ :
                                           kDefaultHttpRedirectResponseCode;
    *location = GURL(redirect_url_);
    return true;
  } else {
    DCHECK(redirect_status_ == 0)
        << "Unexpectedly have redirect status but no URL";
  }

  return false;
}

int URLRequestAutomationJob::MayFilterMessage(const IPC::Message& message) {
  switch (message.type()) {
    case AutomationMsg_RequestStarted::ID:
    case AutomationMsg_RequestData::ID:
    case AutomationMsg_RequestEnd::ID: {
      void* iter = NULL;
      int tab = 0;
      int id = 0;
      if (message.ReadInt(&iter, &tab) && message.ReadInt(&iter, &id)) {
        DCHECK(id);
        return id;
      }
      break;
    }
  }

  return 0;
}

void URLRequestAutomationJob::OnMessage(const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(URLRequestAutomationJob, message)
    IPC_MESSAGE_HANDLER(AutomationMsg_RequestStarted, OnRequestStarted)
    IPC_MESSAGE_HANDLER(AutomationMsg_RequestData, OnDataAvailable)
    IPC_MESSAGE_HANDLER(AutomationMsg_RequestEnd, OnRequestEnd)
  IPC_END_MESSAGE_MAP()
}

void URLRequestAutomationJob::OnRequestStarted(
    int tab, int id, const IPC::AutomationURLResponse& response) {
  DLOG(INFO) << "URLRequestAutomationJob: " <<
      request_->url().spec() << " - response started.";
  set_expected_content_size(response.content_length);
  mime_type_ = response.mime_type;

  redirect_url_ = response.redirect_url;
  redirect_status_ = response.redirect_status;
  DCHECK(redirect_status_ == 0 || redirect_status_ == 200 ||
         (redirect_status_ >= 300 && redirect_status_ < 400));

  GURL url_for_cookies =
      GURL(redirect_url_.empty() ? request_->url().spec().c_str() :
          redirect_url_.c_str());

  URLRequestContext* ctx = request_->context();

  if (!response.headers.empty()) {
    headers_ = new net::HttpResponseHeaders(
        net::HttpUtil::AssembleRawHeaders(response.headers.data(),
                                          response.headers.size()));
    // Parse and set HTTP cookies.
    const std::string name = "Set-Cookie";
    std::string value;
    std::vector<std::string> response_cookies;

    void* iter = NULL;
    while (headers_->EnumerateHeader(&iter, name, &value)) {
      if (request_->context()->InterceptCookie(request_, &value))
        response_cookies.push_back(value);
    }

    if (response_cookies.size()) {
      if (ctx && ctx->cookie_store() &&
          ctx->cookie_policy()->CanSetCookie(
              url_for_cookies, request_->first_party_for_cookies())) {
        net::CookieOptions options;
        options.set_include_httponly();
        ctx->cookie_store()->SetCookiesWithOptions(url_for_cookies,
                                                   response_cookies,
                                                   options);
      }
    }
  }

  if (ctx && ctx->cookie_store() && !response.persistent_cookies.empty() &&
      ctx->cookie_policy()->CanSetCookie(
          url_for_cookies, request_->first_party_for_cookies())) {
    StringTokenizer cookie_parser(response.persistent_cookies, ";");

    while (cookie_parser.GetNext()) {
      net::CookieOptions options;
      ctx->cookie_store()->SetCookieWithOptions(url_for_cookies,
                                                cookie_parser.token(),
                                                options);
    }
  }

  NotifyHeadersComplete();
}

void URLRequestAutomationJob::OnDataAvailable(
    int tab, int id, const std::string& bytes) {
  DLOG(INFO) << "URLRequestAutomationJob: " <<
      request_->url().spec() << " - data available, Size: " << bytes.size();
  DCHECK(!bytes.empty());

  // The request completed, and we have all the data.
  // Clear any IO pending status.
  SetStatus(URLRequestStatus());

  if (pending_buf_ && pending_buf_->data()) {
    DCHECK_GE(pending_buf_size_, bytes.size());
    const int bytes_to_copy = std::min(bytes.size(), pending_buf_size_);
    memcpy(pending_buf_->data(), &bytes[0], bytes_to_copy);

    pending_buf_ = NULL;
    pending_buf_size_ = 0;

    NotifyReadComplete(bytes_to_copy);
  }
}

void URLRequestAutomationJob::OnRequestEnd(
    int tab, int id, const URLRequestStatus& status) {
#ifndef NDEBUG
  std::string url;
  if (request_)
    url = request_->url().spec();
  DLOG(INFO) << "URLRequestAutomationJob: "
      << url << " - request end. Status: " << status.status();
#endif

  // TODO(tommi): When we hit certificate errors, notify the delegate via
  // OnSSLCertificateError().  Right now we don't have the certificate
  // so we don't.  We could possibly call OnSSLCertificateError with a NULL
  // certificate, but I'm not sure if all implementations expect it.
  // if (status.status() == URLRequestStatus::FAILED &&
  //    net::IsCertificateError(status.os_error()) && request_->delegate()) {
  //  request_->delegate()->OnSSLCertificateError(request_, status.os_error());
  // }

  DisconnectFromMessageFilter();
  // NotifyDone may have been called on the job if the original request was
  // redirected.
  if (!is_done())
    NotifyDone(status);

  // Reset any pending reads.
  if (pending_buf_) {
    pending_buf_ = NULL;
    pending_buf_size_ = 0;
    NotifyReadComplete(0);
  }
}

void URLRequestAutomationJob::Cleanup() {
  headers_ = NULL;
  mime_type_.erase();

  id_ = 0;
  tab_ = 0;

  DCHECK(message_filter_ == NULL);
  DisconnectFromMessageFilter();

  pending_buf_ = NULL;
  pending_buf_size_ = 0;
}

void URLRequestAutomationJob::StartAsync() {
  DLOG(INFO) << "URLRequestAutomationJob: start request: " <<
      (request_ ? request_->url().spec() : "NULL request");

  // If the job is cancelled before we got a chance to start it
  // we have nothing much to do here.
  if (is_done())
    return;

  if (!request_) {
    NotifyStartError(URLRequestStatus(URLRequestStatus::FAILED,
        net::ERR_FAILED));
    return;
  }

  // Register this request with automation message filter.
  message_filter_->RegisterRequest(this);

  // Strip unwanted headers.
  std::string new_request_headers(
      net::HttpUtil::StripHeaders(request_->extra_request_headers(),
                                  kFilteredHeaderStrings,
                                  arraysize(kFilteredHeaderStrings)));

  if (request_->context()) {
    // Only add default Accept-Language and Accept-Charset if the request
    // didn't have them specified.
    net::HttpUtil::AppendHeaderIfMissing(
        "Accept-Language", request_->context()->accept_language(),
        &new_request_headers);
    net::HttpUtil::AppendHeaderIfMissing(
        "Accept-Charset", request_->context()->accept_charset(),
        &new_request_headers);
  }

  // Ensure that we do not send username and password fields in the referrer.
  GURL referrer(request_->GetSanitizedReferrer());

  // The referrer header must be suppressed if the preceding URL was
  // a secure one and the new one is not.
  if (referrer.SchemeIsSecure() && !request_->url().SchemeIsSecure()) {
    DLOG(INFO) <<
        "Suppressing referrer header since going from secure to non-secure";
    referrer = GURL();
  }

  // Ask automation to start this request.
  IPC::AutomationURLRequest automation_request = {
    request_->url().spec(),
    request_->method(),
    referrer.spec(),
    new_request_headers,
    request_->get_upload()
  };

  DCHECK(message_filter_);
  message_filter_->Send(new AutomationMsg_RequestStart(0, tab_, id_,
      automation_request));
}

void URLRequestAutomationJob::DisconnectFromMessageFilter() {
  if (message_filter_) {
    message_filter_->UnRegisterRequest(this);
    message_filter_ = NULL;
  }
}
