// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/url_request_automation_job.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/time.h"
#include "chrome/browser/automation/automation_resource_message_filter.h"
#include "chrome/common/automation_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_request_info.h"
#include "net/base/host_port_pair.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_data_stream.h"
#include "net/base/upload_file_element_reader.h"
#include "net/cookies/cookie_monster.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/url_request/http_user_agent_settings.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"

using base::Time;
using base::TimeDelta;
using content::BrowserThread;
using content::ResourceRequestInfo;

namespace {

// The list of filtered headers that are removed from requests sent via
// StartAsync(). These must be lower case.
const char* const kFilteredHeaderStrings[] = {
  "connection",
  "cookie",
  "expect",
  "max-forwards",
  "proxy-authorization",
  "referer",
  "te",
  "upgrade",
  "via"
};

// Creates UploadData from UploadDataStream.
net::UploadData* CreateUploadData(
    const net::UploadDataStream* upload_data_stream) {
  net::UploadData* upload_data = new net::UploadData();
  const ScopedVector<net::UploadElementReader>& element_readers =
      upload_data_stream->element_readers();
  for (size_t i = 0; i < element_readers.size(); ++i) {
    const net::UploadElementReader* reader = element_readers[i];
    if (reader->AsBytesReader()) {
      const net::UploadBytesElementReader* bytes_reader =
          reader->AsBytesReader();
      upload_data->AppendBytes(bytes_reader->bytes(), bytes_reader->length());
    } else if (reader->AsFileReader()) {
      const net::UploadFileElementReader* file_reader =
          reader->AsFileReader();
      upload_data->AppendFileRange(file_reader->path(),
                                   file_reader->range_offset(),
                                   file_reader->range_length(),
                                   file_reader->expected_modification_time());
    } else {
      NOTIMPLEMENTED();
    }
  }
  upload_data->set_identifier(upload_data_stream->identifier());
  upload_data->set_is_chunked(upload_data_stream->is_chunked());
  upload_data->set_last_chunk_appended(
      upload_data_stream->last_chunk_appended());
  return upload_data;
}

}  // namespace

int URLRequestAutomationJob::instance_count_ = 0;
bool URLRequestAutomationJob::is_protocol_factory_registered_ = false;

net::URLRequest::ProtocolFactory* URLRequestAutomationJob::old_http_factory_
    = NULL;
net::URLRequest::ProtocolFactory* URLRequestAutomationJob::old_https_factory_
    = NULL;

URLRequestAutomationJob::URLRequestAutomationJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    const net::HttpUserAgentSettings* http_user_agent_settings,
    int tab,
    int request_id,
    AutomationResourceMessageFilter* filter,
    bool is_pending)
    : net::URLRequestJob(request, network_delegate),
      http_user_agent_settings_(http_user_agent_settings),
      id_(0),
      tab_(tab),
      message_filter_(filter),
      pending_buf_size_(0),
      redirect_status_(0),
      request_id_(request_id),
      is_pending_(is_pending),
      upload_size_(0),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
  DVLOG(1) << "URLRequestAutomationJob create. Count: " << ++instance_count_;
  DCHECK(message_filter_ != NULL);

  if (message_filter_) {
    id_ = message_filter_->NewAutomationRequestId();
    DCHECK_NE(id_, 0);
  }
}

URLRequestAutomationJob::~URLRequestAutomationJob() {
  DVLOG(1) << "URLRequestAutomationJob delete. Count: " << --instance_count_;
  Cleanup();
}

void URLRequestAutomationJob::EnsureProtocolFactoryRegistered() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (!is_protocol_factory_registered_) {
    old_http_factory_ =
        net::URLRequest::Deprecated::RegisterProtocolFactory(
            "http", &URLRequestAutomationJob::Factory);
    old_https_factory_ =
        net::URLRequest::Deprecated::RegisterProtocolFactory(
            "https", &URLRequestAutomationJob::Factory);
    is_protocol_factory_registered_ = true;
  }
}

