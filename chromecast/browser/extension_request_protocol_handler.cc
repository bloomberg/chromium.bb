// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/extension_request_protocol_handler.h"

#include "base/bind.h"
#include "chromecast/common/cast_redirect_manifest_handler.h"
#include "content/common/net/url_request_user_data.h"
#include "extensions/browser/extension_protocols.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/info_map.h"
#include "extensions/common/extension.h"
#include "net/base/ip_endpoint.h"
#include "net/base/upload_data_stream.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_job_manager.h"

namespace chromecast {

namespace {

class UploadDataStreamRedirect : public net::UploadDataStream {
 public:
  explicit UploadDataStreamRedirect(net::UploadDataStream* parent);
  ~UploadDataStreamRedirect() override;

 private:
  // net::UploadDataStream implementation:
  int InitInternal(const net::NetLogWithSource& net_log) override;
  int ReadInternal(net::IOBuffer* buf, int buf_len) override;
  void ResetInternal() override;

  void PostInit();
  void PostRead();

  net::UploadDataStream* stream_;

  DISALLOW_COPY_AND_ASSIGN(UploadDataStreamRedirect);
};

UploadDataStreamRedirect::UploadDataStreamRedirect(
    net::UploadDataStream* parent)
    : net::UploadDataStream(parent->is_chunked(), 0), stream_(parent) {}

UploadDataStreamRedirect::~UploadDataStreamRedirect() {}

void UploadDataStreamRedirect::PostInit() {
  if (!is_chunked())
    SetSize(stream_->size());
}

void UploadDataStreamRedirect::PostRead() {
  if (is_chunked() && stream_->IsEOF())
    SetIsFinalChunk();
}

int UploadDataStreamRedirect::InitInternal(
    const net::NetLogWithSource& net_log) {
  int ret = stream_->Init(base::BindOnce(
                              [](UploadDataStreamRedirect* stream, int result) {
                                stream->PostInit();
                                stream->OnInitCompleted(result);
                              },
                              this),
                          net_log);
  if (ret == net::OK)
    PostInit();
  return ret;
}

int UploadDataStreamRedirect::ReadInternal(net::IOBuffer* buf, int buf_len) {
  int ret = stream_->Read(buf, buf_len,
                          base::BindOnce(
                              [](UploadDataStreamRedirect* stream, int result) {
                                stream->PostRead();
                                stream->OnReadCompleted(result);
                              },
                              this));
  if (ret != net::ERR_IO_PENDING)
    PostRead();
  return ret;
}

void UploadDataStreamRedirect::ResetInternal() {
  stream_->Reset();
}

class CastExtensionURLRequestJob : public net::URLRequestJob,
                                   public net::URLRequest::Delegate {
 public:
  CastExtensionURLRequestJob(net::URLRequest* request,
                             net::NetworkDelegate* network_delegate,
                             const GURL& redirect_url);

  ~CastExtensionURLRequestJob() override;

  // net::URLRequestJob implementation:
  void Start() override;
  void SetExtraRequestHeaders(const net::HttpRequestHeaders& headers) override;
  int ReadRawData(net::IOBuffer* buf, int buf_size) override;
  void SetRequestHeadersCallback(net::RequestHeadersCallback callback) override;
  void SetResponseHeadersCallback(
      net::ResponseHeadersCallback callback) override;
  bool GetMimeType(std::string* mime_type) const override;
  int GetResponseCode() const override;
  net::IPEndPoint GetResponseRemoteEndpoint() const override;
  void StopCaching() override;
  bool GetFullRequestHeaders(net::HttpRequestHeaders* headers) const override;
  int64_t GetTotalReceivedBytes() const override;
  int64_t GetTotalSentBytes() const override;
  net::LoadState GetLoadState() const override;
  bool GetCharset(std::string* charset) override;
  void GetResponseInfo(net::HttpResponseInfo* info) override;
  void GetLoadTimingInfo(net::LoadTimingInfo* load_timing_info) const override;
  bool GetTransactionRemoteEndpoint(net::IPEndPoint* endpoint) const override;
  void PopulateNetErrorDetails(net::NetErrorDetails* details) const override;
  bool IsRedirectResponse(GURL* location,
                          int* http_status_code,
                          bool* insecure_scheme_was_upgraded) override;
  bool CopyFragmentOnRedirect(const GURL& location) const override;
  bool IsSafeRedirect(const GURL& location) override;
  bool NeedsAuth() override;
  std::unique_ptr<net::AuthChallengeInfo> GetAuthChallengeInfo() override;
  void SetAuth(const net::AuthCredentials& credentials) override;
  void CancelAuth() override;
  void ContinueWithCertificate(
      scoped_refptr<net::X509Certificate> client_cert,
      scoped_refptr<net::SSLPrivateKey> client_private_key) override;
  void ContinueDespiteLastError() override;

  // net::URLRequest::Delegate implementation:
  void OnReceivedRedirect(net::URLRequest* request,
                          const net::RedirectInfo& redirect_info,
                          bool* defer_redirect) override;
  void OnAuthRequired(net::URLRequest* request,
                      const net::AuthChallengeInfo& auth_info) override;
  void OnCertificateRequested(
      net::URLRequest* request,
      net::SSLCertRequestInfo* cert_request_info) override;
  void OnSSLCertificateError(net::URLRequest* request,
                             int net_error,
                             const net::SSLInfo& ssl_info,
                             bool fatal) override;
  void OnResponseStarted(net::URLRequest* request, int net_error) override;
  void OnReadCompleted(net::URLRequest* request, int bytes_read) override;

 private:
  std::unique_ptr<net::URLRequest> sub_request_;
};

CastExtensionURLRequestJob::CastExtensionURLRequestJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    const GURL& redirect_url)
    : net::URLRequestJob(request, network_delegate),
      sub_request_(request->context()->CreateRequest(redirect_url,
                                                     request->priority(),
                                                     this)) {
  // Copy necessary information from the original request.
  // (|URLRequest| is not copyable.)
  sub_request_->set_method(request->method());
  sub_request_->SetExtraRequestHeaders(request->extra_request_headers());
  sub_request_->SetReferrer(request->referrer());
  sub_request_->set_referrer_policy(request->referrer_policy());
  if (request->get_upload()) {
    sub_request_->set_upload(std::make_unique<UploadDataStreamRedirect>(
        const_cast<net::UploadDataStream*>(request->get_upload())));
  }
  content::URLRequestUserData* user_data =
      static_cast<content::URLRequestUserData*>(
          request->GetUserData(content::URLRequestUserData::kUserDataKey));
  sub_request_->SetUserData(
      content::URLRequestUserData::kUserDataKey,
      std::make_unique<content::URLRequestUserData>(
          user_data->render_process_id(), user_data->render_frame_id()));
}

CastExtensionURLRequestJob::~CastExtensionURLRequestJob() {}

void CastExtensionURLRequestJob::Start() {
  sub_request_->Start();
}

bool CastExtensionURLRequestJob::IsRedirectResponse(
    GURL* location,
    int* http_status_code,
    bool* insecure_scheme_was_upgraded) {
  return false;
}

void CastExtensionURLRequestJob::SetExtraRequestHeaders(
    const net::HttpRequestHeaders& headers) {
  sub_request_->SetExtraRequestHeaders(headers);
}

int CastExtensionURLRequestJob::ReadRawData(net::IOBuffer* buf, int buf_size) {
  return sub_request_->Read(buf, buf_size);
}

void CastExtensionURLRequestJob::SetRequestHeadersCallback(
    net::RequestHeadersCallback callback) {
  sub_request_->SetRequestHeadersCallback(callback);
}

void CastExtensionURLRequestJob::SetResponseHeadersCallback(
    net::ResponseHeadersCallback callback) {
  sub_request_->SetResponseHeadersCallback(callback);
}

bool CastExtensionURLRequestJob::GetMimeType(std::string* mime_type) const {
  sub_request_->GetMimeType(mime_type);
  return true;
}

int CastExtensionURLRequestJob::GetResponseCode() const {
  return sub_request_->GetResponseCode();
}

net::IPEndPoint CastExtensionURLRequestJob::GetResponseRemoteEndpoint() const {
  return sub_request_->GetResponseRemoteEndpoint();
}

void CastExtensionURLRequestJob::StopCaching() {
  sub_request_->StopCaching();
}

bool CastExtensionURLRequestJob::GetFullRequestHeaders(
    net::HttpRequestHeaders* headers) const {
  return sub_request_->GetFullRequestHeaders(headers);
}

int64_t CastExtensionURLRequestJob::GetTotalReceivedBytes() const {
  return sub_request_->GetTotalReceivedBytes();
}

int64_t CastExtensionURLRequestJob::GetTotalSentBytes() const {
  return sub_request_->GetTotalSentBytes();
}

net::LoadState CastExtensionURLRequestJob::GetLoadState() const {
  return sub_request_->GetLoadState().state;
}

bool CastExtensionURLRequestJob::GetCharset(std::string* charset) {
  sub_request_->GetCharset(charset);
  return true;
}

void CastExtensionURLRequestJob::GetResponseInfo(net::HttpResponseInfo* info) {
  *info = sub_request_->response_info();
}

void CastExtensionURLRequestJob::GetLoadTimingInfo(
    net::LoadTimingInfo* load_timing_info) const {
  sub_request_->GetLoadTimingInfo(load_timing_info);
}

bool CastExtensionURLRequestJob::GetTransactionRemoteEndpoint(
    net::IPEndPoint* endpoint) const {
  return sub_request_->GetTransactionRemoteEndpoint(endpoint);
}

void CastExtensionURLRequestJob::PopulateNetErrorDetails(
    net::NetErrorDetails* details) const {
  sub_request_->PopulateNetErrorDetails(details);
}

bool CastExtensionURLRequestJob::CopyFragmentOnRedirect(
    const GURL& location) const {
  return false;
}

bool CastExtensionURLRequestJob::IsSafeRedirect(const GURL& location) {
  return true;
}

bool CastExtensionURLRequestJob::NeedsAuth() {
  return false;
}

std::unique_ptr<net::AuthChallengeInfo>
CastExtensionURLRequestJob::GetAuthChallengeInfo() {
  return nullptr;
}

void CastExtensionURLRequestJob::SetAuth(
    const net::AuthCredentials& credentials) {
  sub_request_->SetAuth(credentials);
}

void CastExtensionURLRequestJob::CancelAuth() {
  sub_request_->CancelAuth();
}

void CastExtensionURLRequestJob::ContinueWithCertificate(
    scoped_refptr<net::X509Certificate> client_cert,
    scoped_refptr<net::SSLPrivateKey> client_private_key) {
  sub_request_->ContinueWithCertificate(client_cert, client_private_key);
}

void CastExtensionURLRequestJob::ContinueDespiteLastError() {
  sub_request_->ContinueDespiteLastError();
}

void CastExtensionURLRequestJob::OnReceivedRedirect(
    net::URLRequest* request,
    const net::RedirectInfo& redirect_info,
    bool* defer_redirect) {
  net::URLRequest::Delegate::OnReceivedRedirect(request, redirect_info,
                                                defer_redirect);
}

void CastExtensionURLRequestJob::OnAuthRequired(
    net::URLRequest* request,
    const net::AuthChallengeInfo& auth_info) {
  net::URLRequest::Delegate::OnAuthRequired(request, auth_info);
}

void CastExtensionURLRequestJob::OnCertificateRequested(
    net::URLRequest* request,
    net::SSLCertRequestInfo* cert_request_info) {
  NotifyCertificateRequested(cert_request_info);
}

void CastExtensionURLRequestJob::OnSSLCertificateError(
    net::URLRequest* request,
    int net_error,
    const net::SSLInfo& ssl_info,
    bool fatal) {
  NotifySSLCertificateError(net_error, ssl_info, fatal);
}

void CastExtensionURLRequestJob::OnResponseStarted(net::URLRequest* request,
                                                   int net_error) {
  net::URLRequestStatus status = net::URLRequestStatus::FromError(net_error);
  if (status.status() == net::URLRequestStatus::SUCCESS)
    NotifyHeadersComplete();
  else
    NotifyStartError(status);
}

void CastExtensionURLRequestJob::OnReadCompleted(net::URLRequest* request,
                                                 int bytes_read) {
  ReadRawDataComplete(bytes_read);
}

}  // namespace

