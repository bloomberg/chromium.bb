// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_fraudulent_certificate_reporter.h"

#include <set>

#include "base/base64.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/time.h"
#include "chrome/browser/net/cert_logger.pb.h"
#include "net/base/load_flags.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_data_stream.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_info.h"
#include "net/url_request/url_request_context.h"

namespace chrome_browser_net {

// TODO(palmer): Switch to HTTPS when the error handling delegate is more
// sophisticated. Ultimately we plan to attempt the report on many transports.
static const char kFraudulentCertificateUploadEndpoint[] =
    "http://clients3.google.com/log_cert_error";

ChromeFraudulentCertificateReporter::ChromeFraudulentCertificateReporter(
    net::URLRequestContext* request_context)
    : request_context_(request_context),
      upload_url_(kFraudulentCertificateUploadEndpoint) {
}

ChromeFraudulentCertificateReporter::~ChromeFraudulentCertificateReporter() {
  STLDeleteElements(&inflight_requests_);
}

static std::string BuildReport(const std::string& hostname,
                               const net::SSLInfo& ssl_info) {
  CertLoggerRequest request;
  base::Time now = base::Time::Now();
  request.set_time_usec(now.ToInternalValue());
  request.set_hostname(hostname);

  std::vector<std::string> pem_encoded_chain;
  if (!ssl_info.cert->GetPEMEncodedChain(&pem_encoded_chain)) {
    LOG(ERROR) << "Could not get PEM encoded chain.";
  }
  std::string* cert_chain = request.mutable_cert_chain();
  for (size_t i = 0; i < pem_encoded_chain.size(); ++i)
    *cert_chain += pem_encoded_chain[i];

  std::string out;
  request.SerializeToString(&out);
  return out;
}

net::URLRequest* ChromeFraudulentCertificateReporter::CreateURLRequest(
      net::URLRequestContext* context) {
  net::URLRequest* request = context->CreateRequest(upload_url_, this);
  request->set_load_flags(net::LOAD_DO_NOT_SEND_COOKIES |
                          net::LOAD_DO_NOT_SAVE_COOKIES);
  return request;
}

void ChromeFraudulentCertificateReporter::SendReport(
    const std::string& hostname,
    const net::SSLInfo& ssl_info,
    bool sni_available) {
  // We do silent/automatic reporting ONLY for Google properties. For other
  // domains (when we start supporting that), we will ask for user permission.
  if (!net::TransportSecurityState::IsGooglePinnedProperty(hostname,
                                                           sni_available)) {
    return;
  }

  std::string report = BuildReport(hostname, ssl_info);

  net::URLRequest* url_request = CreateURLRequest(request_context_);
  url_request->set_method("POST");

  scoped_ptr<net::UploadElementReader> reader(
      net::UploadOwnedBytesElementReader::CreateWithString(report));
  url_request->set_upload(make_scoped_ptr(
      net::UploadDataStream::CreateWithReader(reader.Pass(), 0)));

  net::HttpRequestHeaders headers;
  headers.SetHeader(net::HttpRequestHeaders::kContentType,
                    "x-application/chrome-fraudulent-cert-report");
  url_request->SetExtraRequestHeaders(headers);

  inflight_requests_.insert(url_request);
  url_request->Start();
}

void ChromeFraudulentCertificateReporter::RequestComplete(
    net::URLRequest* request) {
  std::set<net::URLRequest*>::iterator i = inflight_requests_.find(request);
  DCHECK(i != inflight_requests_.end());
  delete *i;
  inflight_requests_.erase(i);
}

// TODO(palmer): Currently, the upload is fire-and-forget but soon we will
// try to recover by retrying, and trying different endpoints, and
// appealing to the user.
void ChromeFraudulentCertificateReporter::OnResponseStarted(
    net::URLRequest* request) {
  const net::URLRequestStatus& status(request->status());
  if (!status.is_success()) {
    LOG(WARNING) << "Certificate upload failed"
                 << " status:" << status.status()
                 << " error:" << status.error();
  } else if (request->GetResponseCode() != 200) {
    LOG(WARNING) << "Certificate upload HTTP status: "
                 << request->GetResponseCode();
  }
  RequestComplete(request);
}

void ChromeFraudulentCertificateReporter::OnReadCompleted(
    net::URLRequest* request, int bytes_read) {}

}  // namespace chrome_browser_net