net::URLRequestJob* URLRequestAutomationJob::Factory(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    const std::string& scheme) {
  bool scheme_is_http = request->url().SchemeIs("http");
  bool scheme_is_https = request->url().SchemeIs("https");

  // Returning null here just means that the built-in handler will be used.
  if (scheme_is_http || scheme_is_https) {
    const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request);
    if (info) {
      int child_id = info->GetChildID();
      int route_id = info->GetRouteID();
      AutomationResourceMessageFilter::AutomationDetails details;
      if (AutomationResourceMessageFilter::LookupRegisteredRenderView(
              child_id, route_id, &details)) {
        URLRequestAutomationJob* job = new URLRequestAutomationJob(
            request, network_delegate,
            request->context()->http_user_agent_settings(),
            details.tab_handle, info->GetRequestID(), details.filter,
            details.is_pending_render_view);
        return job;
      }
    }

    if (scheme_is_http && old_http_factory_)
      return old_http_factory_(request, network_delegate, scheme);
    else if (scheme_is_https && old_https_factory_)
      return old_https_factory_(request, network_delegate, scheme);
  }
  return NULL;
}

// net::URLRequestJob Implementation.
void URLRequestAutomationJob::Start() {
  if (!is_pending()) {
    // Start reading asynchronously so that all error reporting and data
    // callbacks happen as they would for network requests.
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&URLRequestAutomationJob::StartAsync,
                   weak_factory_.GetWeakPtr()));
  } else {
    // If this is a pending job, then register it immediately with the message
    // filter so it can be serviced later when we receive a request from the
    // external host to connect to the corresponding external tab.
    message_filter_->RegisterRequest(this);
  }
}

void URLRequestAutomationJob::Kill() {
  if (message_filter_.get()) {
    if (!is_pending()) {
      message_filter_->Send(new AutomationMsg_RequestEnd(tab_, id_,
          net::URLRequestStatus(net::URLRequestStatus::CANCELED,
                                net::ERR_ABORTED)));
    }
  }
  DisconnectFromMessageFilter();
  net::URLRequestJob::Kill();
}

bool URLRequestAutomationJob::ReadRawData(
    net::IOBuffer* buf, int buf_size, int* bytes_read) {
  DVLOG(1) << "URLRequestAutomationJob: " << request_->url().spec()
           << " - read pending: " << buf_size;

  // We should not receive a read request for a pending job.
  DCHECK(!is_pending());

  pending_buf_ = buf;
  pending_buf_size_ = buf_size;

  if (message_filter_) {
    message_filter_->Send(new AutomationMsg_RequestRead(tab_, id_, buf_size));
    SetStatus(net::URLRequestStatus(net::URLRequestStatus::IO_PENDING, 0));
  } else {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&URLRequestAutomationJob::NotifyJobCompletionTask,
                   weak_factory_.GetWeakPtr()));
  }
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
    info->ssl_info.security_bits = -1;
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
  if (!net::HttpResponseHeaders::IsRedirectResponseCode(redirect_status_))
    return false;

  *http_status_code = redirect_status_;
  *location = GURL(redirect_url_);
  return true;
}

net::UploadProgress URLRequestAutomationJob::GetUploadProgress() const {
  uint64 progress = 0;
  if (request_ && request_->status().is_success()) {
    // We don't support incremental progress notifications in ChromeFrame. When
    // we receive a response for the POST request from Chromeframe, it means
    // that the upload is fully complete.
    progress = upload_size_;
  }
  return net::UploadProgress(progress, upload_size_);
}

net::HostPortPair URLRequestAutomationJob::GetSocketAddress() const {
  return socket_address_;
}

bool URLRequestAutomationJob::MayFilterMessage(const IPC::Message& message,
                                               int* request_id) {
  switch (message.type()) {
    case AutomationMsg_RequestStarted::ID:
    case AutomationMsg_RequestData::ID:
    case AutomationMsg_RequestEnd::ID: {
      PickleIterator iter(message);
      if (message.ReadInt(&iter, request_id))
        return true;
      break;
    }
  }

  return false;
}