ExtensionRequestProtocolHandler::ExtensionRequestProtocolHandler(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {}

ExtensionRequestProtocolHandler::~ExtensionRequestProtocolHandler() {}

net::URLRequestJob* ExtensionRequestProtocolHandler::MaybeCreateJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) const {
  if (!info_map_) {
    // This can't be done in the constructor as extensions::ExtensionSystem::Get
    // will fail until after this is constructed.
    info_map_ = extensions::ExtensionSystem::Get(browser_context_)->info_map();
    default_handler_ = extensions::CreateExtensionProtocolHandler(
        false, const_cast<extensions::InfoMap*>(info_map_));
  }

  const extensions::Extension* extension =
      info_map_->extensions().GetByID(request->url().host());

  if (!extension) {
    LOG(ERROR) << "Can't find extension with id: " << request->url().host();
    return nullptr;
  }

  const GURL& url = request->url();
  std::string cast_url;
  // See if we are being redirected to an extension-specific URL.
  if (!CastRedirectHandler::ParseUrl(&cast_url, extension, url)) {
    // Defer to the default handler to load from disk.
    return default_handler_->MaybeCreateJob(request, network_delegate);
  }

  // The above only handles the scheme, host & path, any query or fragment needs
  // to be copied separately.
  if (url.has_query()) {
    cast_url.push_back('?');
    url.query_piece().AppendToString(&cast_url);
  }

  if (url.has_ref()) {
    cast_url.push_back('#');
    url.ref_piece().AppendToString(&cast_url);
  }

  // Force a redirect to the new URL but without changing where the webpage
  // thinks it is.
  return new CastExtensionURLRequestJob(request, network_delegate,
                                        GURL(cast_url));
}

}  // namespace chromecast