void URLRequestAutomationJob::OnMessage(const IPC::Message& message) {
  if (!request_) {
    NOTREACHED() << __FUNCTION__
                 << ": Unexpected request received for job:"
                 << id();
    return;
  }

  bool deserialize_success = false;
  IPC_BEGIN_MESSAGE_MAP_EX(URLRequestAutomationJob,
                           message,
                           deserialize_success)
    IPC_MESSAGE_HANDLER(AutomationMsg_RequestStarted, OnRequestStarted)
    IPC_MESSAGE_HANDLER(AutomationMsg_RequestData, OnDataAvailable)
    IPC_MESSAGE_HANDLER(AutomationMsg_RequestEnd, OnRequestEnd)
  IPC_END_MESSAGE_MAP_EX()

  if (!deserialize_success) {
    LOG(ERROR) << "Failed to deserialize IPC message.";
  }
}

void URLRequestAutomationJob::OnRequestStarted(
    int id, const AutomationURLResponse& response) {
  DVLOG(1) << "URLRequestAutomationJob: " << request_->url().spec()
           << " - response started.";
  set_expected_content_size(response.content_length);
  mime_type_ = response.mime_type;

  redirect_url_ = response.redirect_url;
  redirect_status_ = response.redirect_status;
  DCHECK(redirect_status_ == 0 || redirect_status_ == 200 ||
         (redirect_status_ >= 300 && redirect_status_ < 400));

  if (!response.headers.empty()) {
    headers_ = new net::HttpResponseHeaders(
        net::HttpUtil::AssembleRawHeaders(response.headers.data(),
                                          response.headers.size()));
  }
  socket_address_ = response.socket_address;
  upload_size_ = response.upload_size;
  NotifyHeadersComplete();
}

void URLRequestAutomationJob::OnDataAvailable(
    int id, const std::string& bytes) {
  DVLOG(1) << "URLRequestAutomationJob: " << request_->url().spec()
           << " - data available, Size: " << bytes.size();
  DCHECK(!bytes.empty());

  // The request completed, and we have all the data.
  // Clear any IO pending status.
  SetStatus(net::URLRequestStatus());

  if (pending_buf_ && pending_buf_->data()) {
    DCHECK_GE(pending_buf_size_, bytes.size());
    const int bytes_to_copy = std::min(bytes.size(), pending_buf_size_);
    memcpy(pending_buf_->data(), &bytes[0], bytes_to_copy);

    pending_buf_ = NULL;
    pending_buf_size_ = 0;

    NotifyReadComplete(bytes_to_copy);
  } else {
    NOTREACHED() << "Received unexpected data of length:" << bytes.size();
  }
}

void URLRequestAutomationJob::OnRequestEnd(
    int id, const net::URLRequestStatus& status) {
#ifndef NDEBUG
  std::string url;
  if (request_)
    url = request_->url().spec();
  DVLOG(1) << "URLRequestAutomationJob: " << url << " - request end. Status: "
           << status.status();
#endif

  // TODO(tommi): When we hit certificate errors, notify the delegate via
  // OnSSLCertificateError().  Right now we don't have the certificate
  // so we don't.  We could possibly call OnSSLCertificateError with a NULL
  // certificate, but I'm not sure if all implementations expect it.
  // if (status.status() == net::URLRequestStatus::FAILED &&
  //    net::IsCertificateError(status.error()) && request_->delegate()) {
  //  request_->delegate()->OnSSLCertificateError(request_, status.error());
  // }

  DisconnectFromMessageFilter();
  // NotifyDone may have been called on the job if the original request was
  // redirected.
  if (!is_done()) {
    // We can complete the job if we have a valid response or a pending read.
    // An end request can be received in the following cases
    // 1. We failed to connect to the server, in which case we did not receive
    //    a valid response.
    // 2. In response to a read request.
    if (!has_response_started()) {
      NotifyStartError(status);
    } else if (pending_buf_) {
      pending_buf_ = NULL;
      pending_buf_size_ = 0;
      NotifyDone(status);
      NotifyReadComplete(0);
    } else {
      // Wait for the http stack to issue a Read request where we will notify
      // that the job has completed.
      request_status_ = status;
    }
  }
  // Note
  // The job could have been destroyed above. Please don't attempt to access
  // member variables here.
}

void URLRequestAutomationJob::Cleanup() {
  headers_ = NULL;
  mime_type_.erase();

  id_ = 0;
  tab_ = 0;

  DCHECK(!message_filter_);
  DisconnectFromMessageFilter();

  pending_buf_ = NULL;
  pending_buf_size_ = 0;
}

void URLRequestAutomationJob::StartAsync() {
  DVLOG(1) << "URLRequestAutomationJob: start request: "
           << (request_ ? request_->url().spec() : "NULL request");

  // If the job is cancelled before we got a chance to start it
  // we have nothing much to do here.
  if (is_done())
    return;

  // We should not receive a Start request for a pending job.
  DCHECK(!is_pending());

  if (!request_) {
    NotifyStartError(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                           net::ERR_FAILED));
    return;
  }

  // Register this request with automation message filter.
  message_filter_->RegisterRequest(this);

  // Strip unwanted headers.
  net::HttpRequestHeaders new_request_headers;
  new_request_headers.MergeFrom(request_->extra_request_headers());
  for (size_t i = 0; i < arraysize(kFilteredHeaderStrings); ++i)
    new_request_headers.RemoveHeader(kFilteredHeaderStrings[i]);

  // Only add default Accept-Language and Accept-Charset if the request
  // didn't have them specified.
  if (!new_request_headers.HasHeader(
      net::HttpRequestHeaders::kAcceptLanguage) &&
      http_user_agent_settings_) {
    std::string accept_language =
        http_user_agent_settings_->GetAcceptLanguage();
    if (!accept_language.empty()) {
      new_request_headers.SetHeader(net::HttpRequestHeaders::kAcceptLanguage,
                                    accept_language);
    }
  }
  if (!new_request_headers.HasHeader(
      net::HttpRequestHeaders::kAcceptCharset) &&
      http_user_agent_settings_) {
    std::string accept_charset = http_user_agent_settings_->GetAcceptCharset();
    if (!accept_charset.empty()) {
      new_request_headers.SetHeader(net::HttpRequestHeaders::kAcceptCharset,
                                    accept_charset);
    }
  }

  // Ensure that we do not send username and password fields in the referrer.
  GURL referrer(request_->GetSanitizedReferrer());

  // The referrer header must be suppressed if the preceding URL was
  // a secure one and the new one is not.
  if (referrer.SchemeIsSecure() && !request_->url().SchemeIsSecure()) {
    DVLOG(1) << "Suppressing referrer header since going from secure to "
                "non-secure";
    referrer = GURL();
  }

  // Get the resource type (main_frame/script/image/stylesheet etc.
  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request_);
  ResourceType::Type resource_type = ResourceType::MAIN_FRAME;
  if (info) {
    resource_type = info->GetResourceType();
  }

  // Construct UploadData from UploadDataStream.
  scoped_refptr<net::UploadData> upload_data;
  if (request_->get_upload())
    upload_data = CreateUploadData(request_->get_upload());

  // Ask automation to start this request.
  AutomationURLRequest automation_request;
  automation_request.url = request_->url().spec();
  automation_request.method = request_->method();
  automation_request.referrer = referrer.spec();
  automation_request.extra_request_headers = new_request_headers.ToString();
  automation_request.upload_data = upload_data;
  automation_request.resource_type = resource_type;
  automation_request.load_flags = request_->load_flags();

  DCHECK(message_filter_);
  message_filter_->Send(new AutomationMsg_RequestStart(
      tab_, id_, automation_request));
}

void URLRequestAutomationJob::DisconnectFromMessageFilter() {
  if (message_filter_) {
    message_filter_->UnRegisterRequest(this);
    message_filter_ = NULL;
  }
}

void URLRequestAutomationJob::StartPendingJob(
    int new_tab_handle,
    AutomationResourceMessageFilter* new_filter) {
  DCHECK(new_filter != NULL);
  tab_ = new_tab_handle;
  message_filter_ = new_filter;
  is_pending_ = false;
  Start();
}

void URLRequestAutomationJob::NotifyJobCompletionTask() {
  if (!is_done()) {
    NotifyDone(request_status_);
  }
  // Reset any pending reads.
  if (pending_buf_) {
    pending_buf_ = NULL;
    pending_buf_size_ = 0;
    NotifyReadComplete(0);
  }
}
